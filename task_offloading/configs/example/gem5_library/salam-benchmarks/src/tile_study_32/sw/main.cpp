#include <cstdio>
#include <cstdlib>
#include "../../common/m5ops.h"
#include "../gemm_clstr_hw_defines.h"
#include "../defines.h"
#include "bench.h"

#define DEV_INIT 0x01
#define DEV_INTR 0x04

volatile int stage;

volatile uint8_t  *top   = (uint8_t  *)(TOP + 0x00);
volatile uint32_t *val_a = (uint32_t *)(TOP + 0x01);
volatile uint32_t *val_b = (uint32_t *)(TOP + 0x09);
volatile uint32_t *val_c = (uint32_t *)(TOP + 0x11);

/* Bare-metal exp() — libm isn't linked in this build. A 20-term Taylor
 * series is plenty for the row-softmax use below, where the pre-shifted
 * input `x = C[i,j] - max(C[i, :])` is always <= 0 (and in practice well
 * within [-20, 0] for our tiled GEMM on small random data). The clamp
 * guards against pathological rows. */
static TYPE exp_approx(TYPE x)
{
    if (x < (TYPE)-20.0) return (TYPE)0.0;
    TYPE r = (TYPE)1.0;
    TYPE term = (TYPE)1.0;
    for (int k = 1; k < 20; k++) {
        term *= x / (TYPE)k;
        r += term;
    }
    if (r < (TYPE)0.0) r = (TYPE)0.0;
    return r;
}

/* Initialize A, B, C in DRAM. Pulled out of main() so it inherits the
 * file-default -O3 from the Makefile instead of main()'s optimize("0").
 *
 * Two key tricks to make this fast on bare-metal MinorCPU + soft-float:
 *   1. We use a precomputed 16-entry double table instead of any int->double
 *      cast, so the inner loop has zero __aeabi_i2d / __aeabi_ddiv calls.
 *   2. The LCG -> table lookup is pure integer code that -O3 vectorizes
 *      and unrolls cleanly.
 *
 * At N=2048 the loop is 4 M iterations of ~10 ARM instructions each
 * (sub-second of host wall time), versus the previous srand/rand+softfloat
 * version which took >5 minutes of simulated soft-float work to even
 * reach the first dispatch. */
static void __attribute__((noinline))
init_data(TYPE *A, TYPE *B, TYPE *C, int N)
{
    static const TYPE vals[16] = {
        (TYPE)-8.0, (TYPE)-7.0, (TYPE)-6.0, (TYPE)-5.0,
        (TYPE)-4.0, (TYPE)-3.0, (TYPE)-2.0, (TYPE)-1.0,
        (TYPE) 0.0, (TYPE) 1.0, (TYPE) 2.0, (TYPE) 3.0,
        (TYPE) 4.0, (TYPE) 5.0, (TYPE) 6.0, (TYPE) 7.0
    };
    const TYPE zero = (TYPE)0.0;
    for (int i = 0; i < N * N; i++) {
        unsigned int r = (unsigned int)i * 1664525u + 1013904223u;
        A[i] = vals[(r >> 24) & 0x0F];
        B[i] = vals[(r >> 16) & 0x0F];
        C[i] = zero;
    }
}

/* The three helpers below used to live inline in main() at optimize("0").
 * At N=512 that made the timed region dominated by -O0 soft-float staging,
 * accumulate, and softmax work — billions of ARM instructions that had
 * *nothing* to do with task-offloading overhead, drowning the signal we
 * actually wanted to measure (dispatch + DMA + IRQ round-trip).
 *
 * Pulling them out into static noinline helpers lets them inherit the
 * file-default -O3 from the Makefile. The -O0 envelope in main() then
 * collapses to just the MMR writes, wfi, and dispatch-loop bookkeeping,
 * which *is* the offload-overhead story. Staging+accumulate are still
 * inside the m5_reset_stats/m5_dump_stats window, so simSeconds still
 * correctly reflects all CPU-side work the offload imposes. */

static void __attribute__((noinline))
stage_tile_in(TYPE *m1, TYPE *m2,
              const TYPE *A, const TYPE *B,
              int i0, int j0, int k0)
{
    for (int i = 0; i < ROW; i++)
        for (int k = 0; k < ROW; k++)
            m1[i * ROW + k] = A[(i0 + i) * PROB_N + (k0 + k)];
    for (int k = 0; k < ROW; k++)
        for (int j = 0; j < ROW; j++)
            m2[k * ROW + j] = B[(k0 + k) * PROB_N + (j0 + j)];
}

