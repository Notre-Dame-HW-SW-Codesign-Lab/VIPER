# SALAM Cycle-Stats Export — Deferred Patch

## Status

**Deferred.** This document captures *what* the patch should do, *why* it
matters, and *what we did instead* in the meantime, so we can come back to
it later without re-deriving the analysis.

## Background

The VIPER iron-law decomposition for the PIM side is

```
CPI_PIM,total = CPI_PIM,exec + m_PIM × AMAT_PIM
```

with `m_PIM = 1` by construction (because `I_PIM` already counts memops).
To validate the decomposition we need, per row, two numbers:

- `CPI_PIM,exec`  — compute-floor CPI (cycles per memop when memory is free)
- `AMAT_PIM`      — average memory access time (cycles per memop)

Today neither of these is exposed by `stats.txt`. The PIM side of
`stats.txt` only contains:

- `system.gemm_clstr.matrix{1,2}.numReads::total`,
  `matrix3.numWrites::total` — gives `I_PIM` (count, not timing)
- `system.gemm_clstr.gemm.power_state.pwrStateResidencyTicks` — gives
  alive vs idle, **not** busy vs stalled
- bandwidth/occupancy on `coherency_bus` and on the matrix scratchpads —
  no direct CPI decomposition

So a *single* run yields one equation (total PIM time from
`system.cpu.quiesceCycles`) and two unknowns. The current
`compute_predictions.py` works around this by **fitting** across the
`m_config` sweep:

- 35 `(CPI_PIM,exec, dAMAT/dm, m_knee)` triples (one per `(size, c)` slice)
- 105 calibration parameters total over 280 sweep points
- in-sample p95 ≈ 7%, leave-one-out p95 ≈ 5%

This works, but the cost is high:

1. **It needs the whole sweep.** A single run cannot decompose itself.
2. **Parameters don't transfer across slices.** Leave-one-slice-out
   gives p95 ≈ 30% with a 89% max — the per-slice fit is purely empirical,
   not mechanistic.
3. **The split is numerical, not physical.** We cannot point at a row and
   say "60% compute, 40% memory stall" with an in-simulator source.

## What SALAM Already Has

`src/salam/HWModeling/hw_statistics.hh:62` defines a per-cycle struct:

```cpp
struct HW_Cycle_Stats {
    int cycle;
    int loadInFlight;  int loadActive;  int loadRawStall;
    int storeInFlight; int storeActive;
    int compInFlight;  int compLaunched; int compActive;
    int compFUStall;   int compCommited;
    // ...
};
```

These fields are **already populated every cycle** by
`LLVMInterface::ActiveFunction::processQueues()` in
`src/salam/llvm_interface.cc:120-340`:

| line | field |
|---|---|
| `:339` | `compActive++` — a compute FU made progress |
| `:241` | `loadRawStall++` — compute is waiting on a memop |
| `:166` | `compFUStall++` — structural FU stall |

The trouble is the struct is `reset()`-ed at the top of every cycle
(`:126`) and **never aggregated** across cycles. The existing
`HWStatistics::print()` in `hw_statistics.cc` just dumps the per-cycle
buffer to `std::cout` for debug, never summing it up and never registering
anything as a `statistics::Scalar`.

There are **zero** `statistics::Scalar` registrations anywhere under
`src/salam/`. SALAM is not yet wired into gem5's native stats database.

## The Patch (When We Do It)

**Goal:** add three persistent counters on `LLVMInterface` that aggregate
the existing per-cycle classification, and emit them at end-of-run.

### Form 1 — printf at end of run (smallest, ~9 lines)

#### `src/salam/llvm_interface.hh`

```cpp
class LLVMInterface : public AccComputeUnit {
  // existing private members ...
  uint64_t busyCycles    = 0;
  uint64_t memStallCycles = 0;
  uint64_t totalCycles   = 0;
};
```

#### `src/salam/llvm_interface.cc`

At the bottom of `ActiveFunction::processQueues()` (after line ~340 where
`compActive` etc. are incremented):

```cpp
owner->totalCycles++;
if (hw_cycle_stats.compActive > 0) {
    owner->busyCycles++;
} else if (hw_cycle_stats.loadRawStall > 0 ||
           hw_cycle_stats.compFUStall  > 0) {
    owner->memStallCycles++;
}
```

Inside `printResults()` (anywhere — sits next to the existing
`std::cout << "Total Area: ..."` block at `:945`):

```cpp
std::cout << "VIPER_STATS"
          << " busy="  << busyCycles
          << " mem="   << memStallCycles
          << " total=" << totalCycles
          << std::endl;
```

That's the entire patch. The line lands in gem5's host stdout (the
`simout` file or wherever `run_system.sh` / SLURM redirects it — **not**
in `system.terminal`, which only sees UART output from the simulated ARM
CPU).

### Form 2 — `regStats()` (cleaner, ~35 lines)

Same logic, but register the three counters as `statistics::Scalar` so
they appear in `stats.txt` rather than stdout. Requires confirming
`AccComputeUnit::regStats()` exists or implementing one. Strictly
"nicer", but Form 1 is enough for the data we need.

### Python side (~10 lines)

In `compute_predictions.py`, replace the entire hinge-fit section with:

