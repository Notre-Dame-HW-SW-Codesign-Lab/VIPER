/* Llama2 transformer with SALAM accelerated batched matmul.
 *
 * Batch prefill: process B tokens simultaneously.
 * Linear layers use matmul_batch (triple-tiled, accelerated).
 * RMSNorm, RoPE, attention, SwiGLU run per-token on CPU.
 *
 * Memory layout: feature-major buf[feature * B + token].
 */
#ifndef LLAMA2_H
#define LLAMA2_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include "../defines.h"
#include "../gemm_clstr_hw_defines.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * Accelerator globals (defined in main.cpp)
 * ---------------------------------------------------------------- */
extern volatile int stage;
extern volatile uint8_t  *top_reg;
extern volatile uint32_t *val_a;
extern volatile uint32_t *val_b;
extern volatile uint32_t *val_c;

/* ----------------------------------------------------------------
 * Transformer data structures
 * ---------------------------------------------------------------- */
typedef struct {
    int dim;
    int hidden_dim;
    int n_layers;
    int n_heads;
    int vocab_size;
    int seq_len;
} Config;

typedef struct {
    TYPE *token_embedding_table; /* (vocab_size, dim) */
    TYPE *rms_att_weight;        /* (n_layers, dim) */
    TYPE *wq;                    /* (n_layers, dim, dim) */
    TYPE *wk;                    /* (n_layers, dim, dim) */
    TYPE *wv;                    /* (n_layers, dim, dim) */
    TYPE *wo;                    /* (n_layers, dim, dim) */
    TYPE *rms_ffn_weight;        /* (n_layers, dim) */
    TYPE *w1;                    /* (n_layers, hidden_dim, dim) */
    TYPE *w2;                    /* (n_layers, dim, hidden_dim) */
    TYPE *w3;                    /* (n_layers, hidden_dim, dim) */
    TYPE *rms_final_weight;      /* (dim,) */
    TYPE *wcls;                  /* (vocab_size, dim) */
} TransformerWeights;

typedef struct {
    TYPE *x;       /* activation (dim * B, feature-major) */
    TYPE *xb;      /* residual branch buffer (dim * B) */
    TYPE *xb2;     /* additional buffer (dim * B) */
    TYPE *hb;      /* hidden dim buffer (hidden_dim * B) */
    TYPE *hb2;     /* w3 gating buffer (hidden_dim * B) */
    TYPE *q;       /* query (dim * B) */
    TYPE *att;     /* attention scores (n_heads * seq_len), per-token */
    TYPE *logits;  /* output logits (vocab_size * B) */
    TYPE *key_cache;   /* (n_layers, seq_len, dim) */
    TYPE *value_cache; /* (n_layers, seq_len, dim) */
} RunState;

typedef struct {
    Config config;
    TransformerWeights weights;
    RunState state;
} Transformer;

/* ----------------------------------------------------------------
 * Batched matrix multiply: SALAM accelerated
 *
 * The HW accelerator computes full ROW x ROW matrix multiply.
 * For batched matmul W(d_tile, n_tile) @ X(n_tile, b_tile):
 *   - Copy W tile into m1[ROW][ROW], zero-padded
 *   - Copy X tile (b_tile columns) into m2[ROW][ROW], zero-padded
 *   - Run accelerator (full matmul m1 @ m2 -> m3)
 *   - Extract d_tile x b_tile result from m3
 * ---------------------------------------------------------------- */
