<h1 align="center">VIPER</h1>

<p align="center">
  <b>A fast, workload-driven design-space exploration framework for Processing-In-Memory</b>
</p>

<p align="center">
  <img alt="python" src="https://img.shields.io/badge/python-%E2%89%A5%203.8-blue">
  <img alt="gem5" src="https://img.shields.io/badge/gem5-v24-green">
</p>

---

## What Is VIPER?

**VIPER** is a framework for exploring the design space of Processing-In-Memory (PIM) accelerators — in *seconds*, on *your own workload*, before writing a single line of PIM code.

Traditional PIM tools force a hard trade-off:
- **Cycle-accurate simulators** (gem5-based) take hours per design point and require the workload to be built and instrumented.
- **Circuit/device simulators** are fast but only model the array itself — they cannot answer "what does this do to my end-to-end application?"
- **Real hardware** (UPMEM, Samsung HBM-PIM) is the ground truth, but you can only measure what has already been fabricated.

VIPER closes that gap with a closed-form model that predicts the impact of a PIM design on any workload from CPU performance counters, a matching gem5 timing model for cycle-accurate verification, and a real-hardware case study on a UPMEM PIM server.

The intended workflow: **use the closed-form model to prune the design space in seconds, then verify the best candidates in gem5 or on hardware.**

## Two PIM Paradigms

VIPER covers two ways of coupling compute to memory:

- **Data-trigger** — PIM operations fire automatically at the memory controller on last-level-cache misses.
- **Task-offloading** — a DMA-attached accelerator (e.g. a UPMEM DPU) to which the CPU explicitly offloads kernels.

## Repository Layout

| Folder | What it is | When to use it |
|---|---|---|
| [`viper_task_off/`](viper_task_off/) | `perf`-based collector for CPU CPI/AMAT metrics (`CPU_time = I·CPI·t_clock`, `CPI = CPI_exec + m_CPU·AMAT`). | Producing the raw performance counters that feed VIPER. |
| [`task_offloading/`](task_offloading/) | gem5 DMA-enabled accelerator model for the task-offloading side of VIPER (register interface, DMA, interrupts, LLaMA2 sweep configs). | Verifying a task-offloading design in gem5. |
| [`case_study_task_off/`](case_study_task_off/) | A LLaMA2-style transformer **measured on a 2,560-DPU UPMEM server**, reconciled against the VIPER prediction. | Seeing VIPER validated on real silicon (2.46× measured vs 2.42× predicted). |
| [`Data_trigger/`](Data_trigger/) | Modified gem5 v24 with the PIM data-trigger timing model injected at the memory controller. | Cycle-accurate verification of a specific data-trigger PIM configuration. |
| [`data_trigger_data/`](data_trigger_data/) | Raw gem5 CSVs and analysis scripts backing the data-trigger evaluation. | Reproducing the data-trigger figures. |

Every folder ships with its own detailed README — start there for build-and-run instructions.

## Quick Start

Profile your workload to collect the performance counters VIPER needs:

```bash
cd viper_task_off
sudo ./collect_metrics.sh <your-application>
# -> perf_metrics_<timestamp>.txt  (raw counters + CPI/AMAT breakdown)
```

Then use those metrics to drive the task-offloading gem5 model in [`task_offloading/`](task_offloading/), or reproduce the real-hardware validation in [`case_study_task_off/`](case_study_task_off/).
