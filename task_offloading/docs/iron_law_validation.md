# VIPER Model Validation (CPU + PIM)

## Goal

We have an analytical execution-time formula:

```
T = I_CPU × (CPI_CPU,exec + m_CPU × AMAT_CPU) × t_CPU
  + I_PIM × (CPI_PIM,exec + m_PIM × AMAT_PIM) × t_PIM,clk
```

The objective is **not** to predict execution time from first principles
(which would require building HW models). The objective is to verify that
the formula **can describe the real world**: i.e., for every parameter on
the right-hand side, there exists a value sourceable from `stats.txt` such
that `T_predicted ≈ T_simulated` across the entire 280-point sweep.

If the formula fits the data, it is a valid decomposition we can later
use to reason about which terms dominate, where to optimize, and how
to extrapolate.

## Formula Instantiation

For the gem5-SALAM Llama2 sweep:

| Symbol | CPU side | PIM side |
|---|---|---|
| `I` | `system.cpu.commitStats0.numInsts` (`simInsts`) | `matrix1.numReads + matrix2.numReads + matrix3.numWrites` |
| `t_clk` | `0.5 ns` (cpu_clk_domain = 2 GHz) | `1 ns` (clk_domain = 1 GHz) |
| `m × AMAT` | `(dmiss × dlat + imiss × ilat) / I_CPU` (cycles/inst) | `m_PIM × m` (slope × scratchpad latency in ns) |
| `CPI_exec` | calibrated, **one constant per `size`** | calibrated, **one constant per `(size, compute)`** |

### Stats.txt fields used

CPU side (one per simulated point):

```
simSeconds
simInsts
system.cpu.numCycles
system.cpu.quiesceCycles
system.cpu.cpi
system.cpu.dcache.demandMisses::total
system.cpu.dcache.demandAvgMissLatency::total
system.cpu.icache.demandMisses::total
system.cpu.icache.demandAvgMissLatency::total
```

PIM side (one per simulated point):

```
system.gemm_clstr.matrix1.numReads::total
system.gemm_clstr.matrix2.numReads::total
system.gemm_clstr.matrix3.numWrites::total
```

### Calibration

**CPU `CPI_exec(size)`** — fitted once per binary at `(size, c=5, m=0)`
as a residual:

```
CPI_exec(size) = system.cpu.cpi  -  cpi_mem_derived
```

It is invariant across the `m` sweep to 5+ decimal places, confirming the
CPU instruction stream is binary-determined and does not depend on the
accelerator's scratchpad latency.

| size | CPI_exec |
|---:|---:|
|   8 | 2.784973 |
|  16 | 1.841481 |
|  32 | 1.459439 |
|  64 | 1.266635 |
| 128 | 1.168027 |

The monotonic decrease with `size` is physically meaningful: larger ROW
amortizes more arithmetic per loop iteration, lowering the soft-float
overhead per FP op.

**PIM `(CPI_PIM,exec, m_PIM)`** — fitted as a 2-parameter linear regression
per `(size, compute)` over the 8 scratchpad-latency points:

```
CPI_PIM_total(m) = T_PIM_measured / (I_PIM × t_PIM,clk)
                 = a + b × m
                 = CPI_PIM,exec + m_PIM × m
```

where `T_PIM_measured = system.cpu.quiesceCycles × t_CPU`
(time the CPU spent quiesced waiting for the accelerator). This gives 35
`(a, b)` pairs total — one per `(size, compute)`.

Selected fits:

| size | compute | CPI_PIM,exec (a) | m_PIM (b) |
|---:|---:|---:|---:|
|   8 |    5 |    4.6966 | 0.255603 |
|   8 |   14 |    6.8326 | 0.233156 |
|   8 |   50 |   35.8175 | 0.030067 |
|   8 | 5000 | 3311.7063 | 0.018431 |
|  16 |    5 |    2.5431 | 0.246304 |
|  32 |    5 |    2.0258 | 0.243643 |
|  64 |    5 |    1.8476 | 0.242673 |
| 128 |    5 |    1.7689 | 0.242277 |
| 128 |   14 |    1.7705 | 0.242263 |
| 128 |   50 |    1.7834 | 0.242251 |
| 128 | 5000 |  196.1512 | 0.001029 |

