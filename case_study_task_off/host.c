// UPMEM host driver for the VIPER case study.
// Offloads the eight Llama2 matmuls to N DPUs, measures REAL on-device
// time (input DMA, kernel launch, output DMA) and verifies the result
// against the CPU reference. Sweeps DPU counts to produce a measured
// speedup curve to overlay on the VIPER prediction.
//
// Build: dpu-pkg-config gives -I/usr/include/dpu -ldpu
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dpu.h>
#include "layer.h"

#ifndef DPU_BINARY
#define DPU_BINARY "./matmul.kernel"
#endif

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e3 + ts.tv_nsec * 1e-6;
}

static uint32_t ceil_div(uint32_t a, uint32_t b) { return (a + b - 1) / b; }
/* round cols up to a multiple of 4 so int16 tiles are 8-byte aligned */
static uint32_t round4(uint32_t x) { return (x + 3) & ~3u; }

typedef struct {
    double t_in, t_launch, t_out; /* ms, summed over 8 matmuls */
    uint32_t n_active;            /* DPUs doing work (<= allocated) */
} phase_t;

/* int8-quantized weights + per-tensor scales (value = int8 * scale) */
static int8_t g_w8[WEIGHTS_ELEMS];
static float  g_sw[NUM_MATMULS];

static void quantize_weights(void) {
    for (int m = 0; m < NUM_MATMULS; m++) {
        const q16 *w = g_weights + MM[m].w_off / 4;
        uint32_t n = MM[m].in_dim * MM[m].out_dim;
        float mx = 1e-9f;
        for (uint32_t i = 0; i < n; i++) { float a = Q2F(w[i]); a = a < 0 ? -a : a; if (a > mx) mx = a; }
        float s = mx / 127.0f;
        g_sw[m] = s;
        int8_t *w8 = g_w8 + MM[m].w_off / 4;
        for (uint32_t i = 0; i < n; i++) {
            int v = (int)lrintf(Q2F(w[i]) / s);
            w8[i] = (int8_t)(v > 127 ? 127 : v < -127 ? -127 : v);
        }
    }
}

/* padded staging buffers (max sizes) */
static int8_t  in_pad8[SEQ_LEN * MAX_IN_DIM];          /* R*rows_per rows, int8 */
static int16_t out_stage16[1024 * 1024];              /* per-DPU int16 tiles */

/* Offload a group of n_sub matmuls that share the same input activation
 * block and (in_dim,out_dim) shape (q/k/v, or w1/w3) in ONE dispatch,
 * amortizing launch + input-transfer overhead. outs[s] receives sub s. */