static void __attribute__((noinline))
accumulate_tile_out(TYPE *C, const TYPE *m3, int i0, int j0)
{
    for (int i = 0; i < ROW; i++)
        for (int j = 0; j < ROW; j++)
            C[(i0 + i) * PROB_N + (j0 + j)] += m3[i * ROW + j];
}

static void __attribute__((noinline))
softmax_rows(TYPE *C)
{
    for (int i = 0; i < PROB_N; i++) {
        TYPE mx = C[i * PROB_N];
        for (int j = 1; j < PROB_N; j++)
            if (C[i * PROB_N + j] > mx) mx = C[i * PROB_N + j];
        TYPE sum = (TYPE)0;
        for (int j = 0; j < PROB_N; j++) {
            C[i * PROB_N + j] = exp_approx(C[i * PROB_N + j] - mx);
            sum += C[i * PROB_N + j];
        }
        for (int j = 0; j < PROB_N; j++)
            C[i * PROB_N + j] /= sum;
    }
}

int __attribute__((optimize("0")))
main(void)
{
    const int T = ROW;
    const int N = PROB_N;
    const int tpd = N / T;                     /* tiles per dim */
    const int total_dispatch = tpd * tpd * tpd;

    /* Staging buffers the accelerator's Top DMAs into its SPM. */
    uint32_t base = 0x80c00000;
    TYPE *m1 = (TYPE *)(base                 );
    TYPE *m2 = (TYPE *)(base +  8 * T * T    );
    TYPE *m3 = (TYPE *)(base + 16 * T * T    );

    /* Big problem matrices in DRAM, past the staging region. */
    uintptr_t abc_base = (uintptr_t)base + 32 * T * T;
    TYPE *A = (TYPE *)(abc_base                                 );
    TYPE *B = (TYPE *)(abc_base +     (uintptr_t)N * N * sizeof(TYPE));
    TYPE *C = (TYPE *)(abc_base + 2 * (uintptr_t)N * N * sizeof(TYPE));

    /* Print the banner first so the SGE/system.terminal heartbeat tells us
     * the kernel actually booted, *before* we burn cycles in the data-init
     * loop. */
    printf("tile_study: T=%d N=%d dispatches=%d\n", T, N, total_dispatch);
    printf("tile_study: initializing A, B, C in DRAM...\n");

    /* Init runs at the file-default -O3 (see init_data above). */
    init_data(A, B, C, N);
    printf("tile_study: init complete, starting dispatch loop\n");

    m5_reset_stats();
    stage = 0;
    int done = 0;
    /* Print progress ~32 times per run so we can watch the dispatch loop
     * advance without adding measurable printf overhead inside the timed
     * region. For dispatch counts <= 32 (e.g. T=256/N=512 with 8
     * dispatches) the step degenerates to 1 and we print every dispatch,
     * which is still a tiny constant cost. */
    const int progress_step = total_dispatch > 32 ? total_dispatch / 32 : 1;

    /* Tiled GEMM: C[i0..i0+T, j0..j0+T] += A[i0.., k0..] * B[k0.., j0..] */
    for (int i0 = 0; i0 < N; i0 += T) {
        for (int j0 = 0; j0 < N; j0 += T) {
            for (int k0 = 0; k0 < N; k0 += T) {
                /* Stage A and B sub-tiles into m1/m2 (runs at -O3 via
                 * the noinline helper, so the CPU-side copy cost does
                 * not dominate the timed region). */
                stage_tile_in(m1, m2, A, B, i0, j0, k0);

                /* Dispatch the accelerator (same MMR contract as gemm) */
                *val_a = (uint32_t)(void *)m1;
                *val_b = (uint32_t)(void *)m2;
                *val_c = (uint32_t)(void *)m3;
                *top   = DEV_INIT;
                done += 1;
                while (stage < done) {
                    asm volatile("wfi");
                }

                /* Heartbeat: lets us watch the dispatch loop advance in
                 * system.terminal even on huge sweeps. */
                if ((done % progress_step) == 0 || done == total_dispatch) {
                    printf("tile_study: dispatch %d/%d\n",
                           done, total_dispatch);
                }

                /* Accumulate m3 into the corresponding C sub-tile
                 * (runs at -O3 via the noinline helper). */
                accumulate_tile_out(C, m3, i0, j0);
            }
        }
    }

    /* Row-wise softmax over the full N x N result (runs at -O3 via
     * the noinline helper so the soft-float Taylor expansion does not
     * dominate host wall time). */
    softmax_rows(C);

    m5_dump_stats();
    printf("tile_study: complete, dispatches=%d, C[0]=%d (x1000)\n",
           done, (int)(C[0] * 1000));
    m5_exit();
    return 0;
}
