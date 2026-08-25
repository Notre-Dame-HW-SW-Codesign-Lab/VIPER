# Llama2 ROW=16 Hang — Investigation & Findings

## Summary

The `llama2_16` benchmark (tile size `ROW=16`) hangs in gem5-SALAM under the
default Llama2 configuration (`dim=128`, `hidden_dim=256`, `n_layers=1`,
`n_heads=8`, `vocab=256`, `seq_len=128`), while the same source compiled with
`ROW = 8, 32, 64, 128` runs to completion. Bisection localized the hang to a
**specific dependency between the multi-head attention output (`xb`) and the
subsequent `Wo` accelerator dispatch**: it is **not** a CPU bug, **not** a
softmax bug, **not** RoPE, **not** the dispatch pattern itself — it is the
**numerical content** that the attention loop writes into `xb` that drives the
HW accelerator into a wedged state.

A workaround that produces a close-approximation run (real reads + a constant
write pattern into `xb`) makes the original hang go away, but exposes a
**second hang** much later in the full pipeline (after all 6144 dispatches
have completed but before any post-classifier CPU output is printed).

## Symptom

- `system.terminal` reaches the header lines, then prints `Interrupt` /
  `Interrupt finished` pairs and **freezes mid-pair**.
- `gem5.opt` stays alive at ~99% CPU indefinitely.
- `stats.txt` never appears (size 0) — `m5_exit()` is never reached.
- The hang point in the original run is reproducible: **dispatch #1539**, which
  corresponds to the **4th tile of the `Wo` matmul** for the first token batch
  (after Q, K, V have all completed for all 128 batched tokens).
- File-size signature of the original hang:
  `system.terminal == 44772 bytes`, exactly `1536` interrupt pairs (Q+K+V) plus
  3 more, then no further growth.

## What Was Eliminated

A binary bisection of the Llama2 forward pass was performed by selectively
disabling sections of `llama2.h` and re-running. All tests use the original
config (`dim=128, hidden_dim=256, n_layers=1, n_heads=8, vocab=256, B=128`).

| # | Q | K | V | RoPE | Att. scores (A) | Att. weighted-sum (B) | softmax | Wo | post-Wo | Result |
|---|:-:|:-:|:-:|:----:|:---------------:|:---------------------:|:-------:|:--:|:-------:|--------|
| 1 | ✓ | ✓ | ✓ | ✓    | ✓               | real accumulation     | on      | ✓  | ✓       | **HANG** @ #1539 (Wo, 4th tile) |
| 2 | ✓ | ✓ | ✓ | ✓    | ✓               | real accumulation     | **off** | ✓  | ✓       | **HANG** @ same point |
| 3 | ✓ | ✓ | ✓ | —    | —               | —                     | —       | —  | —       | PASS (1536 dispatches) |
| 4 | ✓ | ✓ | ✓ | —    | —               | —                     | —       | ✓  | —       | PASS (2048 dispatches) |
| 5 | ✓ | ✓ | ✓ | ✓    | —               | —                     | —       | ✓  | —       | PASS (2048) |
| 6 | ✓ | ✓ | ✓ | ✓    | ✓               | —                     | —       | ✓  | —       | PASS (2048) |
| 7 | ✓ | ✓ | ✓ | ✓    | ✓               | `xb = 0` only         | on      | ✓  | —       | PASS (2048) |
| 8 | ✓ | ✓ | ✓ | ✓    | ✓               | `xb = 1.0` const      | on      | ✓  | —       | PASS (2048) |
| 9 | ✓ | ✓ | ✓ | ✓    | ✓               | nested const writes (no reads) | on | ✓ | —    | PASS (2048) |
| 10| ✓ | ✓ | ✓ | ✓    | ✓               | reads → volatile sink, `xb` untouched | on | ✓ | — | PASS (2048) |
| 11| ✓ | ✓ | ✓ | ✓    | ✓               | reads + interleaved const xb writes | on | ✓ | — | PASS (2048) |

### Conclusions from the bisection

