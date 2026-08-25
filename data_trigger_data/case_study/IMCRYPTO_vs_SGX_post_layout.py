import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

base = ""

sgx  = pd.read_csv(base + "Intel_SGX.csv").query("ht_mode == 'tag+data+resp'").set_index("benchmark")
c64  = pd.read_csv(base + "IMCRYPTO_64MB.csv").query("ht_mode == 'tag+data+resp'").set_index("benchmark")

t_pim_mem = 32.0 + 200

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

amat_norm = []
for bench in benchmarks:
    tL1 = sgx.loc[bench, "tL1_hit_cyc"]
    tL2 = sgx.loc[bench, "tL2_hit_cyc"]
    tL3 = sgx.loc[bench, "tL3_hit_cyc"]
    m1  = sgx.loc[bench, "m1_L1_miss_rate"]
    m2  = sgx.loc[bench, "m2_L2_miss_rate"]
    m3  = sgx.loc[bench, "m3_L3_miss_rate"]
    amat_base = sgx.loc[bench, "amat_textbook_cyc"]
    amat_v    = tL1 + m1 * (tL2 + m2 * (tL3 + m3 * t_pim_mem))
    amat_norm.append(amat_v / amat_base)

bench_labels = [full_labels[b] for b in benchmarks]
x = np.arange(len(benchmarks))

# 🔥 Bigger fonts
TITLE_FONT = 22
LABEL_FONT = 22
TICK_FONT  = 22
LEGEND_FONT = 30

BAR_W = 0.35

COL_SGX   = "#2166ac"
COL_VIPER = "#d73027"

fig, ax = plt.subplots(figsize=(20, 7))
fig.patch.set_facecolor("white")

ax.bar(x - BAR_W/2, [1.0]*len(benchmarks), BAR_W,
       label="Intel SGX", color=COL_SGX, edgecolor="black", lw=1.0)

ax.bar(x + BAR_W/2, amat_norm, BAR_W,
       label="IMCRYPTO", color=COL_VIPER, edgecolor="black", lw=1.0)

ax.axhline(1.0, color="gray", lw=1.5, ls="--", alpha=0.7)

ax.set_xticks(x)
ax.set_xticklabels(
    bench_labels,
    fontsize=TICK_FONT,
    rotation=35,
    ha="right",
    fontweight="bold"   # 🔥 bold labels
)

ax.set_ylabel("Normalized Latency", fontsize=LABEL_FONT)

# 🔥 Legend at top center, single row
ax.legend(
    fontsize=LEGEND_FONT,
    loc="upper center",
    ncol=2,
    bbox_to_anchor=(0.5, 1.18),
    frameon=True
)

ax.tick_params(axis='y', labelsize=TICK_FONT)
ax.grid(axis='y', alpha=0.3)

ax.set_ylim(0, max(amat_norm) * 1.18)

# for i, v in enumerate(amat_norm):
#     if v > 1.05 or v < 0.95:
#         ax.text(
#             x[i] + BAR_W/2,
#             v + 0.02,
#             f"{v:.2f}",
#             ha="center",
#             va="bottom",
#             fontsize=TICK_FONT,
#             fontweight="bold",
#             color=COL_VIPER
#         )

# leave space for legend
plt.subplots_adjust(top=0.82)

plt.savefig("imcrypto_latency.pdf", bbox_inches="tight", dpi=300)
print("Done")