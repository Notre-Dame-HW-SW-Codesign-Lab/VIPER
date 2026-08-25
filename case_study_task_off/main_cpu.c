// CPU baseline for the VIPER UPMEM case study.
//   main_cpu      : full forward pass                    -> T_full
//   main_cpu_stub : matmuls replaced by memset (-DSTUB)  -> T_stub = T_CPU,rest
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "layer.h"

static void matmul_stub(q16 *out, const q16 *in, int mm_id) {
    (void)in;
    memset(out, 0, (size_t)SEQ_LEN * MM[mm_id].out_dim * sizeof(q16));
}

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e3 + ts.tv_nsec * 1e-6;
}

static int cmp_d(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

int main(int argc, char **argv) {
    int iters = (argc > 1) ? atoi(argv[1]) : 200;
    int warmup = 10;
#ifdef STUB
    matmul_fn mm = matmul_stub;
    const char *tag = "stub";
#else
    matmul_fn mm = matmul_cpu;
    const char *tag = "full";
#endif
    static q16 x0[SEQ_LEN * DIM], x[SEQ_LEN * DIM], logits[SEQ_LEN * VOCAB];
    init_params();
    init_input(x0);

    double *t = malloc(iters * sizeof(double));
    int64_t checksum = 0;
    for (int it = -warmup; it < iters; it++) {
        memcpy(x, x0, sizeof(x));
        double t0 = now_ms();
        forward(x, logits, mm);
        double t1 = now_ms();
        if (it >= 0) t[it] = t1 - t0;
        checksum += logits[(it & 63) * VOCAB + (it & 255)];
    }
    qsort(t, iters, sizeof(double), cmp_d);
    double sum = 0;
    for (int i = 0; i < iters; i++) sum += t[i];
    printf("variant=%s iters=%d mean_ms=%.4f median_ms=%.4f min_ms=%.4f checksum=%lld\n",
           tag, iters, sum / iters, t[iters / 2], t[0], (long long)checksum);
    return 0;
}