static void offload_group(struct dpu_set_t set, uint32_t nr,
                          const int *mm_ids, int n_sub,
                          const q16 *in, q16 *const *outs, phase_t *ph) {
    const uint32_t in_dim = MM[mm_ids[0]].in_dim, out_dim = MM[mm_ids[0]].out_dim;

    /* Cap row-groups at 128/NR_TASKLETS so each DPU's rows map 1:1 onto
     * its tasklets; use column groups for the rest of the DPU fan-out. */
    const uint32_t R_CAP = SEQ_LEN / NR_TASKLETS;  /* 128/16 = 8 */
    uint32_t R = nr < R_CAP ? nr : R_CAP;
    uint32_t C = nr / R; if (C < 1) C = 1;
    if (C > out_dim) C = out_dim;
    uint32_t rows_per = ceil_div(SEQ_LEN, R);
    uint32_t cols_per = round4(ceil_div(out_dim, C));
    uint32_t n_active = R * C;
    ph->n_active = n_active;

    /* per-tensor int8 quantization of the shared input block */
    float mx = 1e-9f;
    for (uint32_t i = 0; i < SEQ_LEN * in_dim; i++) { float a = Q2F(in[i]); a = a < 0 ? -a : a; if (a > mx) mx = a; }
    float sa = mx / 127.0f;
    uint32_t padded_rows = R * rows_per;
    memset(in_pad8, 0, (size_t)padded_rows * in_dim);
    for (uint32_t i = 0; i < SEQ_LEN * in_dim; i++) {
        int v = (int)lrintf(Q2F(in[i]) / sa);
        in_pad8[i] = (int8_t)(v > 127 ? 127 : v < -127 ? -127 : v);
    }

    struct dpu_set_t dpu; uint32_t idx;
    static dpu_args_t args_arr[2560];
    const uint32_t block = (uint32_t)n_sub * rows_per * cols_per; /* ints/DPU */

    for (uint32_t d = 0; d < nr; d++) {
        uint32_t cg = d / R;
        args_arr[d].in_dim  = in_dim;
        args_arr[d].out_dim = out_dim;
        args_arr[d].row_cnt = (d < n_active) ? rows_per : 0;
        args_arr[d].col_start = cg * cols_per;
        args_arr[d].col_pad = cols_per;
        args_arr[d].n_sub = n_sub;
        for (int s = 0; s < n_sub; s++) args_arr[d].w_off[s] = MM[mm_ids[s]].w_off / 4;
    }
    DPU_FOREACH(set, dpu, idx) dpu_prepare_xfer(dpu, &args_arr[idx]);
    dpu_push_xfer(set, DPU_XFER_TO_DPU, "ARGS", 0, sizeof(dpu_args_t), DPU_XFER_DEFAULT);

    /* input DMA (shared across all fused sub-matmuls) */
    double t0 = now_ms();
    DPU_FOREACH(set, dpu, idx)
        dpu_prepare_xfer(dpu, &in_pad8[(size_t)(idx % R) * rows_per * in_dim]);
    dpu_push_xfer(set, DPU_XFER_TO_DPU, DPU_MRAM_HEAP_POINTER_NAME,
                  MRAM_IN8_OFF, (size_t)rows_per * in_dim, DPU_XFER_DEFAULT);
    ph->t_in += now_ms() - t0;

    t0 = now_ms();
    dpu_launch(set, DPU_SYNCHRONOUS);
    ph->t_launch += now_ms() - t0;

    /* output DMA: n_sub int16 tiles per DPU */
    t0 = now_ms();
    DPU_FOREACH(set, dpu, idx)
        dpu_prepare_xfer(dpu, &out_stage16[(size_t)idx * block]);
    dpu_push_xfer(set, DPU_XFER_FROM_DPU, DPU_MRAM_HEAP_POINTER_NAME,
                  MRAM_OUT32_OFF, (size_t)block * sizeof(int16_t), DPU_XFER_DEFAULT);
    ph->t_out += now_ms() - t0;

    /* dequantize each sub-matmul's tiles -> outs[s][SEQ][out_dim].
     * acc was right-shifted by OUT_SHIFT on the DPU, so scale it back. */
    for (int s = 0; s < n_sub; s++) {
        float deq = sa * g_sw[mm_ids[s]] * (float)(1 << OUT_SHIFT);
        q16 *out = outs[s];
        for (uint32_t d = 0; d < n_active; d++) {
            uint32_t rg = d % R, cg = d / R;
            const int16_t *tile = &out_stage16[(size_t)d * block + (size_t)s * rows_per * cols_per];
            for (uint32_t r = 0; r < rows_per; r++) {
                uint32_t gr = rg * rows_per + r;
                if (gr >= SEQ_LEN) break;
                for (uint32_t c = 0; c < cols_per; c++) {
                    uint32_t gc = cg * cols_per + c;
                    if (gc >= out_dim) break;
                    out[gr * out_dim + gc] = F2Q((float)tile[r * cols_per + c] * deq);
                }
            }
        }
    }
}

static struct dpu_set_t g_set; static uint32_t g_nr; static phase_t g_ph;

/* Host forward pass with matmuls offloaded to UPMEM; q/k/v and w1/w3 are
 * fused into single dispatches (5 dispatches instead of 8). Non-matmul
 * work reuses the layer.h Q16.16 helpers. */
