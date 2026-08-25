"""
IMCRYPTO capacity sweep (replaces the 5x12 bar chart).

Model = VIPER data-triggering, Eq. (8)/(9) of the paper:

  Every L3 miss passes through the gateway:

      t_gw = t_in + t_prog(C) + t_comp + t_out

  Resident fraction h(C) of triggered data is served in-array.
  The remaining fraction falls through and additionally pays DRAM latency:

      t_PIM,mem(C) = t_gw(C) + (1 - h(C)) * t_DRAM

All constants are documented. Post-layout latency in ns is converted
to CPU cycles using CPU_GHZ.

Input:
    Intel_SGX.csv

Output:
    imcrypto_capacity.pdf
"""

import numpy as np
import pandas as pd
import matplotlib

matplotlib.use("Agg")

import matplotlib.pyplot as plt


# ============================================================
# Model parameters
# ============================================================

CPU_GHZ = 3.2

# Gateway latency components in ns.
T_IN_NS = 10.0
T_OUT_NS = 10.0
T_COMP_NS = 30.0

# Gateway capacity in KB.
CAP_KB = np.array(
    [
        3,
        64,
        1024,
        16 * 1024,
        64 * 1024,
        256 * 1024,
        1024 * 1024,
    ]
)

# SRAM array access/loading latency in CPU cycles.
TPROG_CY = np.array(
    [
        1,
        4,
        8,
        16,
        25,
        45,
        80,
    ]
)

# Resident fraction h(C) of triggered traffic.
H_C = np.array(
    [
        0.02,
        0.15,
        0.35,
        0.60,
        0.80,
        0.92,
        0.98,
    ]
)


# ============================================================
# Load SGX characterization data
# ============================================================

sgx = (
    pd.read_csv("Intel_SGX.csv")
    .query("ht_mode == 'tag+data+resp'")
    .set_index("benchmark")
)

benchmarks = [
    benchmark
    for benchmark in sgx.index
    if benchmark != "triangleCount"
]

HIGHLIGHT = {
    "462libquantum": (
        "462.libquantum (memory-bound)",
        "#d73027",
    ),
    "625x264_s": (
        "625.x264_s (compute-bound)",
        "#4c78a8",
    ),
}


# ============================================================
# Latency model
# ============================================================

def norm_latency(benchmark, t_pim_cy):
    """Calculate normalized AMAT relative to the SGX baseline."""
    row = sgx.loc[benchmark]

    amat = (
        row.tL1_hit_cyc
        + row.m1_L1_miss_rate
        * (
            row.tL2_hit_cyc
            + row.m2_L2_miss_rate
            * (
                row.tL3_hit_cyc
                + row.m3_L3_miss_rate * t_pim_cy
            )
        )
    )

    return amat / row.amat_textbook_cyc


# Fixed gateway cost plus capacity-dependent programming latency.
t_gw_cy = (
    T_IN_NS
    + T_COMP_NS
    + T_OUT_NS
) * CPU_GHZ + TPROG_CY


# ============================================================
# Calculate benchmark curves
# ============================================================

curves = {}

for benchmark in benchmarks:
    t_dram = sgx.loc[benchmark, "dram_avg_cyc"]

    # Gateway access plus DRAM latency for the nonresident fraction.
    t_pim = t_gw_cy + (1.0 - H_C) * t_dram

    curves[benchmark] = np.array(
        [
            norm_latency(benchmark, latency)
            for latency in t_pim
        ]
    )

all_curves = np.vstack(
    list(curves.values())
)

geomean = np.exp(
    np.mean(
        np.log(all_curves),
        axis=0,
    )
)


# ============================================================
# Identify optimum
# ============================================================

opt_index = np.argmin(geomean)
opt = CAP_KB[opt_index]
opt_latency = geomean[opt_index]

if opt >= 1024:
    opt_label = f"Optimum ≈ {opt / 1024:.0f} MB"
