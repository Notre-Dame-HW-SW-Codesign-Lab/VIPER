# VIPER Data Trigger — Raw Experimental Data

This folder contains the raw gem5 simulation data and analysis scripts that back the **data-trigger evaluation** section of the VIPER paper. Every number, scatter plot, and trend plot in that section can be reproduced directly from the CSV files here.

---

## Folder Structure

```
data_trigger_data/
├── validation/        gem5 sweep results used to validate the VIPER AMAT formula
├── amat_trend/        AMAT comparison across Baseline / 40 ns / 40 ns+2 GB configs
└── case_study/        In-Memory Cryptography (IMCRYPTO) case study data and scripts
```

---

## Benchmarks

All experiments run 13 benchmarks across two categories:

**SPEC CPU2017**

| Short name | Suite benchmark |
|---|---|
| `433milc` | 433.milc |
| `462libquantum` | 462.libquantum |
| `473astar` | 473.astar |
| `605mcf_s` | 605.mcf_s |
| `619lbm_s` | 619.lbm_s |
| `620onmetpp_s` | 620.omnetpp_s |
| `625x264_s` | 625.x264_s |
| `638imagick_s` | 638.imagick_s |

**GraphBIG graph analytics**

| Short name | Algorithm |
|---|---|
| `degreeCentr` | Degree centrality |
| `graphColoring` | Graph coloring |
| `kCore` | k-core decomposition |
| `pageRank` | PageRank |
| `triangleCount` | Triangle counting |

> `triangleCount` is excluded from ratio plots because its baseline AMAT is nearly zero, making normalization numerically unstable. All other 12 benchmarks appear in every figure.

---

## CSV Column Reference

Every CSV in this repository shares the same schema. Each row represents one benchmark under one cache-hit-time accounting mode.

| Column | Unit | Description |
|---|---|---|
| `benchmark` | — | Benchmark short name (see table above) |
| `ht_mode` | — | Hit-time accounting mode: `tag+data` (tag + data latency only) or `tag+data+resp` (includes response latency, used in all paper figures) |
| `alpha_mshr_hit_factor` | fraction | MSHR occupancy correction factor α = 0.5 (fraction of misses that find an in-flight MSHR entry and get a free ride) |
| `llc_extra_ns` | ns | Extra LLC round-trip latency added by the data-trigger gateway |
| `llc_extra_cyc` | cycles | Same as above, converted to CPU cycles at 3 GHz |
| `tL1_hit_cyc` | cycles | L1 cache hit time |
| `tL2_hit_cyc` | cycles | L2 cache hit time |
| `tL3_hit_cyc` | cycles | L3 cache hit time |
| `m1_L1_miss_rate` | fraction | L1 data-cache miss rate (misses / accesses) |
| `m2_L2_miss_rate` | fraction | L2 miss rate, conditional on L1 miss |
| `m3_L3_miss_rate` | fraction | L3 miss rate, conditional on L2 miss |
| `p1_L1_new_mshr_frac` | fraction | Fraction of L1 misses that allocate a new MSHR entry (vs. merging into existing) |
| `p2_L2_new_mshr_frac` | fraction | Same for L2 |
| `p3_L3_new_mshr_frac` | fraction | Same for L3 |
| `dram_avg_cyc` | cycles | Average DRAM access latency measured by gem5 (queue + bus + memory) |
| `amat_stats_cyc` | cycles | **Ground-truth AMAT** computed directly from gem5 statistics (total memory stall ticks / total accesses) |
| `amat_textbook_cyc` | cycles | AMAT predicted by the standard textbook formula: `tL1 + m1*(tL2 + m2*(tL3 + m3*tDRAM))` |
| `amat_textbook_mshr_cyc` | cycles | AMAT predicted by the MSHR-corrected formula (replaces raw miss rates with effective rates accounting for α) |
| `abs_err_textbook` | cycles | Absolute error: `|amat_stats_cyc − amat_textbook_cyc|` |
| `abs_err_textbook_mshr` | cycles | Absolute error for the MSHR-corrected variant |

`Intel_SGX.csv` and `IMCRYPTO_*.csv` (case study) include additional columns:

| Column | Unit | Description |
|---|---|---|
| `dram_num_reads_data` | count | DRAM read requests for data (not instructions) |
| `dram_bytes_read_data_MB` | MB | Total data bytes read from DRAM |
| `dram_avg_q_lat_cyc` | cycles | Average DRAM queuing latency |
| `dram_avg_bus_lat_cyc` | cycles | Average DRAM bus latency |
| `dram_avg_mem_acc_lat_cyc` | cycles | Average DRAM media access latency |
| `dram_bw_read_total_Bps` | B/s | Total DRAM read bandwidth |
| `sim_seconds` | s | Simulated execution time |
| `sim_ticks` | ticks | gem5 simulation ticks (1 tick = 1 ps) |