```python
def read_viper_stats(run_dir):
    with open(os.path.join(run_dir, "simout")) as fh:
        for line in fh:
            m = re.search(
                r"VIPER_STATS busy=(\d+) mem=(\d+) total=(\d+)", line
            )
            if m:
                return tuple(int(x) for x in m.groups())
    return None

# per row:
busy, mem, tot = read_viper_stats(run_dir)
cpi_exec  = busy / I_PIM
amat_pim  = mem  / I_PIM
T_pim     = tot * T_PIM_CLK
```

After the patch, the iron-law decomposition reproduces every row
**exactly by construction**, with two *measured* numbers instead of
105 fitted ones.

## Why We're Deferring It

Two costs:

1. **Rebuild gem5.** Patching SimObject C++ requires a full scons rebuild
   inside the apptainer container.
2. **Re-run the entire sweep.** All 280 stats files need to be regenerated
   to pick up the new counters. The runs are not cheap.

Neither cost is huge, but neither is zero, and we have a working
alternative (see next section) that needs **none** of it.

## What We're Doing Instead

For the validation pass we are currently writing up, we use the
**`m_config = 0` anchor** in `compute_predictions.py`:

At `m_config = 0`, by definition there is no exposed scratchpad latency,
so `AMAT_PIM = 0` and the iron-law collapses to

```
CPI_PIM,total(m=0) = CPI_PIM,exec    ← read directly from stats.txt
```

For any other `m_config`,

```
AMAT_PIM(size, c, m) = CPI_PIM,total(size, c, m) − CPI_PIM,exec(size, c)
```

Both terms come from the existing 280 `stats.txt` files. There is no
fitting, no rebuild, no rerun, and the decomposition reproduces every
row exactly. Cost: ~10 lines of Python.

The trade-off vs the SALAM patch:

| | `m=0` anchor (now) | SALAM `printResults()` patch (later) |
|---|---|---|
| C++ work | none | ~9 lines |
| Reruns | none | 280 |
| Per-row mechanistic split | no (numerical) | yes (busy vs stall measured) |
| Can extrapolate to unmeasured `m` | no | no (still per-row) |
| Validation argument | "every row reproduces itself by construction" | "every row reproduces itself by construction *and* the split is mechanistic" |

The SALAM patch is the right thing to do *if* we ever want to publish
attribution numbers (e.g., "this sweep point is 60% compute-bound"). The
`m=0` anchor is enough for the iron-law decomposition itself.

## Plumbing Probe Already in Place

To validate the *workflow* of "emit a tagged line at end-of-run, grep
it offline" without touching SALAM C++, a single integer-only printf
was added to the bare-metal main.cpp at
`configs/example/gem5_library/salam-benchmarks/src/llama2_16/sw/main.cpp`,
right before `m5_dump_stats()`:

```cpp
/* VIPER end-of-run marker — integer-only print so we don't trip the
 * size=16 soft-float printf bug. Greppable from system.terminal. */
printf("VIPER_MARK end_of_run max_idx=%d\n", max_idx);
```

This is **only a plumbing test**, not a data source. `max_idx` carries
no iron-law information. The point is to verify, with the cheapest
possible change, that:

1. Edits to `main.cpp` survive `make clean && make`
2. UART output reaches `BM_ARM_OUT/src/llama2_16/system.terminal`
3. A distinctive `VIPER_MARK` prefix can be greppped out

Once the plumbing is confirmed for the bare-metal side, the SALAM-side
patch is the same idea but lives one process up: `printResults()` runs
in the gem5 *host* process, so its output goes to the host stdout
(`simout` / SLURM `.o*` file), not `system.terminal`.

## Open Items For "Later"

When we come back to the SALAM patch:

- [ ] Confirm where `run_system.sh` and the SLURM submit scripts route
  gem5's host stdout, so we know which file the new `VIPER_STATS` line
  will land in for each sweep run.
- [ ] Decide whether the cycle classification rule (`compActive` ⇒ busy,
  else `loadRawStall || compFUStall` ⇒ stall, else idle) is the
  decomposition we want, or whether to split `dmaStallCycles` out of
  `loadRawStall` separately.
- [ ] Apply the ~9-line patch to `llvm_interface.{hh,cc}`.
- [ ] Rebuild gem5 inside the apptainer container.
- [ ] Re-run the 280-point sweep (or a subset for validation first).
- [ ] Replace the hinge-fit section of `compute_predictions.py` with
  the `read_viper_stats()` reader.
- [ ] Update `docs/iron_law_validation.md` to report the
  measurement-based numbers and retire the calibration parameter count.
- [ ] Optionally promote Form 1 (printf) to Form 2 (`regStats()`) so
  the numbers live in `stats.txt` instead of stdout.

## References

- `docs/iron_law_validation.md` — current validation results with the
  hinge fit
- `src/salam/HWModeling/hw_statistics.hh:62` — `HW_Cycle_Stats` struct
  with the fields we need (already populated, never aggregated)
- `src/salam/llvm_interface.cc:120-340` — per-cycle classification logic
  that already exists
- `src/salam/llvm_interface.cc:878` — `LLVMInterface::printResults()`,
  the function the new `std::cout` line would go into
- `configs/example/gem5_library/salam-benchmarks/src/llama2_16/sw/main.cpp` —
  bare-metal `VIPER_MARK` plumbing probe
- `compute_predictions.py` — current hinge-fit calibration script
  (to be replaced)
