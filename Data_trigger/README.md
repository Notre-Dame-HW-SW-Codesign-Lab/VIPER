# Data Trigger — gem5 Verification Model

This folder holds the modified **gem5 simulator** used to verify the VIPER data-trigger formula against cycle-accurate memory-system behaviour. It is the ground-truth reference for the closed-form model in [`../viper_data_trigger/`](../viper_data_trigger/).

## Contents

```
Data_trigger/
└── gem5/          Modified gem5 v24 with PIM data-trigger timing model
```

## What Was Modified

VIPER injects the data-trigger overhead **inside the CPU memory controller** so that every LLC (L3) miss automatically fires a PIM operation, exactly as the closed-form model describes.

- `gem5/src/mem/mem_ctrl.cc` — PIM DRAM accesses, PIM SRAM capacity cache, PIM compute delay
- `gem5/src/mem/packet.hh` — added `bespoke` field to suppress CPU responses to PIM-internal packets
- `gem5/configs/se_config.py` — syscall-emulation configuration (3 GHz TimingSimpleCPU or DerivO3CPU, L1/L2/L3, DDR4-2400)

All PIM parameters live at the top of `mem_ctrl.cc` and are recompiled in a matter of seconds:

- `pimDelay` — PIM compute latency (ticks)
- `PIM_SRAM_CAPACITY` — PIM on-die SRAM capacity
- `pimDramLeaves`, `pimDramAccessCount` — number of DRAM accesses per data trigger
- `pimCounterBaseAddr`, `pimDramBaseAddr` — PIM address-mapping offsets

## How to Use

See [`gem5/README.md`](gem5/README.md) for the full build-and-run guide, including the Apptainer-based reproducible workflow used to generate every gem5 data point in the paper.

## Relation to the Rest of VIPER

| Fast path (early-stage DSE) | Verification path |
|---|---|
| [`../viper_data_trigger/`](../viper_data_trigger/) | This folder |
| Closed-form Python model | Full gem5 cycle-accurate simulator |
| Seconds per design point | Hours per benchmark |
| Predicts AMAT slowdown | Measures AMAT directly from cache statistics |

The paper shows Pearson r > 0.999 and MAE < 0.12 cycles between the two — the closed-form model is a reliable substitute for gem5 during design-space exploration.