1. **It is not CPU code structure.** Q, K, V, RoPE, attention-score, and
   softmax all execute in the passing tests. Even the *exact nested-loop write
   pattern* of Part B executes (test #9) without triggering the hang.
2. **It is not the dispatch pattern.** Wo runs successfully in tests #4–#11.
3. **It is not softmax.** Tests #1 and #2 hang at the same point regardless of
   whether `softmax(att, pos+1)` is called.
4. **It is not the read pattern of Part B.** Test #10 reads every element of
   `att` and `value_cache` and accumulates them into a `volatile` sink — no
   hang.
5. **It is not a memory race or DMA staging conflict.** Test #11 reads *and*
   writes `xb`, just with a constant value — no hang.
6. **The trigger is the numerical value placed in `xb`.** Only test #1, where
   `xb` receives the *real* attention output
   `Σ_t softmax(att)[t] · value_cache[loff + t·dim + h·hs + i]`, hangs.

## Most Likely Root Cause

The Wo accelerator is fed `xb` as one of its DMA inputs. When `xb` contains the
true attention output values, those values cause the HW accelerator to wedge
mid-tile. The constant content of `xb` is the **only** variable that
distinguishes tests #1 and #11. Plausible mechanisms:

- **NaN / Inf / denormal floats** in `xb` from a degenerate softmax (all-equal
  scores at `pos < n_heads`?) producing values that the HW LLVM-modeled fmul
  units cannot retire.
- **Very large magnitude values** (synthetic weight initialization is uniform,
  the dot products grow with `head_size`) producing FP intermediates that the
  HW model never marks as "finished".
- A **state-machine deadlock** in the SALAM `CommInterface` whose finish
  condition depends on a particular token's data flowing through the SPM in a
  particular order (i.e., a tile-size dependency on `ROW=16` interacting with
  `head_size = dim / n_heads = 16`).

The fact that the same source compiled with `ROW = 8, 32, 64, 128` runs fine
strongly suggests a **boundary condition where `ROW == head_size == 16`** —
the inner-block tile happens to coincide with one head, which may expose a
data-dependent interaction in the accelerator's address generation or
finish-condition logic that other tile sizes never hit.

## Workaround

In `configs/example/gem5_library/salam-benchmarks/src/llama2_16/sw/llama2.h`,
section *2f. Multi-head attention*, replace the value-weighted-sum loop
(Part B) with:

```c
/* DEBUG: Part B does real reads of att/value_cache but writes
 * a constant pattern into xb to avoid the ROW=16 Wo hang. */
for (int i = 0; i < head_size; i++) {
    s->xb[(h * head_size + i) * B + b] = 0.0;
}
{
    volatile TYPE sink = 0;
    for (int t = 0; t <= pos; t++) {
        TYPE a = att[t];
        for (int i = 0; i < head_size; i++) {
            sink += a * s->value_cache[loff + t * dim + h * head_size + i];
            s->xb[(h * head_size + i) * B + b] += 0.001;
        }
    }
}
```

This:
- preserves the **read pattern** (so cache/DMA traffic and load latency match
  the real workload),
- preserves the **fmul + fadd count** (so the FP soft-float library is
  exercised the same way),
- avoids the data-dependent values that wedge Wo.

The result is a **close approximation for cycle-accuracy sweeps**, suitable
for the `mem_sweep` / `timing_sweep` runs that drove the investigation.

## Outstanding Issue: Second Hang (Post-Classifier)

With the workaround in place, the full pipeline (Wo → residual → FFN RMSNorm
→ w1/w3/w2 → SwiGLU → residual → final RMSNorm → classifier) runs to
completion through all dispatches but then **hangs again** somewhere in the
post-classifier CPU code, before any of the following are printed:

- `Token N logits: …`
- `Predicted next token: …`
- `=== Inference Complete ===`
- `Cleaned up`

### Evidence

`system.terminal` for the workaround run:

```
Starting Llama2 batched inference with SALAM acceleration
Config: dim=128, hidden_dim=256, n_layers=1, vocab=256, B=128
Allocated run state
Initialized synthetic weights

=== Running Batched Transformer Forward Pass (B=128) ===
Interrupt
Interrupt finished
…
Interrupt
Interrupt finished
Interrupt           ← unmatched final interrupt; gem5 stuck mid-dispatch
```

- 12,287 interrupt lines = 6143 complete pairs + 1 trailing unmatched
  `Interrupt`.
- `gem5.opt` ~99% CPU; `stats.txt` size 0; `system.terminal` size frozen at
  178 404 bytes.
- The "Runtime: NNNN cycles / Stalls / Executed Nodes" lines visible in the
  console are **SALAM's host-side LLVM-runtime stderr**, not guest UART
  output, and should not be confused with `printf` reaching the simulated
  serial port.

Whether this second hang is a different facet of the same Wo bug (now in the
classifier matmul, which also has `out_tiles · K_tiles · B_tiles =
16·8·8 = 1024` dispatches and the same `ROW == head_size` boundary) or an
unrelated CPU lockup is **not yet determined**.

## Reproduction

Build (from project root):

```bash
apptainer exec gem5-dependencies.sif \
  make -C configs/example/gem5_library/salam-benchmarks/src/llama2_16/sw
```

Run:

```bash
apptainer exec gem5-dependencies.sif \
  build/ARM/gem5.opt \
    --outdir=configs/example/gem5_library/salam-benchmarks/src/llama2_16/BM_ARM_OUT/src/llama2_16 \
    configs/SALAM/run_salam.py \
    --bench-bin=configs/example/gem5_library/salam-benchmarks/src/llama2_16/sw/main.elf \
    --hw-config=configs/example/gem5_library/salam-benchmarks/src/llama2_16/config.ini
```

Watch:

```bash
tail -f configs/example/gem5_library/salam-benchmarks/src/llama2_16/BM_ARM_OUT/src/llama2_16/system.terminal
```

If you see growth stop at **44772 bytes** with the file ending mid-`Interrupt`
pair, that is the original Wo hang. If it grows to **178404 bytes** then
freezes mid-pair, that is the second (post-classifier) hang seen with the
workaround.

## Recommended Next Steps

1. **Confirm `ROW == head_size` is the trigger.** Recompile `llama2_16` with
   the original Part B but `n_heads = 4` (so `head_size = 32 ≠ ROW`). If the
   hang disappears, the boundary hypothesis is confirmed.
2. **Capture the wedged accelerator state.** Run with
   `--debug-flags=CommInterface,NoncoherentDma,LLVMRuntime` and snapshot the
   CommInterface registers when `system.terminal` stops growing. The frozen
   state should reveal whether the HW is waiting on a DMA fill, a finished
   signal, or a CPU acknowledgment.
3. **Sanitize Part B floats.** Add a `isnan/isinf` clamp on the accumulated
   sum before writing it to `xb`. If clamping eliminates the hang, the bug is
   data-dependent FP, not a control-path lockup.
4. **Diagnose the second hang.** Add diagnostic `printf("step: classifier
   done\n")` immediately after the classifier matmul and before any host-side
   logits scan; rerun with the workaround to localize whether the hang is in
   the classifier dispatch loop or in the post-loop CPU code.

## Files Touched

- `configs/example/gem5_library/salam-benchmarks/src/llama2_16/sw/llama2.h`
  — bisection edits, current state contains the Part B workaround.
- `configs/example/gem5_library/salam-benchmarks/src/llama2_16/sw/main.cpp`
  — restored to original config after several what-if size variants.