static void forward_upmem(q16 *x, q16 *logits) {
    static q16 xb[SEQ_LEN*DIM], xq[SEQ_LEN*DIM], xk[SEQ_LEN*DIM], xv[SEQ_LEN*DIM];
    static q16 xatt[SEQ_LEN*DIM], xo[SEQ_LEN*DIM];
    static q16 h1[SEQ_LEN*HIDDEN], h3[SEQ_LEN*HIDDEN], ffn[SEQ_LEN*DIM];

    rmsnorm(xb, x, g_rms_g1, SEQ_LEN);
    { int ids[3]={MM_Q,MM_K,MM_V}; q16 *o[3]={xq,xk,xv}; offload_group(g_set,g_nr,ids,3,xb,o,&g_ph); }
    rope(xq, xk, SEQ_LEN);
    attention(xatt, xq, xk, xv);
    { int ids[1]={MM_O}; q16 *o[1]={xo}; offload_group(g_set,g_nr,ids,1,xatt,o,&g_ph); }
    for (int i = 0; i < SEQ_LEN*DIM; i++) x[i] += xo[i];

    rmsnorm(xb, x, g_rms_g2, SEQ_LEN);
    { int ids[2]={MM_W1,MM_W3}; q16 *o[2]={h1,h3}; offload_group(g_set,g_nr,ids,2,xb,o,&g_ph); }
    swiglu(h1, h3, SEQ_LEN*HIDDEN);
    { int ids[1]={MM_W2}; q16 *o[1]={ffn}; offload_group(g_set,g_nr,ids,1,h1,o,&g_ph); }
    for (int i = 0; i < SEQ_LEN*DIM; i++) x[i] += ffn[i];

    rmsnorm(xb, x, g_rms_g3, SEQ_LEN);
    { int ids[1]={MM_CLS}; q16 *o[1]={logits}; offload_group(g_set,g_nr,ids,1,xb,o,&g_ph); }
}

int main(int argc, char **argv) {
    /* sweep points; each requests that many DPUs */
    uint32_t req[] = {1,2,4,8,16,32,49,64,128,256,512,1024,2048,2560};
    int npts = (int)(sizeof(req)/sizeof(req[0]));
    int reps = (argc > 1) ? atoi(argv[1]) : 20;
    int verify_once = 1;

    init_params();
    quantize_weights();
    static q16 x0[SEQ_LEN * DIM], x[SEQ_LEN * DIM], logits[SEQ_LEN * VOCAB];
    static q16 ref[SEQ_LEN * VOCAB];
    init_input(x0);

    /* CPU reference logits for correctness check */
    memcpy(x, x0, sizeof(x));
    forward(x, ref, matmul_cpu);

    printf("# req_dpus alloc_dpus active_dpus t_in_ms t_launch_ms t_out_ms t_mm_ms\n");
    for (int p = 0; p < npts; p++) {
        struct dpu_set_t set;
        if (dpu_alloc(req[p], NULL, &set) != DPU_OK) {
            fprintf(stderr, "alloc %u failed\n", req[p]);
            continue;
        }
        uint32_t nr; dpu_get_nr_dpus(set, &nr);
        dpu_load(set, DPU_BINARY, NULL);
        /* preload int8 weight blob into every DPU's MRAM once */
        dpu_broadcast_to(set, DPU_MRAM_HEAP_POINTER_NAME, MRAM_W8_OFF,
                         g_w8, WEIGHTS_ELEMS, DPU_XFER_DEFAULT);

        g_set = set; g_nr = nr;

        /* verify correctness on the largest sensible config once */
        if (verify_once) {
            memset(&g_ph, 0, sizeof(g_ph));
            memcpy(x, x0, sizeof(x));
            forward_upmem(x, logits);
            /* int8 offload vs Q16.16 reference: report relative error */
            double num = 0, den = 0, maxabs = 0;
            for (int i = 0; i < SEQ_LEN * VOCAB; i++) {
                double e = Q2F(logits[i]) - Q2F(ref[i]);
                double a = e < 0 ? -e : e;
                if (a > maxabs) maxabs = a;
                num += e * e; den += (double)Q2F(ref[i]) * Q2F(ref[i]);
            }
            fprintf(stderr, "[verify @ %u DPUs] int8 offload rel_l2=%.4f%% max_abs=%.4e\n",
                    nr, 100.0 * sqrt(num / den), maxabs);
            verify_once = 0;
        }

        /* timed repetitions */
        double best_mm = 1e30; phase_t best = {0};
        for (int r = 0; r < reps + 1; r++) {   /* 1 warmup */
            memset(&g_ph, 0, sizeof(g_ph));
            memcpy(x, x0, sizeof(x));
            forward_upmem(x, logits);
            double t_mm = g_ph.t_in + g_ph.t_launch + g_ph.t_out;
            if (r >= 1 && t_mm < best_mm) { best_mm = t_mm; best = g_ph; }
        }
        printf("%u %u %u %.4f %.4f %.4f %.4f\n",
               req[p], nr, best.n_active,
               best.t_in, best.t_launch, best.t_out, best_mm);
        fflush(stdout);
        dpu_free(set);
    }
    return 0;
}
