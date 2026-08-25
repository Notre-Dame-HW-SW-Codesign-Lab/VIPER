# Batched Matmul for Llama2 Accelerator Sweep

## Problem

The original llama2 benchmark uses **matrix-vector** products: `W(d,n) x x(n,1)`. The SALAM accelerator always computes a full `ROW x ROW` matrix multiply, but only column 0 of m2 has data -- columns 1..ROW-1 are zeros. This means larger tiles waste more computation (waste factor = ROW), making bigger accelerators **slower**.

### Old Results: Matrix-Vector (B=1, 3 tokens)

| SIZE | simSeconds | Interrupts | Speedup vs SIZE=8 |
|------|-----------|------------|-------------------|
| 8    | 0.2897    | 9,216      | 1.00x             |
| 16   | 0.2265    | 2,304      | 1.28x             |
| 32   | 0.2643    | 576        | 1.10x             |
| 64   | 0.3522    | 144        | 0.82x             |
| 128  | 0.5392    | 36         | 0.54x             |

Scaling is inverted -- SIZE=16 wins, SIZE=128 is worst.

## Fix: Batch Prefill

Switch from single-token inference to **batch prefill** -- process B=128 tokens simultaneously. The matmul becomes `W(d,n) x X(n,B)`, filling B columns of m2 with real data. When B >= ROW, every column does useful work and the only advantage of larger tiles is fewer accelerator calls (less per-call overhead).

### Expected Scaling After Batching

For Q matmul W(128x128) x X(128x128) with B=128:

| SIZE | d_tiles | n_tiles | b_tiles | Total calls |
|------|---------|---------|---------|-------------|
| 8    | 16      | 16      | 16      | 4,096       |
| 16   | 8       | 8       | 8       | 512         |
| 32   | 4       | 4       | 4       | 64          |
| 64   | 2       | 2       | 2       | 8           |
| 128  | 1       | 1       | 1       | 1           |

Each doubling of SIZE -> 8x fewer calls. All sizes have 100% column utilization.

## Implementation Steps

### Step 1: Modify `llama2/sw/llama2.h`

**a) `matmul_salam_batch()` (replaces `matmul_salam`)**

Fills b_tile columns of m2 (not just column 0), extracts d_tile x b_tile result from m3.

```c
static void matmul_salam_batch(TYPE *tile_out, TYPE *X, TYPE *W,
                                int n_tile, int d_tile, int b_tile,
                                int w_stride, int x_stride)
```

Key change -- m2 fill:
```
for k in 0..n_tile-1:
    for j in 0..b_tile-1:
        m2[k * ROW + j] = X[k * x_stride + j]
    for j in b_tile..ROW-1:
        m2[k * ROW + j] = 0.0    // zero-pad remaining columns
```

Key change -- m3 extract:
```
for i in 0..d_tile-1:
    for j in 0..b_tile-1:
        tile_out[i * b_tile + j] = m3[i * ROW + j]
```

**b) `matmul_batch()` (replaces `matmul`)**

Triple-nested tiling over d, n, and B dimensions:

```c
static void matmul_batch(TYPE *xout, TYPE *X, TYPE *W, int n, int d, int B)
```

- Computes `Xout(d,B) = W(d,n) x X(n,B)`
- Feature-major layout: `buf[feature * B + token]`
- Accumulates partial sums across n-tiles

**c) `forward_batch()` (replaces `forward`)**

```c
static TYPE *forward_batch(Transformer *transformer, int *tokens, int *positions, int B)
```

Flow:
1. Embed all B tokens -> x(dim, B)
2. Per layer:
   - RMSNorm (per-token CPU loop)
   - Q = matmul_batch(xb, Wq) -- accelerated
   - K = matmul_batch(xb, Wk) -> store in cache -- accelerated
   - V = matmul_batch(xb, Wv) -> store in cache -- accelerated
   - RoPE (per-token CPU loop)
   - Attention: per-token causal (token b attends to 0..b)
   - Wo projection -- accelerated
   - Residual connection
   - FFN RMSNorm (per-token CPU loop)
   - w1, w3 projections -- accelerated
   - SwiGLU (element-wise over hidden_dim x B)
   - w2 projection -- accelerated
   - Residual connection
3. Final RMSNorm (per-token)
4. Classifier matmul_batch -- accelerated

**d) `malloc_run_state()` -- scale buffers by B**

| Buffer          | Old Size       | New Size (B=128) |
|-----------------|---------------|------------------|
| x, xb, xb2, q  | dim           | dim * B          |
| hb, hb2         | hidden_dim    | hidden_dim * B   |
| logits          | vocab_size    | vocab_size * B   |
| att             | n_heads * seq_len | unchanged    |
| key/value cache | unchanged     | unchanged        |

**e) Removed `rmsnorm()`** -- inlined into per-token loops with strided access.

### Step 2: Modify `llama2/sw/main.cpp`

- `seq_len = 128` (was 16)
- Replace 3-token autoregressive loop with single `forward_batch()` call
- Pass B=seq_len token IDs (0..127) and positions (0..127)
- Print logits for last token

### Step 3: Increase heap in `boot.ld`

Old heap was 512KB (0x80000) -- too small for batched buffers (~3.3MB total).

Changed to **4MB** (0x400000):

```
. = . + 0x400000; /* 4MB heap room for batched buffers */
```

### Step 4: No HW changes

`gemm.c`, `top.c`, `hw_defines.h`, `config.yml`, `defines.h`, `boot.s` -- all unchanged. The HW already computes the full ROW x ROW matmul; we're just filling m2 with useful data.

## Files Modified

All paths relative to `configs/example/gem5_library/salam-benchmarks/src/llama2/`:

| File           | Change                                       |
|----------------|----------------------------------------------|
| `sw/llama2.h`  | Replaced matmul/forward with batched versions |
| `sw/main.cpp`  | seq_len=128, single forward_batch() call      |
| `sw/boot.ld`   | Heap 512KB -> 4MB                            |

## Staging Area Memory Layout (0x80C00000)

| Region   | Offset           | Size (bytes)       |
|----------|------------------|--------------------|
| m1       | 0                | 8 * ROW * COL      |
| m2       | 8 * ROW * COL    | 8 * ROW * COL      |
| m3       | 16 * ROW * COL   | 8 * ROW * COL      |
| tile_out | 24 * ROW * COL   | 8 * ROW * COL      |

Max total (SIZE=128): 4 x 128KB = 512KB < 1MB non-cacheable page.

## Backup

Old (matrix-vector) code and results are preserved:

```
src/llama2_8_not_batch/
src/llama2_16_not_batch/
src/llama2_32_not_batch/
src/llama2_64_not_batch/
src/llama2_128_not_batch/
```

## Build and Run

The sweep script handles everything (copy, patch defines, build, submit):

```bash
./sweep_llama2_sizes.sh
```

Or submit individually:

```bash
qsub submit_llama2_128.sh
```

`run_system.sh` builds automatically before running (`BUILD=True` by default).

## Verification

1. Check `system.terminal` for "Inference Complete" and non-zero logits
2. Verify no "malloc_run_state failed!" message
3. Count interrupts -- should scale with tile count
4. Collect results: `./collect_llama2_sweep.sh`
5. Expect: larger accelerators have lower simSeconds (inverted from old results)
6. All sizes should produce identical logits (correctness check)

## Debugging: First Run Issue

The first batched run (llama2_128) failed with:
- `malloc_run_state failed!` -- heap too small
- All-zero logits -- consequence of NULL buffers

Fix: increased heap from 512KB to 4MB in `boot.ld`.
