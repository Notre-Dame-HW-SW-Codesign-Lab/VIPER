// Shared definitions for the VIPER UPMEM case study workload:
// single-layer Llama2 prefill, Q16.16 fixed point.
//   dim=128, hidden_dim=256, n_heads=8, head_dim=16, B=128 tokens, vocab=256
// Eight offloadable matmuls per forward pass:
//   q, k, v, o (128x128), w1, w3 (128x256), w2 (256x128), cls (128x256)
// => 25,165,824 MACs; 576 KB inputs / 704 KB outputs transferred.
#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#define DIM        128
#define HIDDEN     256
#define N_HEADS    8
#define HEAD_DIM   (DIM / N_HEADS)   /* 16 */
#define SEQ_LEN    128               /* B: prefill batch (tokens) */
#define VOCAB      256

#define NUM_MATMULS 8

/* Q16.16 helpers */
typedef int32_t q16;
#define Q16_ONE   (1 << 16)
#define F2Q(x)    ((q16)((x) * 65536.0f))
#define Q2F(x)    ((float)(x) / 65536.0f)

static inline q16 q16_mul(q16 a, q16 b) {
    return (q16)(((int64_t)a * (int64_t)b) >> 16);
}

/* Matmul descriptor table shared by CPU reference, host and DPU kernel.
 * Weights are stored TRANSPOSED (column-major, i.e. W^T[out][in]) so a
 * weight column is a contiguous 8-byte-aligned MRAM block on the DPU. */
typedef struct {
    const char *name;
    uint32_t in_dim;
    uint32_t out_dim;
    uint32_t w_off;      /* byte offset of W^T inside the weights blob */
} mm_desc_t;

#define MM_Q   0
#define MM_K   1
#define MM_V   2
#define MM_O   3
#define MM_W1  4
#define MM_W3  5
#define MM_W2  6
#define MM_CLS 7

static const mm_desc_t MM[NUM_MATMULS] = {
    {"q",   DIM,    DIM,    0},
    {"k",   DIM,    DIM,    DIM*DIM*4},
    {"v",   DIM,    DIM,    2*DIM*DIM*4},
    {"o",   DIM,    DIM,    3*DIM*DIM*4},
    {"w1",  DIM,    HIDDEN, 4*DIM*DIM*4},
    {"w3",  DIM,    HIDDEN, 4*DIM*DIM*4 + DIM*HIDDEN*4},
    {"w2",  HIDDEN, DIM,    4*DIM*DIM*4 + 2*DIM*HIDDEN*4},
    {"cls", DIM,    VOCAB,  4*DIM*DIM*4 + 2*DIM*HIDDEN*4 + HIDDEN*DIM*4},
};

#define WEIGHTS_BYTES (4*DIM*DIM*4 + 2*DIM*HIDDEN*4 + HIDDEN*DIM*4 + DIM*VOCAB*4) /* 768 KB */

/* --- INT8-quantized offload path ---
 * The offloaded matmuls run in symmetric per-tensor int8 (weights and
 * activations), accumulating in int32 on the DPU: one native mul_sl_sl
 * per MAC. Weights are pre-quantized once and resident in MRAM; the host
 * quantizes each activation block before transfer and dequantizes the
 * int32 partial sums on return. MRAM heap layout (bytes): */
#define MRAM_W8_OFF    0                       /* int8 weights, WEIGHTS_ELEMS bytes */
#define MRAM_IN8_OFF   (256*1024)              /* int8 activations */
#define MRAM_OUT32_OFF (256*1024 + 256*1024)   /* int32 partial sums */
#define WEIGHTS_ELEMS  (WEIGHTS_BYTES / 4)     /* = int8 weight-blob byte size */
/* DPU right-shifts the int32 partial sum to int16 before write-back to
 * halve the output DMA. Worst-case |acc| = in_dim*127*127 = 4.13M
 * (in_dim=256) -> >>8 = 16129 < 32767, so int16 never overflows. */
#define OUT_SHIFT 8
#define MAX_IN_DIM    HIDDEN   /* 256 */
#define MAX_OUT_DIM   HIDDEN   /* 256 */

/* Per-dispatch arguments pushed to each DPU (size must be multiple of 8).
 * A dispatch may fuse up to MAX_SUB matmuls that share the same input
 * block and (in_dim,out_dim) shape but use different weights (q/k/v and
 * w1/w3), cutting per-dispatch launch+transfer overhead. */
#define MAX_SUB 3
typedef struct {
    uint32_t in_dim;
    uint32_t out_dim;    /* real out_dim (shared across the fused sub-matmuls) */
    uint32_t row_cnt;    /* rows of the batch this DPU computes */
    uint32_t col_start;  /* first weight column this DPU computes */
    uint32_t col_pad;    /* padded col count (even); cols beyond out_dim are zeroed */
    uint32_t n_sub;      /* number of fused sub-matmuls (1..MAX_SUB) */
    uint32_t w_off[MAX_SUB]; /* int8 byte offset of each W^T in the weights blob */
    uint32_t _pad;       /* keep size a multiple of 8 (40 bytes) */
} dpu_args_t;

/* Deterministic LCG so CPU and UPMEM builds generate identical data */
static inline uint32_t lcg_next(uint32_t *s) {
    *s = *s * 1664525u + 1013904223u;
    return *s;
}
/* random Q16.16 in (-scale, scale) */
static inline q16 lcg_q16(uint32_t *s, float scale) {
    int32_t r = (int32_t)lcg_next(s);
    return (q16)(((int64_t)r >> 16) * (int64_t)F2Q(scale) >> 15);
}

#endif
