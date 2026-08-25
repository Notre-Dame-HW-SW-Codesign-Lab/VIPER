# Case Study — Task Offloading Validation on Real UPMEM Hardware

This case study **validates the VIPER task-offloading prediction on a real 2,560-DPU UPMEM PIM server** by porting a LLaMA2-style transformer, running it on the hardware, and comparing the measured speedup curve against the a-priori VIPER projection.

The detailed methodology, measured numbers, and reconciliation between prediction and hardware are in [**REPORT.md**](REPORT.md).

## Headline Result

| Metric | VIPER prediction (before hardware) | Measured on 2,560-DPU UPMEM |
|---|---|---|
| Per-DPU INT8 throughput | 45 MOPS (published) | 48.0 MOPS (within 7%) |
| Compute scaling law | T_kernel(1) / N | Holds to within ~2% up to 512 DPUs |
| Break-even DPU count | ≈ 49 DPUs | ~40–49 DPUs |
| Peak speedup | 2.42× | **2.46×** |
| Optimal DPU count | (uncapped) | 256 DPUs |

VIPER predicted the offload ceiling correctly **before writing a single line of DPU code**. Hardware measurement then refined one input — the DMA transfer term grows with DPU count at large N — which reproduces the observed peak-and-roll-off at 2,560 DPUs.

## Contents

```
case_study_task_off/
├── REPORT.md                  Full detailed report (methodology, measurements, analysis)
├── common.h, layer.h          Q16.16 forward-pass + int8 offload layout
├── main_cpu.c                 CPU baseline (full and STUB variant for task-off profiling)
├── matmul_dpu.c               INT8 DPU kernel (native mul_sl_sl, fused sub-matmuls)
├── host.c                     Offload driver, DPU sweep, correctness check
├── bench_launch.c             Empty-launch overhead microbenchmark
├── plot.py                    Regenerates the three figures from sweep data
├── Makefile                   `make all-repro` rebuilds, re-runs sweep, re-plots
├── viper_speedup.png          Measured vs predicted speedup curve (fixed-transfer model)
├── viper_speedup_nonfixed.png Measured vs refined-transfer model curve
├── viper_time_decomp.png      Compute vs transfer breakdown across DPU sweep
└── viper_transfer.png         DMA transfer time vs DPU count
```

## Workload

- LLaMA2-style transformer, dim=128, hidden_dim=256, n_layers=1, n_heads=8, vocab=256, batch=128
- 8 matmul calls per forward pass, 25.2 M MACs total
- Host runs Q16.16 fixed-point (transcendentals stay on CPU)
- Offloaded matmuls run as INT8 GEMM on DPUs, INT32 accumulation, right-shifted for write-back

## Reproduce

Requires access to a UPMEM server with the UPMEM SDK 2025.1.0 installed.

```bash
make            # build CPU baseline, INT8 DPU kernel, host driver
make baseline   # → T_full ≈ 20.4 ms, T_stub ≈ 3.1 ms
make sweep      # → sweep_final.dat (runs the full DPU sweep on hardware)
make figures    # → viper_speedup / viper_time_decomp / viper_transfer .png
```

See [REPORT.md](REPORT.md) for the complete narrative and analysis.