static void matmul_salam_batch(TYPE *tile_out, TYPE *X, TYPE *W,
                                int n_tile, int d_tile, int b_tile,
                                int w_stride, int x_stride) {
    uint32_t base = 0x80c00000;
    TYPE *m1 = (TYPE *)base;
    TYPE *m2 = (TYPE *)(base + 8 * ROW * COL);
    TYPE *m3 = (TYPE *)(base + 16 * ROW * COL);

    /* Copy weight tile W[d_tile][n_tile] -> m1[ROW][ROW], zero-padded */
    for (int i = 0; i < d_tile; i++) {
        for (int k = 0; k < n_tile; k++) {
            m1[i * ROW + k] = W[i * w_stride + k];
        }
        for (int k = n_tile; k < ROW; k++) {
            m1[i * ROW + k] = 0.0;
        }
    }
    for (int i = d_tile; i < ROW; i++) {
        for (int k = 0; k < ROW; k++) {
            m1[i * ROW + k] = 0.0;
        }
    }

    /* Copy input tile X[n_tile][b_tile] -> m2[ROW][ROW], zero-padded */
    for (int k = 0; k < n_tile; k++) {
        for (int j = 0; j < b_tile; j++) {
            m2[k * ROW + j] = X[k * x_stride + j];
        }
        for (int j = b_tile; j < ROW; j++) {
            m2[k * ROW + j] = 0.0;
        }
    }
    for (int k = n_tile; k < ROW; k++) {
        for (int j = 0; j < ROW; j++) {
            m2[k * ROW + j] = 0.0;
        }
    }

    /* Trigger accelerator */
    *val_a = (uint32_t)(void *)m1;
    *val_b = (uint32_t)(void *)m2;
    *val_c = (uint32_t)(void *)m3;
    stage = 0;
    *top_reg = 0x01;
    while (stage < 1) {
        asm volatile("wfi");
    }

    /* Extract result tile m3[d_tile][b_tile] -> tile_out */
    for (int i = 0; i < d_tile; i++) {
        for (int j = 0; j < b_tile; j++) {
            tile_out[i * b_tile + j] = m3[i * ROW + j];
        }
    }
}

/* Tiled batched matmul: W(d,n) @ X(n,B) -> Xout(d,B)
 * Triple-tiles along d, n, and B dimensions.
 * X and Xout use feature-major layout: [feature * B + token].
 * W is row-major: [row * n + col]. */
static void matmul_batch(TYPE *xout, TYPE *X, TYPE *W, int n, int d, int B) {
    /* Zero output: d * B elements */
    memset(xout, 0, d * B * sizeof(TYPE));

    for (int d_off = 0; d_off < d; d_off += ROW) {
        int d_tile = (d - d_off < ROW) ? (d - d_off) : ROW;

        for (int n_off = 0; n_off < n; n_off += ROW) {
            int n_tile = (n - n_off < ROW) ? (n - n_off) : ROW;

            for (int b_off = 0; b_off < B; b_off += ROW) {
                int b_tile = (B - b_off < ROW) ? (B - b_off) : ROW;

                uint32_t tmp_base = 0x80c00000 + 24 * ROW * COL;
                TYPE *tile_out = (TYPE *)tmp_base;

                matmul_salam_batch(tile_out,
                                   X + n_off * B + b_off,
                                   W + d_off * n + n_off,
                                   n_tile, d_tile, b_tile, n, B);

                /* Accumulate into output */
                for (int i = 0; i < d_tile; i++) {
                    for (int j = 0; j < b_tile; j++) {
                        xout[(d_off + i) * B + (b_off + j)] += tile_out[i * b_tile + j];
                    }
                }
            }
        }
    }
}

/* ----------------------------------------------------------------
 * Neural net operations (CPU, per-token)
 * ---------------------------------------------------------------- */
static void softmax(TYPE *x, int size) {
    TYPE max_val = x[0];
    for (int i = 1; i < size; i++) {
        if (x[i] > max_val) max_val = x[i];
    }
    TYPE sum = 0.0;
    for (int i = 0; i < size; i++) {
        x[i] = exp(x[i] - max_val);
        sum += x[i];
    }
    for (int i = 0; i < size; i++) {
        x[i] /= sum;
    }
}

/* ----------------------------------------------------------------
 * Memory management
 * ---------------------------------------------------------------- */
