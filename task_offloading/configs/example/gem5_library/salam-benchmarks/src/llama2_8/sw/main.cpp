#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include "../../common/m5ops.h"
#include "../gemm_clstr_hw_defines.h"
#include "llama2.h"

volatile int stage;

volatile uint8_t  *top_reg = (uint8_t  *)(TOP + 0x00);
volatile uint32_t *val_a   = (uint32_t *)(TOP + 0x01);
volatile uint32_t *val_b   = (uint32_t *)(TOP + 0x09);
volatile uint32_t *val_c   = (uint32_t *)(TOP + 0x11);

int __attribute__((optimize("0")))
main(void)
{
    m5_reset_stats();
    stage = 0;

    printf("Starting Llama2 batched inference with SALAM acceleration\n");

    Transformer transformer;
    transformer.config.dim = 128;
    transformer.config.hidden_dim = 256;
    transformer.config.n_layers = 1;
    transformer.config.n_heads = 8;
    transformer.config.vocab_size = 256;
    transformer.config.seq_len = 128;

    int B = transformer.config.seq_len;

    printf("Config: dim=%d, hidden_dim=%d, n_layers=%d, vocab=%d, B=%d\n",
           transformer.config.dim, transformer.config.hidden_dim,
           transformer.config.n_layers, transformer.config.vocab_size, B);

    malloc_run_state(&transformer.state, &transformer.config);
    printf("Allocated run state\n");

    init_weights_synthetic(&transformer.weights, &transformer.config);
    printf("Initialized synthetic weights\n");

    /* Prepare token IDs and positions for batch prefill */
    int tokens[128], positions[128];
    for (int i = 0; i < B; i++) {
        tokens[i] = i % transformer.config.vocab_size;
        positions[i] = i;
    }

    printf("\n=== Running Batched Transformer Forward Pass (B=%d) ===\n", B);

    TYPE *logits = forward_batch(&transformer, tokens, positions, B);

    /* Print logits for last token */
    int last = B - 1;
    printf("\nToken %d logits: ", last);
    for (int j = 0; j < 5 && j < transformer.config.vocab_size; j++) {
        int val_int = (int)(logits[j * B + last] * 1000);
        printf("%d ", val_int);
    }
    printf("(x1000)\n");

    /* Find argmax for last token */
    int max_idx = 0;
    TYPE max_val = logits[0 * B + last];
    for (int j = 1; j < transformer.config.vocab_size; j++) {
        if (logits[j * B + last] > max_val) {
            max_val = logits[j * B + last];
            max_idx = j;
        }
    }
    int max_int = (int)(max_val * 1000);
    printf("Predicted next token: %d (logit: %d x1000)\n", max_idx, max_int);

    printf("\n=== Inference Complete ===\n");

    free_run_state(&transformer.state);
    free_weights(&transformer.weights);
    printf("Cleaned up\n");

    m5_dump_stats();
    m5_exit();
    return 0;
}
