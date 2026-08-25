#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../../common/m5ops.h"
#include "../gemm_clstr_hw_defines.h"
#include "bench.h"

gemm_struct ges;
volatile int stage;

volatile uint8_t *top = (uint8_t *)(TOP + 0x00);
volatile uint32_t *val_a = (uint32_t *)(TOP + 0x01);
volatile uint32_t *val_b = (uint32_t *)(TOP + 0x09);
volatile uint32_t *val_c = (uint32_t *)(TOP + 0x11);

int __attribute__((optimize("0")))
main(void)
{
    m5_reset_stats();
    uint32_t base = 0x80c00000;
    TYPE *m1 = (TYPE *)base;
    TYPE *m2 = (TYPE *)(base + 8 * ROW * COL);
    TYPE *m3 = (TYPE *)(base + 16 * ROW * COL);
    TYPE *check = (TYPE *)(base + 24 * ROW * COL);
    int row_size = ROW;
    int col_size = COL;
    stage = 0;

    ges.a = m1;
    ges.b = m2;
    ges.c = m3;
    ges.row_size = row_size;
    ges.col_size = col_size;

    printf("Generating data\n");
    genData(&ges);
    printf("Data generated\n");

    *val_a = (uint32_t)(void *)m1;
    *val_b = (uint32_t)(void *)m2;
    *val_c = (uint32_t)(void *)m3;
    *top = 0x01;
    while (stage < 1) {
        asm volatile("wfi");
    }

    printf("Job complete\n");

    // ReLU activation on CPU: clamp negatives to 0
    printf("Applying ReLU activation\n");
    for (int idx = 0; idx < ROW * COL; idx++) {
        if (m3[idx] < 0) {
            m3[idx] = 0;
        }
    }
    printf("Activation complete\n");

#ifdef CHECK
    printf("Checking result\n");
    printf("Running bench on CPU\n");
    bool fail = false;
    int i, j, k, k_col, i_col;
    TYPE sum = 0;
    TYPE mult = 0;
    for (i = 0; i < ROW; i++) {
        for (j = 0; j < COL; j++) {
            i_col = i * COL;
            sum = 0;
            for (k = 0; k < ROW; k++) {
                k_col = k * COL;
                mult = m1[i_col + k] * m2[k_col + j];
                sum += mult;
            }
            check[i_col + j] = sum;
        }
    }
    printf("Comparing CPU run to accelerated run\n");
    for (i = 0; i < ROW * COL; i++) {
        if (m3[i] != check[i]) {
            printf("Mismatch at index %d\n", i);
            fail = true;
            break;
        }
    }
    if (fail) {
        printf("Check Failed\n");
    } else {
        printf("Check Passed\n");
    }
#endif
    m5_dump_stats();
    m5_exit();
}