---

## `validation/` — VIPER Formula Validation

### What This Is

This is the core validation dataset. We ran gem5 across a full matrix of PIM configurations and recorded both the **gem5-measured AMAT** (`amat_stats_cyc`) and the **VIPER formula prediction** (`amat_textbook_cyc`). The goal is to show that the VIPER closed-form formula tracks gem5 across all PIM parameters — proving the formula can be used to predict performance without running gem5 for every design point.

### Configuration Matrix

Each CSV file corresponds to one (CPU type, PIM latency, PIM SRAM capacity) configuration:

| File | CPU | t_PIM | PIM SRAM |
|---|---|---|---|
| `Baseline_inorder.csv` | In-Order | — (no PIM) | — |
| `Baseline_OoO.csv` | Out-of-Order | — (no PIM) | — |
| `inorder_5ns_1MB.csv` | In-Order | 5 ns | 1 MB |
| `inorder_50ns_1MB.csv` | In-Order | 50 ns | 1 MB |
| `inorder_500ns_1MB.csv` | In-Order | 500 ns | 1 MB |
| `inorder_50ns_64MB.csv` | In-Order | 50 ns | 64 MB |
| `inorder_50ns_256MB.csv` | In-Order | 50 ns | 256 MB |
| `inorder_50ns_4GB.csv` | In-Order | 50 ns | 4 GB |
| `inorder_500ns_100GB.csv` | In-Order | 500 ns | 100 GB (PIM-in-storage) |
| `OoO_5ns_1MB.csv` | Out-of-Order | 5 ns | 1 MB |
| `OoO_50ns_1MB.csv` | Out-of-Order | 50 ns | 1 MB |
| `OoO_500ns_1MB.csv` | Out-of-Order | 500 ns | 1 MB |
| `OoO_500ns_100GB.csv` | Out-of-Order | 500 ns | 100 GB (PIM-in-storage) |

The three sweep axes:
- **t_PIM sweep** (C1–C6): varies PIM compute latency (5/50/500 ns) at fixed 1 MB SRAM
- **Capacity sweep** (C7–C9): varies PIM SRAM size (64/256 MB / 4 GB) at fixed 50 ns
- **PIM-in-storage** (C10–C11): 500 ns latency, 100 GB capacity (DRAM-scale storage)

### Reproducing the Figures

**Scatter validation plot** (`scatter_validation.pdf`) — shows gem5 AMAT ratio vs. VIPER predicted ratio for every benchmark × configuration:

```bash
cd validation/
pip install pandas matplotlib scipy numpy
python Scatter.py
# output: scatter_validation.pdf
```

**Per-benchmark trend plot** (`trend_per_benchmark.pdf`) — shows that VIPER and gem5 AMAT move together across all configurations for each benchmark individually:

```bash
python Trending.py
# output: trend_per_benchmark.pdf
```

**Key metrics from the paper:** Pearson r > 0.999 and MAE < 0.12 across all 132 In-Order data points and all 48 OoO data points, confirming that the VIPER formula predicts gem5 AMAT with high fidelity.

---

## `amat_trend/` — Baseline vs. PIM AMAT Comparison

### What This Is

A focused comparison of three operational points — **Baseline** (no PIM), **40 ns no-PIM** (gateway overhead only, no in-array compute), and **40 ns + 2 GB PIM** (full PIM with capacity) — for both In-Order and Out-of-Order CPUs. This validates that the VIPER AMAT formula tracks gem5 not just in ratio but also in trend direction across configuration changes.

### Files

| File | CPU | Configuration |
|---|---|---|
| `Baseline_inorder.csv` | In-Order | No PIM (CPU only) |
| `40ns_no_pim_inorder.csv` | In-Order | 40 ns gateway, no in-array compute |
| `40ns_2GB_pim_inorder.csv` | In-Order | 40 ns gateway + 2 GB PIM SRAM |
| `Baseline_no_pim_OoO.csv` | Out-of-Order | No PIM (CPU only) |
| `40ns_no_pim_OoO.csv` | Out-of-Order | 40 ns gateway, no in-array compute |
| `40ns_2GB_OoO.csv` | Out-of-Order | 40 ns gateway + 2 GB PIM SRAM |

### Reproducing the Figures

