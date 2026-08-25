// Single-layer Llama2 prefill forward pass (Q16.16), with the eight
// matmuls routed through a pluggable matmul_batch implementation so the
// same non-matmul code runs in the CPU baseline, the stub build and the
// UPMEM-offloaded build.
#ifndef LAYER_H
#define LAYER_H

#include <math.h>
#include <string.h>
#include "common.h"

/* out[SEQ][out_dim] = in[SEQ][in_dim] @ W(mm_id) */
typedef void (*matmul_fn)(q16 *out, const q16 *in, int mm_id);

/* Weights blob, W^T (column-major) layout: WT(mm)[o][k] at MM[mm].w_off */
static q16 g_weights[WEIGHTS_BYTES / 4];
static q16 g_rms_g1[DIM], g_rms_g2[DIM], g_rms_g3[DIM];

static void init_params(void) {
    uint32_t seed = 0x12345678u;
    for (int m = 0; m < NUM_MATMULS; m++) {
        q16 *w = g_weights + MM[m].w_off / 4;
        uint32_t n = MM[m].in_dim * MM[m].out_dim;
        for (uint32_t i = 0; i < n; i++)
            w[i] = lcg_q16(&seed, 0.12f);   /* ~1/sqrt(dim) scale */
    }
    for (int i = 0; i < DIM; i++) {
        g_rms_g1[i] = F2Q(1.0f);
        g_rms_g2[i] = F2Q(1.0f);
        g_rms_g3[i] = F2Q(1.0f);
    }
}

static void init_input(q16 *x /*[SEQ_LEN][DIM]*/) {
    uint32_t seed = 0xCAFEBABEu;
    for (int i = 0; i < SEQ_LEN * DIM; i++)
        x[i] = lcg_q16(&seed, 0.8f);
}

/* Reference CPU matmul: out[b][o] = sum_k in[b][k] * WT[o][k]  (Q16.16) */
static void matmul_cpu(q16 *out, const q16 *in, int mm_id) {
    const uint32_t in_dim = MM[mm_id].in_dim, out_dim = MM[mm_id].out_dim;
    const q16 *wt = g_weights + MM[mm_id].w_off / 4;
    for (uint32_t b = 0; b < SEQ_LEN; b++) {
        const q16 *xr = in + b * in_dim;
        for (uint32_t o = 0; o < out_dim; o++) {
            const q16 *wr = wt + o * in_dim;
            int64_t acc = 0;
            for (uint32_t k = 0; k < in_dim; k++)
                acc += (int64_t)xr[k] * (int64_t)wr[k];
            out[b * out_dim + o] = (q16)(acc >> 16);
        }
    }
}

static void rmsnorm(q16 *dst, const q16 *src, const q16 *g, int n_rows) {
    for (int b = 0; b < n_rows; b++) {
        const q16 *x = src + b * DIM;
        q16 *y = dst + b * DIM;
        float ss = 0.0f;
        for (int i = 0; i < DIM; i++) { float f = Q2F(x[i]); ss += f * f; }
        float scale = 1.0f / sqrtf(ss / DIM + 1e-5f);
        for (int i = 0; i < DIM; i++)
            y[i] = F2Q(Q2F(x[i]) * scale * Q2F(g[i]));
    }
}

static void rope(q16 *q, q16 *k, int n_rows) {
    for (int b = 0; b < n_rows; b++) {
        for (int h = 0; h < N_HEADS; h++) {
            q16 *qh = q + b * DIM + h * HEAD_DIM;
            q16 *kh = k + b * DIM + h * HEAD_DIM;
            for (int i = 0; i < HEAD_DIM; i += 2) {
                float freq = powf(10000.0f, -(float)i / HEAD_DIM);
                float a = (float)b * freq;
                float c = cosf(a), s = sinf(a);
                float q0 = Q2F(qh[i]), q1 = Q2F(qh[i + 1]);
                float k0 = Q2F(kh[i]), k1 = Q2F(kh[i + 1]);
                qh[i]     = F2Q(q0 * c - q1 * s);
                qh[i + 1] = F2Q(q0 * s + q1 * c);
                kh[i]     = F2Q(k0 * c - k1 * s);
                kh[i + 1] = F2Q(k0 * s + k1 * c);
            }
        }
    }
}

/* Causal multi-head attention, entirely on the host (branchy, non-offloaded) */
static void attention(q16 *out, const q16 *q, const q16 *k, const q16 *v) {
    static float scores[SEQ_LEN];
    const float inv_sqrt_hd = 1.0f / sqrtf((float)HEAD_DIM);
    for (int h = 0; h < N_HEADS; h++) {
        for (int b = 0; b < SEQ_LEN; b++) {
            const q16 *qh = q + b * DIM + h * HEAD_DIM;
            float maxv = -1e30f;
            for (int j = 0; j <= b; j++) {
                const q16 *kh = k + j * DIM + h * HEAD_DIM;
                int64_t acc = 0;
                for (int i = 0; i < HEAD_DIM; i++)
                    acc += (int64_t)qh[i] * (int64_t)kh[i];
                float s = (float)acc / (65536.0f * 65536.0f) * inv_sqrt_hd;
                scores[j] = s;
                if (s > maxv) maxv = s;
            }
            float sum = 0.0f;
            for (int j = 0; j <= b; j++) {
                scores[j] = expf(scores[j] - maxv);
                sum += scores[j];
            }
            float inv = 1.0f / sum;
            q16 *ob = out + b * DIM + h * HEAD_DIM;
            for (int i = 0; i < HEAD_DIM; i++) {
                float acc = 0.0f;
                for (int j = 0; j <= b; j++)
                    acc += scores[j] * Q2F(v[j * DIM + h * HEAD_DIM + i]);
                ob[i] = F2Q(acc * inv);
            }
        }
    }
}

static void swiglu(q16 *h1, const q16 *h3, int n) {
    for (int i = 0; i < n; i++) {
        float f = Q2F(h1[i]);
        float silu = f / (1.0f + expf(-f));
        h1[i] = F2Q(silu * Q2F(h3[i]));
    }
}

/* Full forward pass; the eight mm() calls are the offloadable work. */
static void forward(q16 *x /*[SEQ][DIM], modified*/, q16 *logits, matmul_fn mm) {
    static q16 xb[SEQ_LEN * DIM], xq[SEQ_LEN * DIM], xk[SEQ_LEN * DIM];
    static q16 xv[SEQ_LEN * DIM], xatt[SEQ_LEN * DIM], xo[SEQ_LEN * DIM];
    static q16 h1[SEQ_LEN * HIDDEN], h3[SEQ_LEN * HIDDEN], ffn[SEQ_LEN * DIM];

    rmsnorm(xb, x, g_rms_g1, SEQ_LEN);
    mm(xq, xb, MM_Q);
    mm(xk, xb, MM_K);
    mm(xv, xb, MM_V);
    rope(xq, xk, SEQ_LEN);
    attention(xatt, xq, xk, xv);
    mm(xo, xatt, MM_O);
    for (int i = 0; i < SEQ_LEN * DIM; i++) x[i] += xo[i];

    rmsnorm(xb, x, g_rms_g2, SEQ_LEN);
    mm(h1, xb, MM_W1);
    mm(h3, xb, MM_W3);
    swiglu(h1, h3, SEQ_LEN * HIDDEN);
    mm(ffn, h1, MM_W2);
    for (int i = 0; i < SEQ_LEN * DIM; i++) x[i] += ffn[i];

    rmsnorm(xb, x, g_rms_g3, SEQ_LEN);
    mm(logits, xb, MM_CLS);
}

#endif
