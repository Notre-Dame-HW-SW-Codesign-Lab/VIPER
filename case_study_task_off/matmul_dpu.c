// UPMEM DPU kernel: INT8 x INT8 -> INT32 matmul tile.
// Each DPU computes rows [0,row_cnt) of its local int8 input block against
// weight columns [col_start, col_start+col_pad). One native mul_sl_sl per
// MAC (the DPU has no wider hardware multiplier). Weights (W^T,
// column-major, int8) are pre-loaded in MRAM; int8 inputs are DMA'd per
// dispatch and int32 partial sums are DMA'd back.
#include <stdint.h>
#include <defs.h>
#include <mram.h>
#include <alloc.h>
#include <barrier.h>
#include <built_ins.h>
#include "common.h"

__host dpu_args_t ARGS;
BARRIER_INIT(bar, NR_TASKLETS);

int main(void) {
    const unsigned tid = me();
    if (tid == 0) mem_reset();
    barrier_wait(&bar);

    const uint32_t in_dim   = ARGS.in_dim;
    const uint32_t out_dim  = ARGS.out_dim;
    const uint32_t col_pad  = ARGS.col_pad;
    const uint32_t row_cnt  = ARGS.row_cnt;
    const uint32_t n_sub    = ARGS.n_sub;

    int8_t  *xrow = (int8_t  *)mem_alloc(MAX_IN_DIM);        /* bytes */
    int8_t  *wcol = (int8_t  *)mem_alloc(MAX_IN_DIM);
    int16_t *orow = (int16_t *)mem_alloc(MAX_OUT_DIM * 2);   /* int16 partials */

    uint32_t mram = (uint32_t)DPU_MRAM_HEAP_POINTER;
    uint32_t in_base  = mram + MRAM_IN8_OFF;
    uint32_t out_base = mram + MRAM_OUT32_OFF;

    for (uint32_t r = tid; r < row_cnt; r += NR_TASKLETS) {
        mram_read((__mram_ptr void *)(in_base + r * in_dim), xrow, in_dim);
        for (uint32_t s = 0; s < n_sub; s++) {
            uint32_t w_base = mram + MRAM_W8_OFF + ARGS.w_off[s];
            for (uint32_t c = 0; c < col_pad; c++) {
                uint32_t col = ARGS.col_start + c;
                if (col >= out_dim) { orow[c] = 0; continue; }
                mram_read((__mram_ptr void *)(w_base + col * in_dim), wcol, in_dim);
                int32_t acc = 0;
                for (uint32_t k = 0; k < in_dim; k++) {
                    int32_t p;
                    __builtin_mul_sl_sl_rrr(p, xrow[k], wcol[k]); /* signed 16x16->32, exact for int8 */
                    acc += p;
                }
                orow[c] = (int16_t)(acc >> OUT_SHIFT);
            }
            /* sub s, row r tile at (s*row_cnt + r); int16 => col_pad*2 bytes */
            mram_write(orow, (__mram_ptr void *)(out_base + (s * row_cnt + r) * col_pad * 2),
                       col_pad * 2);
        }
    }
    return 0;
}