```bash
cd amat_trend/
pip install pandas matplotlib scipy numpy
python AMAT_trend.py
# outputs:
#   AMAT_trend_scatter.png       — scatter: stat vs. textbook/MSHR variants
#   AMAT_trend_lines_InOrder.png — per-benchmark trend lines, In-Order
#   AMAT_trend_lines_OutofOrder.png — per-benchmark trend lines, OoO
#   AMAT_trend_MAE_perbenchmark.png — per-benchmark MAE bar chart
```

---

## `case_study/` — In-Memory Cryptography (IMCRYPTO)

### What This Is

An end-to-end case study showing how the VIPER data-trigger model applies to a real PIM application: **In-Memory Cryptography (IMCRYPTO)**, which places AES encryption logic inside DRAM and triggers it on every LLC miss. The data here characterizes SGX baseline performance and models the PIM-side overhead as a function of gateway capacity, working-set size, and technology.

The model derives from the VIPER data-trigger equation (paper Eq. 8/9):

```
t_PIM,mem(C) = t_gw(C) + (1 − h(C)) × t_DRAM
```

where `t_gw(C)` is the gateway latency (fixed overhead + capacity-dependent SRAM programming time) and `h(C)` is the fraction of triggered accesses served from the PIM on-chip SRAM.

### Files

| File | Description |
|---|---|
| `Intel_SGX.csv` | gem5 characterization of each benchmark running with Intel SGX (baseline for the IMCRYPTO comparison). Contains full DRAM statistics. |
| `IMCRYPTO_64MB.csv` | IMCRYPTO with 64 MB gateway SRAM |
| `IMCRYPTO_256MB.csv` | IMCRYPTO with 256 MB gateway SRAM |
| `IMCRYPTO_4GB.csv` | IMCRYPTO with 4 GB gateway SRAM |

### Analysis Scripts

| Script | Figure | Description |
|---|---|---|
| `IMCRYPTO_capacity_sweep.py` | Capacity sweep plot | Normalized memory latency vs. gateway on-chip SRAM capacity (3 KB → 1 GB). Identifies the optimal capacity point. Reads `Intel_SGX.csv`. |
| `IMCRYPTO_size_swap.py` | Working-set size plot | Latency vs. application working-set size at fixed capacity. Shows when SRAM becomes insufficient. |
| `IMCRYPTO_technologies_swap.py` | Technology comparison | Compares ReRAM vs. SRAM vs. DRAM as gateway storage technology at various capacities. |
| `IMCRYPTO_vs_SGX_post_layout.py` | SGX vs. IMCRYPTO bar chart | Direct performance comparison. Reads `IMCRYPTO_*.csv` and `Intel_SGX.csv`. |
| `IMCRYPTO_tprop_breakeven.py` | Break-even analysis | Shows the programming latency `t_prog` at which IMCRYPTO breaks even with SGX, as a function of capacity. |

### Reproducing the Figures

```bash
cd case_study/
pip install pandas matplotlib numpy scipy
python IMCRYPTO_capacity_sweep.py    # → imcrypto_capacity.pdf
python IMCRYPTO_size_swap.py         # → imcrypto_size.pdf
python IMCRYPTO_technologies_swap.py # → imcrypto_technology.pdf
python IMCRYPTO_vs_SGX_post_layout.py # → imcrypto_latency.pdf
python IMCRYPTO_tprop_breakeven.py   # → imcrypto_tprog.pdf
```

---

## System Configuration

All gem5 simulations use the following fixed system configuration (see `Data_trigger/gem5/configs/se_config.py`):

| Component | Setting |
|---|---|
| CPU | TimingSimpleCPU (In-Order) or DerivO3CPU (OoO) @ 3 GHz |
| L1 I/D cache | 32 KiB, 8-way, 2-cycle tag+data, 2-cycle response |
| L2 cache | 256 KiB, 8-way, 10-cycle tag+data, 10-cycle response |
| L3 cache | 8 MiB, 16-way, 30-cycle tag+data, 30-cycle response |
| DRAM | DDR4-2400, 8×8, 32 GiB address space |
| DRAM read/write buffers | 1024 entries each |
| Simulator version | gem5 v24 (modified — see `Data_trigger/gem5/`) |

The `ht_mode = tag+data+resp` rows are used in all paper figures. The `tag+data` rows are included for sensitivity analysis.

---

## Dependencies

```
python >= 3.8
pandas >= 1.3
matplotlib >= 3.4
numpy >= 1.20
scipy >= 1.7
```

Install all at once:

```bash
pip install pandas matplotlib numpy scipy
```
