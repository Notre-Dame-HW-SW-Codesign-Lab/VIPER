import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

base = ""

sgx = pd.read_csv(base + "Intel_SGX.csv").query("ht_mode == 'tag+data+resp'").set_index("benchmark")

benchmarks = [b for b in sgx.index if b != "triangleCount"]

full_labels = {
    "433milc":        "433.milc",
    "462libquantum":  "462.libquantum",
    "473astar":       "473.astar",
    "605mcf_s":       "605.mcf_s",
    "619lbm_s":       "619.lbm_s",
    "620onmetpp_s":   "620.omnetpp_s",
    "625x264_s":      "625.x264_s",
    "638imagick_s":   "638.imagick_s",
    "degreeCentr":    "degreeCentr",
    "graphColoring":  "graphColoring",
    "kCore":          "kCore",
    "pageRank":       "pageRank",
}

# --------------------------------------------------
# Set PIM memory latency for each technology here
# Replace these numbers with your actual values if needed
# --------------------------------------------------
t_pim_mem_cfg = {
    "IMCRYPTO CMOS":  32.0 + 200,
    "IMCRYPTO RRAM":  30.0 + 40+200,
    "IMCRYPTO FeFET": 30.0 +20+ 200,
    "IMCRYPTO PCM":   30.0 +100+ 200,
}

def compute_norm_latency(t_pim_mem):
    amat_norm = []
    for bench in benchmarks:
        tL1 = sgx.loc[bench, "tL1_hit_cyc"]
        tL2 = sgx.loc[bench, "tL2_hit_cyc"]
        tL3 = sgx.loc[bench, "tL3_hit_cyc"]
        m1  = sgx.loc[bench, "m1_L1_miss_rate"]
        m2  = sgx.loc[bench, "m2_L2_miss_rate"]
        m3  = sgx.loc[bench, "m3_L3_miss_rate"]

        amat_base = sgx.loc[bench, "amat_textbook_cyc"]
        amat_v = tL1 + m1 * (tL2 + m2 * (tL3 + m3 * t_pim_mem))
        amat_norm.append(amat_v / amat_base)

    return amat_norm

# Compute results for each IMCRYPTO technology
imcrypto_results = {
    label: compute_norm_latency(t_pim_mem)
    for label, t_pim_mem in t_pim_mem_cfg.items()
}

bench_labels = [full_labels[b] for b in benchmarks]
x = np.arange(len(benchmarks))

TITLE_FONT  = 22
LABEL_FONT  = 22
TICK_FONT   = 22
LEGEND_FONT = 30

BAR_W = 0.16

COLORS = {
    "Baseline":       "#2166ac",  # keep same as before
    "IMCRYPTO CMOS":  "#d73027",  # keep same as before
    "IMCRYPTO RRAM":  "#4C78A8",   # muted blue (different from baseline)
    "IMCRYPTO FeFET": "#7f7f7f",   # neutral gray
    "IMCRYPTO PCM":   "#5b3c88",   # dark purple
}

fig, ax = plt.subplots(figsize=(20, 7))
fig.patch.set_facecolor("white")

# 5 bars per benchmark
offsets = [-2*BAR_W, -1*BAR_W, 0, 1*BAR_W, 2*BAR_W]

ax.bar(
    x + offsets[0], [1.0] * len(benchmarks), BAR_W,
    label="Baseline",
    color=COLORS["Baseline"],
    edgecolor="black",
    lw=1.0
)

ax.bar(
    x + offsets[1], imcrypto_results["IMCRYPTO CMOS"], BAR_W,
    label="IMCRYPTO CMOS",
    color=COLORS["IMCRYPTO CMOS"],
    edgecolor="black",
    lw=1.0
)

ax.bar(
    x + offsets[2], imcrypto_results["IMCRYPTO RRAM"], BAR_W,
    label="IMCRYPTO RRAM",
    color=COLORS["IMCRYPTO RRAM"],
    edgecolor="black",
    lw=1.0
)

ax.bar(
    x + offsets[3], imcrypto_results["IMCRYPTO FeFET"], BAR_W,
    label="IMCRYPTO FeFET",
    color=COLORS["IMCRYPTO FeFET"],
    edgecolor="black",
    lw=1.0
)

ax.bar(
    x + offsets[4], imcrypto_results["IMCRYPTO PCM"], BAR_W,
    label="IMCRYPTO PCM",
    color=COLORS["IMCRYPTO PCM"],
    edgecolor="black",
    lw=1.0
)

ax.axhline(1.0, color="gray", lw=1.5, ls="--", alpha=0.7)

ax.set_xticks(x)
ax.set_xticklabels(
    bench_labels,
    fontsize=TICK_FONT,
    rotation=35,
    ha="right",
    fontweight="bold"
)

ax.set_ylabel("Normalized Latency", fontsize=LABEL_FONT)

ax.legend(
    fontsize=LEGEND_FONT,
    loc="upper center",
    ncol=3,
    bbox_to_anchor=(0.5, 1.30),
    frameon=True
)

ax.tick_params(axis="y", labelsize=TICK_FONT)
ax.grid(axis="y", alpha=0.3)

# Set y-limit using all values
all_vals = [1.0] * len(benchmarks)
for vals in imcrypto_results.values():
    all_vals.extend(vals)

ax.set_ylim(0, max(all_vals) * 1.18)

plt.subplots_adjust(top=0.80)
plt.savefig("imcrypto_technology.pdf", bbox_inches="tight", dpi=300)
print("Done")