static void malloc_run_state(RunState *s, Config *p) {
    int B = p->seq_len; /* batch size = sequence length for prefill */
    s->x      = (TYPE *)calloc(p->dim * B, sizeof(TYPE));
    s->xb     = (TYPE *)calloc(p->dim * B, sizeof(TYPE));
    s->xb2    = (TYPE *)calloc(p->dim * B, sizeof(TYPE));
    s->hb     = (TYPE *)calloc(p->hidden_dim * B, sizeof(TYPE));
    s->hb2    = (TYPE *)calloc(p->hidden_dim * B, sizeof(TYPE));
    s->q      = (TYPE *)calloc(p->dim * B, sizeof(TYPE));
    s->att    = (TYPE *)calloc(p->n_heads * p->seq_len, sizeof(TYPE));
    s->logits = (TYPE *)calloc(p->vocab_size * B, sizeof(TYPE));
    s->key_cache   = (TYPE *)calloc(p->n_layers * p->seq_len * p->dim, sizeof(TYPE));
    s->value_cache = (TYPE *)calloc(p->n_layers * p->seq_len * p->dim, sizeof(TYPE));
    if (!s->x || !s->xb || !s->xb2 || !s->hb || !s->hb2 || !s->q
        || !s->att || !s->logits || !s->key_cache || !s->value_cache) {
        printf("malloc_run_state failed!\n");
    }
}

static void free_run_state(RunState *s) {
    free(s->x);
    free(s->xb);
    free(s->xb2);
    free(s->hb);
    free(s->hb2);
    free(s->q);
    free(s->att);
    free(s->logits);
    free(s->key_cache);
    free(s->value_cache);
}

/* ----------------------------------------------------------------
 * Synthetic weight initialization
 * ---------------------------------------------------------------- */
static void init_weights_synthetic(TransformerWeights *w, Config *p) {
    int dim = p->dim;
    int hidden_dim = p->hidden_dim;
    int n_layers = p->n_layers;
    int vocab_size = p->vocab_size;

    w->token_embedding_table = (TYPE *)calloc(vocab_size * dim, sizeof(TYPE));
    w->rms_att_weight  = (TYPE *)calloc(n_layers * dim, sizeof(TYPE));
    w->wq  = (TYPE *)calloc(n_layers * dim * dim, sizeof(TYPE));
    w->wk  = (TYPE *)calloc(n_layers * dim * dim, sizeof(TYPE));
    w->wv  = (TYPE *)calloc(n_layers * dim * dim, sizeof(TYPE));
    w->wo  = (TYPE *)calloc(n_layers * dim * dim, sizeof(TYPE));
    w->rms_ffn_weight  = (TYPE *)calloc(n_layers * dim, sizeof(TYPE));
    w->w1  = (TYPE *)calloc(n_layers * dim * hidden_dim, sizeof(TYPE));
    w->w2  = (TYPE *)calloc(n_layers * hidden_dim * dim, sizeof(TYPE));
    w->w3  = (TYPE *)calloc(n_layers * dim * hidden_dim, sizeof(TYPE));
    w->rms_final_weight = (TYPE *)calloc(dim, sizeof(TYPE));
    w->wcls = (TYPE *)calloc(vocab_size * dim, sizeof(TYPE));

    srand(42);

    /* Token embeddings: small random */
    for (int i = 0; i < vocab_size * dim; i++) {
        w->token_embedding_table[i] = ((TYPE)rand() / RAND_MAX) * 0.1 - 0.05;
    }

    /* RMS norm weights = 1.0 */
    for (int i = 0; i < n_layers * dim; i++) {
        w->rms_att_weight[i] = 1.0;
        w->rms_ffn_weight[i] = 1.0;
    }
    for (int i = 0; i < dim; i++) {
        w->rms_final_weight[i] = 1.0;
    }

    /* Attention weights: small random */
    for (int i = 0; i < n_layers * dim * dim; i++) {
        w->wq[i] = ((TYPE)rand() / RAND_MAX) * 0.02 - 0.01;
        w->wk[i] = ((TYPE)rand() / RAND_MAX) * 0.02 - 0.01;
        w->wv[i] = ((TYPE)rand() / RAND_MAX) * 0.02 - 0.01;
        w->wo[i] = ((TYPE)rand() / RAND_MAX) * 0.02 - 0.01;
    }

    /* FFN weights: small random */
    for (int i = 0; i < n_layers * dim * hidden_dim; i++) {
        w->w1[i] = ((TYPE)rand() / RAND_MAX) * 0.02 - 0.01;
        w->w3[i] = ((TYPE)rand() / RAND_MAX) * 0.02 - 0.01;
    }
    for (int i = 0; i < n_layers * hidden_dim * dim; i++) {
        w->w2[i] = ((TYPE)rand() / RAND_MAX) * 0.02 - 0.01;
    }

    /* Classifier weights: small random */
    for (int i = 0; i < vocab_size * dim; i++) {
        w->wcls[i] = ((TYPE)rand() / RAND_MAX) * 0.02 - 0.01;
    }
}

