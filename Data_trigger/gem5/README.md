# VIPER gem5 — PIM Data Trigger Timing Model

This repository contains a modified version of [gem5](http://www.gem5.org) that implements a **cycle-accurate timing model for PIM (Processing-In-Memory) data trigger operations executed inside a CPU core**. It is used to produce end-to-end latency measurements for the VIPER project.

---

## Table of Contents

- [Overview](#overview)
- [What Is a Data Trigger?](#what-is-a-data-trigger)
- [How the Model Works](#how-the-model-works)
- [Modified Files](#modified-files)
- [PIM Parameters You Can Tune](#pim-parameters-you-can-tune)
- [Prerequisites and Build](#prerequisites-and-build)
- [Running a Simulation](#running-a-simulation)
- [Collecting Results](#collecting-results)
- [Running the Full Benchmark Suite](#running-the-full-benchmark-suite)
- [Citation](#citation)

---

## Overview

Modern PIM architectures (e.g., UPMEM, Axdimm) place compute logic near DRAM to eliminate the memory-bandwidth bottleneck. The VIPER design introduces a **data trigger** mechanism: instead of offloading whole kernels to the PIM die, small cryptographic integrity checks (Merkle-tree counter verification) are triggered automatically by each memory access and performed in-memory.

This gem5 fork models that data trigger overhead **inside the CPU memory controller** so we can:

1. Measure the end-to-end slowdown of running real workloads with the data trigger enabled.
2. Sweep PIM parameters (latency, SRAM capacity, tree depth) without building real hardware.
3. Compare against a CPU-only baseline to quantify the data trigger penalty.

---

## What Is a Data Trigger?

A data trigger fires on every DRAM read and performs the following sequence:

1. **PIM DRAM accesses** — the PIM die autonomously reads `pimDramAccessCount` (= `ceil(log2(pimDramLeaves))`) addresses from DRAM to fetch the Merkle-tree nodes needed for integrity verification. These are DRAM accesses issued *by the PIM itself*, not by the CPU.
2. **PIM SRAM capacity lookup** — the PIM's on-die SRAM (`PIMCapacityCache`) is checked for the data's counter block. On a **hit**, no extra DRAM traffic is needed. On a **miss**, the PIM must fetch the counter block from DRAM, generating another round of PIM DRAM accesses.
3. **PIM compute delay** — a fixed latency (`pimDelay`) is injected to model the in-memory compute time of the verification.

All auxiliary DRAM transactions are injected transparently into gem5's memory-controller pipeline — the CPU never sees them.

---

## How the Model Works

### Memory Controller (`src/mem/mem_ctrl.cc`)

All modifications live at the top of `mem_ctrl.cc` (lines 60–280) and inside `MemCtrl::recvTimingReq()`.

**Key added classes:**

| Class | Purpose |
|---|---|
| `AddressCache` | Simple FIFO cache for address deduplication. |
| `DataCache` | LRU cache modeling a CPU-side data cache (L2/L3 hit/miss). |
| `PIMCapacityCache` | LRU cache modeling the PIM's on-die SRAM. Its capacity (`PIM_SRAM_CAPACITY`) directly controls how often the PIM must go back to DRAM for a counter block. A **miss** triggers another round of PIM DRAM accesses. |

**Key added functions:**

| Function | Purpose |
|---|---|
| `pimDramAddresses(addr)` | Returns the list of DRAM addresses the PIM must read (Merkle-tree nodes) for a given data address. These represent the PIM's autonomous DRAM traffic. |
| `pim_capacity_lookup(addr)` | Probes `PIMCapacityCache`. Logs each hit/miss and the running miss rate to `/tmp/PIM_MISS_RATE.txt`. Returns `true` on a miss. |

**Request flow for every CPU DRAM read:**

```
CPU issues read to address A
  │
  ├─ pimDramAddresses(A) → PIM fetches pimDramAccessCount addresses from DRAM
  │                         (packets marked bespoke=1, no CPU response needed)
  │
  ├─ pim_capacity_lookup(A) — check PIM on-die SRAM
  │    ├─ HIT  → no extra DRAM traffic
  │    └─ MISS → PIM fetches pimDramAccessCount more addresses from DRAM
  │
  └─ Schedule original CPU packet with delay: curTick() + pimDelay
```

### Packet Tagging (`src/mem/packet.hh`)

A single field was added to `Packet`:

```cpp
int bespoke = 0;   // 1 = PIM-generated DRAM access, suppress CPU response
```

PIM-generated packets (`bespoke = 1`) are silently dropped in `accessAndRespond()` after the DRAM access, so the CPU never receives a spurious response for PIM's internal traffic.

### Simulation Config (`configs/se_config.py`)

A syscall-emulation (SE) configuration that instantiates:
- `TimingSimpleCPU` at 3 GHz (swap for `DerivO3CPU` to model out-of-order execution)
- L1 I/D caches (32 KiB, 8-way), L2 (256 KiB, 8-way), L3 (8 MiB, 16-way)
- `DDR4_2400_8x8` DRAM with 1024-entry read/write buffers
- 32 GiB address space

---

## Modified Files

```
src/mem/mem_ctrl.cc   ← PIM DRAM access injection, PIM SRAM capacity model, PIM delay
src/mem/packet.hh     ← added `int bespoke` field to suppress PIM-generated responses
configs/se_config.py  ← SE-mode simulation configuration
script.sh             ← SGE job script for running SPEC + Graph benchmark suite
aes_script.sh         ← SGE job script for AES-instrumented benchmarks
```

---

## PIM Parameters You Can Tune

All tunable constants are at the top of `src/mem/mem_ctrl.cc`. Edit them and recompile — only the changed file is rebuilt, so incremental builds are fast.

```cpp
// ── PIM DRAM access structure (PIM's own accesses to DRAM) ────────────────
const int pimDramLeaves = 2;
// Number of Merkle tree leaves (must be a power of 2).
// Controls how deep the tree is: pimDramAccessCount = ceil(log2(pimDramLeaves)).
// A deeper tree means more PIM DRAM accesses per CPU read.

const int pimDramBaseAddr = 0x200;
// Base address offset used by the PIM when computing which DRAM addresses
// to access for Merkle-tree node data. Adjust to match your PIM memory layout.

const int pimCounterBaseAddr = 0x10000;
// Address offset used to derive the counter block address from a data address.
// The PIM SRAM lookup uses (data_addr + pimCounterBaseAddr) as the cache key.

// ── PIM compute latency ────────────────────────────────────────────────────
const uint64_t pimDelay = 500000;
// Latency injected per CPU read to model PIM in-memory computation, in gem5 ticks.
// Default gem5 resolution = 1 ps/tick → 500000 ticks = 500 ns.
// Set to 0 to measure pure DRAM traffic overhead with no compute delay.

// ── PIM on-die SRAM capacity ───────────────────────────────────────────────
const int PIM_SRAM_CAPACITY = 15625;
// Number of counter blocks the PIM's on-die SRAM can hold simultaneously.
// Each entry covers 2^10 bytes of data → 15625 entries ≈ 15 MiB working-set coverage.
// Increase to model a larger PIM SRAM (fewer misses); decrease to stress the miss path.

// ── CPU-side cache sizes (for reference accounting) ───────────────────────
const int L2_CACHE_SIZE = 4096 * 2;   // 8192 blocks
const int L3_CACHE_SIZE = 4096 * 4;   // 16384 blocks
```

> **After any edit:** recompile with `scons build/X86/gem5.opt -j$(nproc)` before running new experiments.

---

## Prerequisites and Build

### Option A — Apptainer / Singularity (recommended)

No host dependencies beyond Apptainer. The scripts pull a pre-built Ubuntu 24.04 image from the official gem5 Docker registry.

```bash
# Pull the container image once (~1 GB download)
apptainer pull gem5.sif docker://ghcr.io/gem5/ubuntu-24.04_min-dependencies:v24-0

# Build gem5 inside the container (first build takes 20–40 min)
apptainer exec --bind $(pwd):/mnt/gem5 gem5.sif \
  bash -lc "cd /mnt/gem5 && scons build/X86/gem5.opt -j$(nproc)"
```

The resulting binary is `build/X86/gem5.opt`.

### Option B — Native build

**System requirements:**
- GCC ≥ 10 or Clang ≥ 11
- Python ≥ 3.6 with `pip`
- SCons ≥ 3.0

```bash
# Install Python dependencies
pip install -r requirements.txt

# Build (X86 ISA, optimised binary)
scons build/X86/gem5.opt -j$(nproc)
```

---

## Running a Simulation

```bash
GEM5=build/X86/gem5.opt
CFG=configs/se_config.py

$GEM5 --outdir=m5out/my_run $CFG \
      --cmd=/path/to/benchmark \
      --args="<benchmark arguments>"
```

**Example — `473.astar` with a 192×192 grid:**

```bash
build/X86/gem5.opt --outdir=m5out/astar \
  configs/se_config.py \
  --cmd=SPEC/473astar \
  --args="192 192 8"
```

gem5 writes its statistics to `m5out/my_run/stats.txt`. Key metric:

```
simTicks   # total simulated picoseconds
           # divide by 1e12 to get seconds at gem5 default 1 ps/tick
```

The PIM SRAM miss log is written to `/tmp/PIM_MISS_RATE.txt` during the run. The provided shell scripts automatically move it alongside the gem5 stats output.

---

## Collecting Results

### CPU-only baseline (no PIM overhead)

Set `pimDelay = 0` in `mem_ctrl.cc`, rebuild, and rerun. Compare `simTicks` to quantify data-trigger overhead.

Alternatively, comment out the `pimDramAddresses` packet generation blocks in `recvTimingReq()` to also eliminate the extra PIM DRAM reads.

### PIM SRAM miss rate

After each run:

```
outputs/<benchmark>/PIM_MISS_RATE_<benchmark>.txt
```

Each line records a PIM SRAM hit or miss for a single memory access plus a running average miss rate. A lower miss rate means `PIM_SRAM_CAPACITY` is large enough to hold the working set of counters — a key design trade-off for sizing the PIM's on-die SRAM.

### Useful gem5 statistics

```
system.mem_ctrl.readReqs          # DRAM read requests entering the controller
system.mem_ctrl.bytesReadSys      # total DRAM bytes read (includes PIM DRAM accesses)
system.cpu.numCycles              # total CPU cycles
system.cpu.ipc                    # instructions per cycle
```

---

## Running the Full Benchmark Suite

The provided SGE job scripts run all benchmarks in batch:

```bash
# SPEC CPU2017 + GraphBIG workloads (data trigger enabled)
qsub script.sh

# Same workloads compiled with AES instrumentation
qsub aes_script.sh
```

Both scripts bind-mount the gem5 directory into the Apptainer container and collect the PIM SRAM miss-rate files into per-benchmark output directories under `outputs/`.

If you are **not** on an SGE cluster, run each benchmark individually as shown above, or adapt the job scripts to your scheduler (SLURM, PBS, etc.).

---

## Citation

If you use this simulator in your research, please cite the VIPER paper and gem5:

```
@inproceedings{lowe-power2020gem5,
  title     = {The gem5 Simulator: Version 20.0+},
  author    = {Lowe-Power, Jason and others},
  booktitle = {arXiv preprint arXiv:2007.03152},
  year      = {2020}
}
```
