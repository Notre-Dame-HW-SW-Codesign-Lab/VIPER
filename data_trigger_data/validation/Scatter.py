import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
import numpy as np
from scipy import stats

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
benchmarks = [b for b in data["Baseline_inorder"].index if b != "triangleCount"]

# configs (same as before)
inorder_tpim = [("C1","C1_5ns_1MB_inorder","Baseline_inorder"),
                ("C3","C3_50ns_1MB_inorder","Baseline_inorder"),
                ("C5","C5_500ns_1MB_inorder","Baseline_inorder")]
inorder_cap = [("C7","C7_50ns_64MB_inorder","Baseline_inorder"),
               ("C8","C8_50ns_256MB_inorder","Baseline_inorder"),
               ("C9","C9_50ns_4GB_inorder","Baseline_inorder")]
inorder_storage = [("C10","C10_500ns_100GB_inorder","Baseline_inorder")]

ooo_tpim = [("C2","C2_5ns_1MB_OoO","Baseline_OoO"),
            ("C4","C4_50ns_1MB_OoO","Baseline_OoO"),
            ("C6","C6_500ns_1MB_OoO","Baseline_OoO")]
ooo_storage = [("C11","C11_500ns_100GB_OoO","Baseline_OoO")]

COLORS = {"tpim":"#2166ac","cap":"#d73027","storage":"#4dac26"}
MARKERS = {"tpim":"o","cap":"s","storage":"^"}

# 🔥 MUCH BIGGER
TITLE_FONT  = 30
LABEL_FONT  = 26
TICK_FONT   = 22
LEGEND_FONT = 22
STAT_FONT   = 22

SCATTER_SIZE = 240
EDGE_WIDTH   = 1.6
LINE_WIDTH   = 2.8

def get_points(configs):
    xs, ys = [], []
    for _, cfg_key, base_key in configs:
        for bench in benchmarks:
            bg = data[base_key].loc[bench,"amat_stats_cyc"]
            bv = data[base_key].loc[bench,"amat_textbook_cyc"]
            xs.append(data[cfg_key].loc[bench,"amat_stats_cyc"]/bg)
            ys.append(data[cfg_key].loc[bench,"amat_textbook_cyc"]/bv)
    return np.array(xs), np.array(ys)

# 🔥 larger canvas
fig = plt.figure(figsize=(20, 9))
fig.patch.set_facecolor("white")

gs = gridspec.GridSpec(
    1, 2,
    figure=fig,
    wspace=0.14,
    left=0.08,
    right=0.98,
    top=0.92,
    bottom=0.14
)

panels = [
    ("In-Order CPU",
     [(inorder_tpim,"tpim","$t_{PIM}$ sweep (C1,C3,C5)"),
      (inorder_cap,"cap","Capacity sweep (C7,C8,C9)"),
      (inorder_storage,"storage","PIM in storage (C10)")]),
    ("Out-of-Order CPU",
     [(ooo_tpim,"tpim","$t_{PIM}$ sweep (C2,C4,C6)"),
      (ooo_storage,"storage","PIM in storage (C11)")])
]

for i, (title, groups) in enumerate(panels):
    ax = fig.add_subplot(gs[i])

    all_x, all_y = [], []

    for configs, grp, label in groups:
        xs, ys = get_points(configs)
        all_x.extend(xs); all_y.extend(ys)

        ax.scatter(xs, ys,
                   color=COLORS[grp],
                   marker=MARKERS[grp],
                   s=SCATTER_SIZE,
                   edgecolors="black",
                   linewidths=EDGE_WIDTH,
                   alpha=0.95,
                   label=label)

    all_x, all_y = np.array(all_x), np.array(all_y)

    lim_min = min(all_x.min(), all_y.min()) * 0.98
    lim_max = max(all_x.max(), all_y.max()) * 1.02

    ax.plot([lim_min, lim_max], [lim_min, lim_max],
            "k--", lw=LINE_WIDTH, label="Perfect prediction")

    r, _ = stats.pearsonr(all_x, all_y)
    mae = np.mean(np.abs(all_x - all_y))
    n   = len(all_x)

    # n is shown so the two panels are visibly independent computations:
    # in-order (7 configs x 12 benchmarks, n=84) and OoO (4 x 12, n=48)
    # coincidentally yield MAEs that agree to four decimals
    # (0.114040 vs 0.113971).
    ax.text(0.04, 0.95,
            f"r = {r:.4f}\nMAE = {mae:.4f}\nn = {n}",
            transform=ax.transAxes,
            fontsize=STAT_FONT,
            va="top",
            bbox=dict(boxstyle="round,pad=0.5", fc="white", ec="gray"))

    ax.set_xlim(lim_min, lim_max)
    ax.set_ylim(lim_min, lim_max)

    ax.set_title(title, fontsize=TITLE_FONT, fontweight="bold")
    ax.set_xlabel("gem5 ratio", fontsize=LABEL_FONT)

    if i == 0:
        ax.set_ylabel("VIPER ratio", fontsize=LABEL_FONT)

    ax.tick_params(axis='both', labelsize=TICK_FONT)
    ax.grid(True, alpha=0.3, lw=0.7)

    ax.legend(fontsize=LEGEND_FONT, loc="lower right")

plt.savefig("scatter_validation.pdf", dpi=300)
print("Done")