else:
    opt_label = f"Optimum ≈ {opt:.0f} KB"


# ============================================================
# Plot
# ============================================================

fig, ax = plt.subplots(
    figsize=(7.2, 4.2)
)

x = CAP_KB


# Benchmark range.
ax.fill_between(
    x,
    all_curves.min(axis=0),
    all_curves.max(axis=0),
    color="#9ecae1",
    alpha=0.35,
    label=f"Range across {len(benchmarks)} benchmarks",
    zorder=1,
)


# Geometric mean.
ax.plot(
    x,
    geomean,
    "-o",
    color="#08519c",
    linewidth=2.4,
    markersize=6,
    label="Geometric mean",
    zorder=4,
)


# Highlight representative benchmarks.
for benchmark, (label, color) in HIGHLIGHT.items():
    if benchmark in curves:
        ax.plot(
            x,
            curves[benchmark],
            linestyle="--",
            color=color,
            linewidth=1.8,
            label=label,
            zorder=3,
        )


# ============================================================
# Axes setup
# ============================================================

ax.set_xscale("log")

# Add modest padding on both sides.
ax.set_xlim(
    CAP_KB[0] / 1.25,
    CAP_KB[-1] * 1.18,
)

ax.set_xticks(CAP_KB)

ax.set_xticklabels(
    [
        "3 KB",
        "64 KB",
        "1 MB",
        "16 MB",
        "64 MB",
        "256 MB",
        "1 GB",
    ]
)


# ============================================================
# Reference lines
# ============================================================

ax.axhline(
    1.0,
    color="black",
    linestyle="-.",
    linewidth=1.1,
    zorder=2,
)

ax.axvline(
    opt,
    color="#1b7f1b",
    linestyle=":",
    linewidth=1.5,
    zorder=2,
)


# ============================================================
# Optimum annotation
# ============================================================

# Place the label on the side that keeps it inside the graph.
# The geometric mean is used because the x-axis is logarithmic.
x_mid = np.sqrt(
    CAP_KB[0] * CAP_KB[-1]
)

if opt >= x_mid:
    # Optimum is in the right half: place label to its left.
    label_x = opt / 1.22
    label_ha = "right"
else:
    # Optimum is in the left half: place label to its right.
    label_x = opt * 1.22
    label_ha = "left"

# x is in data coordinates; y is an axes fraction.
label_transform = ax.get_xaxis_transform()

ax.text(
    label_x,
    0.95,
    opt_label,
    transform=label_transform,
    fontsize=9,
    fontweight="bold",
    color="#1b7f1b",
    ha=label_ha,
    va="top",
    clip_on=False,
)


# ============================================================
# Labels and formatting
# ============================================================

ax.set_xlabel(
    "Gateway on-chip capacity",
    fontsize=11.5,
)

ax.set_ylabel(
    "Normalized memory latency vs. SGX",
    fontsize=11.5,
)

ax.tick_params(
    axis="both",
    which="major",
    labelsize=9,
)

ax.grid(
    which="major",
    linewidth=0.6,
    alpha=0.30,
)

ax.grid(
    which="minor",
    linestyle=":",
    linewidth=0.4,
    alpha=0.16,
)

ax.spines["top"].set_visible(False)
ax.spines["right"].set_visible(False)

ax.legend(
    fontsize=8.3,
    loc="best",
    frameon=True,
    framealpha=0.92,
    edgecolor="#bfbfbf",
)


# ============================================================
# Save
# ============================================================

fig.tight_layout()

fig.savefig(
    "imcrypto_capacity.pdf",
    bbox_inches="tight",
)

fig.savefig(
    "imcrypto_capacity.png",
    bbox_inches="tight",
    dpi=300,
)

plt.close(fig)


# ============================================================
# Summary
# ============================================================

print(
    f"Optimum: {opt} KB; "
    f"geometric mean at optimum: {opt_latency:.3f}"
)