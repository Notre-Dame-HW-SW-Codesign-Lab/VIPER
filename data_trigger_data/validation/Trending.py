import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec

base = ""

files = {
    "Baseline_inorder":        "Baseline_inorder.csv",
    "Baseline_OoO":            "Baseline_OoO.csv",
    "C1_5ns_1MB_inorder":      "inorder_5ns_1MB.csv",
    "C2_5ns_1MB_OoO":          "OoO_5ns_1MB.csv",
    "C3_50ns_1MB_inorder":     "inorder_50ns_1MB.csv",
    "C4_50ns_1MB_OoO":         "OoO_50ns_1MB.csv",
    "C5_500ns_1MB_inorder":    "inorder_500ns_1MB.csv",
    "C6_500ns_1MB_OoO":        "OoO_500ns_1MB.csv",
    "C7_50ns_64MB_inorder":    "inorder_50ns_64MB.csv",
    "C8_50ns_256MB_inorder":   "inorder_50ns_256MB.csv",
    "C9_50ns_4GB_inorder":     "inorder_50ns_4GB.csv",
    "C10_500ns_100GB_inorder": "inorder_500ns_100GB.csv",
    "C11_500ns_100GB_OoO":     "OoO_500ns_100GB.csv",
}

def load(fname):
    df = pd.read_csv(base + fname)
    df = df[df["ht_mode"] == "tag+data+resp"][["benchmark","amat_stats_cyc","amat_textbook_cyc"]]
    return df.set_index("benchmark")

data = {k: load(v) for k, v in files.items()}
benchmarks = [b for b in data["Baseline_inorder"].index if b != "triangleCount"][:12]

inorder_configs = [
    ("BL","Baseline_inorder"), ("C1","C1_5ns_1MB_inorder"),
    ("C3","C3_50ns_1MB_inorder"), ("C5","C5_500ns_1MB_inorder"),
    ("C7","C7_50ns_64MB_inorder"), ("C8","C8_50ns_256MB_inorder"),
    ("C9","C9_50ns_4GB_inorder"), ("C10","C10_500ns_100GB_inorder"),
]

ooo_configs = [
    ("BL","Baseline_OoO"), ("C2","C2_5ns_1MB_OoO"),
    ("C4","C4_50ns_1MB_OoO"), ("C6","C6_500ns_1MB_OoO"),
    ("C11","C11_500ns_100GB_OoO"),
]

# GEM5_INORDER  = "#2166ac"
# VIPER_INORDER = "#d73027"
# GEM5_OOO      = "#4dac26"
# VIPER_OOO     = "#e08214"

GEM5_INORDER  = "#2166ac"
VIPER_INORDER = "#2166ac"
GEM5_OOO      = "#4dac26"
VIPER_OOO     = "#4dac26"


# 🔥 BIGGER FONTS
TITLE_FONT  = 24
LABEL_FONT  = 20
TICK_FONT   = 17
LEGEND_FONT = 20

fig = plt.figure(figsize=(22, 28))  # taller for readability

gs = gridspec.GridSpec(
    4, 3,
    figure=fig,
    hspace=0.22,   # tighter
    wspace=0.15,
    top=0.92,
    bottom=0.06,
    left=0.07,
    right=0.99
)

for idx, bench in enumerate(benchmarks):
    row, col = divmod(idx, 3)
    ax = fig.add_subplot(gs[row, col])

    base_g = data["Baseline_inorder"].loc[bench, "amat_stats_cyc"]
    base_v = data["Baseline_inorder"].loc[bench, "amat_textbook_cyc"]

    gem5_io  = [data[k].loc[bench,"amat_stats_cyc"]/base_g for _,k in inorder_configs]
    viper_io = [data[k].loc[bench,"amat_textbook_cyc"]/base_v for _,k in inorder_configs]

    ax.plot(range(len(inorder_configs)), gem5_io,
            color=GEM5_INORDER, lw=3.2, marker="o", ms=9)

    ax.plot(range(len(inorder_configs)), viper_io,
            color=VIPER_INORDER, lw=3.2, ls="--", marker="s", ms=9)

    base_g2 = data["Baseline_OoO"].loc[bench, "amat_stats_cyc"]
    base_v2 = data["Baseline_OoO"].loc[bench, "amat_textbook_cyc"]

    gem5_ooo  = [data[k].loc[bench,"amat_stats_cyc"]/base_g2 for _,k in ooo_configs]
    viper_ooo = [data[k].loc[bench,"amat_textbook_cyc"]/base_v2 for _,k in ooo_configs]

    ax2 = ax.twiny()
    ax2.plot(range(len(ooo_configs)), gem5_ooo,
             color=GEM5_OOO, lw=3.2, marker="^", ms=9)

    ax2.plot(range(len(ooo_configs)), viper_ooo,
             color=VIPER_OOO, lw=3.2, ls="--", marker="D", ms=9)

    ax.set_xticks(range(len(inorder_configs)))
    ax.set_xticklabels([l for l,_ in inorder_configs],
                       fontsize=TICK_FONT, color=GEM5_INORDER)

    ax2.set_xticks(range(len(ooo_configs)))
    ax2.set_xticklabels([l for l,_ in ooo_configs],
                        fontsize=TICK_FONT, color=GEM5_OOO)

    ax.set_title(bench, fontsize=TITLE_FONT, fontweight="bold", pad=8)

    # 🔥 only left column keeps y-label
    if col == 0:
        ax.set_ylabel("Norm. Latency", fontsize=LABEL_FONT)
    else:
        ax.set_ylabel("")

    ax.axhline(1.0, color="gray", lw=1.2, ls=":")
    ax.grid(True, alpha=0.3)
    ax.tick_params(axis='y', labelsize=TICK_FONT)

# Legend
handles = [
    plt.Line2D([0],[0], color=GEM5_INORDER, lw=3.2, marker="o", label="gem5 In-Order"),
    plt.Line2D([0],[0], color=VIPER_INORDER, lw=3.2, ls="--", marker="s", label="VIPER In-Order"),
    plt.Line2D([0],[0], color=GEM5_OOO, lw=3.2, marker="^", label="gem5 OoO"),
    plt.Line2D([0],[0], color=VIPER_OOO, lw=3.2, ls="--", marker="D", label="VIPER OoO"),
]

fig.legend(
    handles=handles,
    loc="upper center",
    ncol=4,
    fontsize=LEGEND_FONT,
    bbox_to_anchor=(0.5, 0.985),
    frameon=True
)

plt.savefig("trend_per_benchmark.pdf", dpi=300)
print("Done")