static void free_weights(TransformerWeights *w) {
    free(w->token_embedding_table);
    free(w->rms_att_weight);
    free(w->wq);
    free(w->wk);
    free(w->wv);
    free(w->wo);
    free(w->rms_ffn_weight);
    free(w->w1);
    free(w->w2);
    free(w->w3);
    free(w->rms_final_weight);
    free(w->wcls);
}

/* ----------------------------------------------------------------
 * Batched transformer forward pass.
 *
 * Processes B tokens simultaneously (batch prefill).
 * Linear layers (Q,K,V,Wo,w1,w2,w3,classifier): matmul_batch
 * RMSNorm, RoPE, SwiGLU: per-token CPU loops
 * Attention: per-token, causal (token b attends to 0..b)
 *
 * All activation buffers use feature-major layout:
 *   buf[feature_index * B + token_index]
 * ---------------------------------------------------------------- */
static TYPE *forward_batch(Transformer *transformer, int *tokens, int *positions, int B) {
    Config *p = &transformer->config;
    TransformerWeights *w = &transformer->weights;
    RunState *s = &transformer->state;
    TYPE *x = s->x;
    int dim = p->dim;
    int hidden_dim = p->hidden_dim;
    int head_size = dim / p->n_heads;

    /* 1. Embed all B tokens into x(dim, B) feature-major */
    for (int b = 0; b < B; b++) {
        TYPE *emb = w->token_embedding_table + tokens[b] * dim;
        for (int d = 0; d < dim; d++) {
            x[d * B + b] = emb[d];
        }
    }

    /* 2. Process through transformer layers */
    for (int l = 0; l < p->n_layers; l++) {

        /* 2a. Attention RMSNorm: per-token */
        for (int b = 0; b < B; b++) {
            TYPE ss = 0.0;
            for (int j = 0; j < dim; j++) {
                TYPE val = x[j * B + b];
                ss += val * val;
            }
            ss = 1.0 / sqrt(ss / dim + 1e-5);
            for (int j = 0; j < dim; j++) {
                s->xb[j * B + b] = w->rms_att_weight[l * dim + j] * (ss * x[j * B + b]);
            }
        }

        int loff = l * p->seq_len * dim;

        /* 2b. Q projection (accelerated) */
        matmul_batch(s->q, s->xb, w->wq + l * dim * dim, dim, dim, B);

        /* 2c. K projection -> xb2, then store in cache */
        matmul_batch(s->xb2, s->xb, w->wk + l * dim * dim, dim, dim, B);
        for (int b = 0; b < B; b++) {
            int pos = positions[b];
            for (int d = 0; d < dim; d++) {
                s->key_cache[loff + pos * dim + d] = s->xb2[d * B + b];
            }
        }

        /* 2d. V projection -> xb2, then store in cache */
        matmul_batch(s->xb2, s->xb, w->wv + l * dim * dim, dim, dim, B);
        for (int b = 0; b < B; b++) {
            int pos = positions[b];
            for (int d = 0; d < dim; d++) {
                s->value_cache[loff + pos * dim + d] = s->xb2[d * B + b];
            }
        }

        /* 2e. RoPE positional encoding: per-token */
        for (int b = 0; b < B; b++) {
            int pos = positions[b];
            for (int i = 0; i < dim; i += 2) {
                int head_dim = i % head_size;
                TYPE freq = 1.0 / pow(10000.0, (TYPE)head_dim / (TYPE)head_size);
                TYPE val = pos * freq;
                TYPE fcr = cos(val);
                TYPE fci = sin(val);
                /* Rotate Q (feature-major) */
                TYPE q0 = s->q[i * B + b];
                TYPE q1 = s->q[(i + 1) * B + b];
                s->q[i * B + b]       = q0 * fcr - q1 * fci;
                s->q[(i + 1) * B + b] = q0 * fci + q1 * fcr;
                /* Rotate K in cache (contiguous per position) */
                TYPE k0 = s->key_cache[loff + pos * dim + i];
                TYPE k1 = s->key_cache[loff + pos * dim + i + 1];
                s->key_cache[loff + pos * dim + i]     = k0 * fcr - k1 * fci;
                s->key_cache[loff + pos * dim + i + 1] = k0 * fci + k1 * fcr;
            }
        }

        /* 2f. Multi-head attention: per-token, causal */
        for (int b = 0; b < B; b++) {
            int pos = positions[b];
            for (int h = 0; h < p->n_heads; h++) {
                TYPE *att = s->att + h * p->seq_len;

                /* Attention scores: Q[h,b] . K[h,t] for t=0..pos */
                for (int t = 0; t <= pos; t++) {
                    TYPE score = 0.0;
                    for (int i = 0; i < head_size; i++) {
                        score += s->q[(h * head_size + i) * B + b]
                               * s->key_cache[loff + t * dim + h * head_size + i];
                    }
                    score /= sqrt((TYPE)head_size);
                    att[t] = score;
                }

                softmax(att, pos + 1);

                /* Weighted sum of values -> xb[h,b] */
                for (int i = 0; i < head_size; i++) {
                    s->xb[(h * head_size + i) * B + b] = 0.0;
                }
                for (int t = 0; t <= pos; t++) {
                    TYPE a = att[t];
                    for (int i = 0; i < head_size; i++) {
                        s->xb[(h * head_size + i) * B + b] +=
                            a * s->value_cache[loff + t * dim + h * head_size + i];
                    }
                }
            }
        }

        /* 2g. Wo projection (accelerated) */
        matmul_batch(s->xb2, s->xb, w->wo + l * dim * dim, dim, dim, B);

        /* 2h. Residual connection */
        for (int i = 0; i < dim * B; i++) {
            x[i] += s->xb2[i];
        }

        /* 2i. FFN RMSNorm: per-token */
        for (int b = 0; b < B; b++) {
            TYPE ss = 0.0;
            for (int j = 0; j < dim; j++) {
                TYPE val = x[j * B + b];
                ss += val * val;
            }
            ss = 1.0 / sqrt(ss / dim + 1e-5);
            for (int j = 0; j < dim; j++) {
                s->xb[j * B + b] = w->rms_ffn_weight[l * dim + j] * (ss * x[j * B + b]);
            }
        }

        /* 2j. FFN w1 and w3 projections (accelerated) */
        matmul_batch(s->hb, s->xb, w->w1 + l * dim * hidden_dim, dim, hidden_dim, B);
        matmul_batch(s->hb2, s->xb, w->w3 + l * dim * hidden_dim, dim, hidden_dim, B);

        /* 2k. SwiGLU: SiLU(w1(x)) * w3(x) - element-wise over hidden_dim*B */
        for (int i = 0; i < hidden_dim * B; i++) {
            TYPE val = s->hb[i];
            val *= 1.0 / (1.0 + exp(-val)); /* SiLU */
            val *= s->hb2[i];               /* gate with w3 */
            s->hb[i] = val;
        }

        /* 2l. w2 projection (accelerated) */
        matmul_batch(s->xb, s->hb, w->w2 + l * hidden_dim * dim, hidden_dim, dim, B);

        /* 2m. Residual connection */
        for (int i = 0; i < dim * B; i++) {
            x[i] += s->xb[i];
        }
    }

    /* 3. Final RMSNorm: per-token */
    for (int b = 0; b < B; b++) {
        TYPE ss = 0.0;
        for (int j = 0; j < dim; j++) {
            TYPE val = x[j * B + b];
            ss += val * val;
        }
        ss = 1.0 / sqrt(ss / dim + 1e-5);
        for (int j = 0; j < dim; j++) {
            x[j * B + b] = w->rms_final_weight[j] * (ss * x[j * B + b]);
        }
    }

    /* 4. Classifier (accelerated) */
    matmul_batch(s->logits, x, w->wcls, dim, p->vocab_size, B);

    return s->logits;
}

#ifdef __cplusplus
}
#endif

#endif /* LLAMA2_H */