The fits expose the regime structure cleanly:

- **Memory-bound region** (`size=128`, low `c`): `CPI_exec ≈ 1.77` is flat
  across `c = 5, 14, 50` — increasing the FMA latency does nothing because
  compute is already hidden under the memory traffic. Slope `m_PIM ≈ 0.242`
  is constant (each memory op pays ~24% of the SPM latency on the critical
  path).

- **Compute-bound region** (`size=128`, `c=5000`): `CPI_exec` jumps to 196,
  and `m_PIM` collapses to 0.001 — scratchpad latency becomes irrelevant
  because the FMA pipeline is saturated. The dataflow engine fully overlaps
  any outstanding memory traffic.

- **Small-tile region** (`size=8`, low `c`): `CPI_exec ≈ 5` (vs 1.77 at
  size=128) because the per-dispatch overhead is not amortized over enough
  ops. `m_PIM ≈ 0.256` is slightly higher because there is less DMA pipelining
  available with the smaller working set.

## Final Predictive Formula

```
T = I_CPU × (CPI_CPU,exec(size) + cpi_mem_CPU) × t_CPU
  + I_PIM × (CPI_PIM,exec(size, c) + m_PIM(size, c) × m) × t_PIM,clk

where:
  I_CPU            = simInsts                                                  (stats.txt)
  cpi_mem_CPU      = (dmiss × dlat + imiss × ilat) / I_CPU                      (stats.txt)
  CPI_CPU,exec     = constant per size (5 numbers total, calibrated at c=5,m=0)
  t_CPU            = 0.5 ns                                                    (config.ini)

  I_PIM            = matrix1.numReads + matrix2.numReads + matrix3.numWrites   (stats.txt)
  CPI_PIM,exec     = intercept of linear fit per (size, compute) (35 numbers)
  m_PIM            = slope of same fit, in cycles per ns of scratchpad latency
  t_PIM,clk        = 1 ns                                                      (config.ini)
```

Total calibration parameters: **5 (CPU) + 70 (PIM, 35 pairs) = 75**, all
sourced from `stats.txt`. Sweep size: **280 points**.

## Results

### Predicted vs Simulated

The single most informative validation view is the predicted-vs-simulated
scatter (`plots/viper_scatter.png`). Each of the 280 sweep points is
plotted as

- **x-axis**: simulated execution time `T_simulated` from gem5
  (`stats.txt` field `simSeconds`),
- **y-axis**: predicted execution time `T_predicted` from the iron-law
  formula above, computed entirely from `stats.txt`-sourced quantities
  and the per-(size, c) calibration constants.

Both axes are log-scaled because the sweep covers nearly three decades
of total execution time (the smallest `(size=8, c=5, m=0)` run finishes
in milliseconds; the largest `(size=128, c=5000, m=128)` run takes
seconds). A point that lies exactly on the diagonal `y = x` means the
formula reproduces gem5 perfectly for that configuration; the dotted red
band shows the ±10% envelope around the diagonal.

Each point is **colored by binary size** (viridis, 5 sizes from 8 to
128) and **shaped by compute regime** (`c`, the configured per-FMA
latency, with 7 distinct markers from `c=5` to `c=5000`). This makes
the plot a single-glance summary of the entire (`size`, `compute`,
`m_config`) cube: every cluster of one color represents one binary
across all of its operating points, and every cluster of one marker
represents one compute regime across all binaries.

The reading is:

- **Diagonal alignment** — points sit tightly along `y = x` across all
  three decades of `T`. The formula scales with `T` correctly; the
  iron-law is not just fit to one operating regime.
- **Within ±10% band** — ~272 of 280 points (97.1%) fall inside the
  envelope. The few stragglers are not random scatter; they cluster at
  one specific operating point (`m_config = 8 ns`, transitioning from
  hidden to exposed scratchpad latency) and are explained in the
  per-size error breakdown below.
- **No systematic curvature, no fanning** — neither the small-time
  cluster (size=8, low `c`) nor the large-time cluster (size=128,
  high `c`) drifts off the diagonal as a group. There is no
  size-dependent or compute-dependent bias the formula is failing to
  capture.

This is exactly what "the iron-law decomposition fits the data" looks
like geometrically: a tight, unbiased scatter along `y = x` over the
full dynamic range of the sweep, with the residuals matching the
expected limitations of a 2-parameter linear `CPI_PIM(m)` fit at one
specific transition point.

### Headline numbers

Across the entire 280-point sweep:

| metric | value |
|---:|---:|
| median \|err\| | **1.18 %** |
| p90 \|err\|    | 6.26 % |
| p95 \|err\|    | 7.98 % |
| p99 \|err\|    | 10.76 % |
| max  \|err\|   | 15.59 % |
| points within 10 % | ~272 / 280 |

Worst-case error per binary size:

| size | max \|err\| |
|---:|---:|
|   8 |  4.73 % |
|  16 | 11.94 % |
|  32 | 10.76 % |
|  64 |  9.06 % |
| 128 | 15.59 % |

The 10 outliers above 10% are all concentrated at **`m = 8`** and at the
**`c = 50` / `c = 500` transition region**. This is consistent with the
expected limitation of a 2-parameter linear fit: the real `CPI_PIM(m)`
curve has a small knee near `m ≈ 8`, where DMA pipelining absorbs some
of the latency, that the pure `a + b·m` form smooths over. A piecewise
or quadratic fit would close the remaining gap, but at the cost of more
calibration parameters.

## Output File

Per-point CSV: `model_predictions.csv` (repo root)

| column | description |
|---|---|
| `benchmark, size, compute, mem_latency_ns` | sweep coordinates |
| `simSeconds, simInsts, system.cpu.numCycles, system.cpu.quiesceCycles, system.cpu.cpi` | raw stats.txt |
| `system.cpu.dcache.demandMisses::total, ...demandAvgMissLatency::total` | raw stats.txt |
| `system.cpu.icache.demandMisses::total, ...demandAvgMissLatency::total` | raw stats.txt |
| `system.gemm_clstr.matrix{1,2}.numReads::total, matrix3.numWrites::total` | raw stats.txt |
| `cpu_cpi_mem` | derived: `(dmiss·dlat + imiss·ilat) / I_CPU` |
| `cpu_cpi_exec_calibrated` | calibrated CPU CPI_exec(size) |
| `I_PIM` | sum of the 3 matrix counters |
| `pim_cpi_exec_calibrated, pim_m_calibrated` | PIM linear fit (a, b) per (size,c) |
| `T_CPU_active_predicted` | `I_CPU × (CPI_exec + cpi_mem) × t_CPU` |
| `T_PIM_predicted` | `I_PIM × (a + b·m) × t_PIM` |
| `computed_simSeconds` | sum of CPU and PIM predicted |
| `simulated_simSeconds` | gem5 `simSeconds` |
| `rel_error_pct` | `(computed - simulated) / simulated × 100` |

## Conclusion

The VIPER model is **validated** as a description of the
gem5-SALAM Llama2 sweep:

- The CPU side (`I_CPU × (CPI_exec + cpi_mem) × t_CPU`) reproduces the
  measured CPU active time to better than 0.1% with a single calibration
  constant per binary, confirming that `CPI_exec` is a stable property
  of the (compiled binary × CPU model) pair and does not leak from
  the accelerator parameters.

- The PIM side (`I_PIM × (CPI_exec + m_PIM × m) × t_PIM`) reproduces the
  measured accelerator wait time to within ~10% over the full sweep using
  a 2-parameter linear fit per `(size, compute)`. The fit cleanly captures
  both the memory-bound regime (`m_PIM` flat, `CPI_exec` flat) and the
  compute-bound regime (`m_PIM → 0`, `CPI_exec` ∝ `c`).

- Sum of the two terms predicts total `simSeconds` to a median of 1.2%
  and within 10% for ~97% of the sweep points.

This means the formula is a **legitimate analytical model** of the
heterogeneous CPU+PIM execution time, where each variable has a clear
physical meaning and a definite source in the simulator output. It can
now be used as the basis for design-space exploration, sensitivity
analysis, and roofline-style reasoning.
