import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
import numpy as np
from scipy.stats import pearsonr

# ─── Configuration ────────────────────────────────────────────────────────────

MODE = 'tag+data'  # change to 'tag+data+resp' if desired

SETUPS = [
    ('Baseline', 'Baseline_inorder.csv',       'Baseline_no_pim_OoO.csv'),
    ('40ns',     '40ns_no_pim_inorder.csv',    '40ns_no_pim_OoO.csv'),
    ('40ns_2GB', '40ns_2GB_pim_inorder.csv',   '40ns_2GB_OoO.csv'),
]

# ─── Load & filter data ───────────────────────────────────────────────────────

inorder_data = {}
ooo_data     = {}

for label, inorder_file, ooo_file in SETUPS:
    df_in = pd.read_csv(inorder_file)
    df_oo = pd.read_csv(ooo_file)
    inorder_data[label] = df_in[df_in['ht_mode'] == MODE].set_index('benchmark')
    ooo_data[label]     = df_oo[df_oo['ht_mode'] == MODE].set_index('benchmark')

benchmarks_in = sorted(set.intersection(*[set(df.index) for df in inorder_data.values()]))
benchmarks_oo = sorted(set.intersection(*[set(df.index) for df in ooo_data.values()]))

setup_labels = [s[0] for s in SETUPS]

# ─── Build normalized ratio arrays ───────────────────────────────────────────
# For each benchmark, compute ratio vs Baseline (i.e. Baseline=1, 40ns=?, 40ns_2GB=?)
# Then flatten across benchmarks*setups → one value per (benchmark, setup) point

def get_ratios(data_dict, benchmarks, metric_col):
    """
    Returns array of shape (n_benchmarks, n_setups) with values normalized
    so that Baseline = 1.0 per benchmark.
    """
    baseline_df = data_dict['Baseline']
    rows = []
    for b in benchmarks:
        baseline_val = baseline_df.loc[b, metric_col]
        row = [data_dict[s].loc[b, metric_col] / baseline_val for s in setup_labels]
        rows.append(row)
    return np.array(rows)  # shape: (n_benchmarks, n_setups)

def build_flat(data_dict, benchmarks):
    """Returns flattened 1D arrays for stat, textbook, mshr across all benchmarks×setups."""
    stat     = get_ratios(data_dict, benchmarks, 'amat_stats_cyc').flatten()
    textbook = get_ratios(data_dict, benchmarks, 'amat_textbook_cyc').flatten()
    mshr     = get_ratios(data_dict, benchmarks, 'amat_textbook_mshr_cyc').flatten()
    return stat, textbook, mshr

stat_in, tb_in, mshr_in = build_flat(inorder_data, benchmarks_in)
stat_oo, tb_oo, mshr_oo = build_flat(ooo_data,     benchmarks_oo)

# ─── Metric computation ───────────────────────────────────────────────────────

def compute_metrics(stat, other):
    corr, _  = pearsonr(stat, other)
    mae      = np.mean(np.abs(stat - other))
    rmse     = np.sqrt(np.mean((stat - other) ** 2))
    max_err  = np.max(np.abs(stat - other))
    return dict(Pearson_r=corr, MAE=mae, RMSE=rmse, Max_Err=max_err)

results = {
    'In-Order  | Textbook':      compute_metrics(stat_in, tb_in),
    'In-Order  | Textbook+MSHR': compute_metrics(stat_in, mshr_in),
    'Out-of-Order | Textbook':      compute_metrics(stat_oo, tb_oo),
    'Out-of-Order | Textbook+MSHR': compute_metrics(stat_oo, mshr_oo),
}

print("\n=== Trend Similarity Metrics (normalized ratios, all benchmarks × setups) ===")
print(f"{'Config':<35} {'Pearson r':>10} {'MAE':>8} {'RMSE':>8} {'Max Err':>9}")
print("-" * 75)
for name, m in results.items():
    print(f"{name:<35} {m['Pearson_r']:>10.4f} {m['MAE']:>8.4f} {m['RMSE']:>8.4f} {m['Max_Err']:>9.4f}")

# ─── Figure 1: Scatter plots (trend alignment) ───────────────────────────────
# x = amat_stats ratio, y = textbook or mshr ratio
# Perfect alignment = diagonal y=x

fig, axes = plt.subplots(2, 2, figsize=(13, 11))
fig.suptitle(
    'Trend Alignment: AMAT_textbook vs AMAT_textbook_MSHR relative to AMAT_stat\n'
    '(Each point = one benchmark×setup ratio; diagonal = perfect agreement)',
    fontsize=12, fontweight='bold'
)

# Color points by setup index so we can see Baseline/40ns/40ns_2GB clustering
n_b_in = len(benchmarks_in)
n_b_oo = len(benchmarks_oo)
colors_in = np.tile(['steelblue', 'tomato', 'mediumseagreen'], n_b_in)
colors_oo = np.tile(['steelblue', 'tomato', 'mediumseagreen'], n_b_oo)

panels = [
    (axes[0, 0], stat_in, tb_in,   colors_in, 'In-Order',     'AMAT_textbook',      'tomato'),
    (axes[0, 1], stat_in, mshr_in, colors_in, 'In-Order',     'AMAT_textbook_MSHR', 'mediumseagreen'),
    (axes[1, 0], stat_oo, tb_oo,   colors_oo, 'Out-of-Order', 'AMAT_textbook',      'tomato'),
    (axes[1, 1], stat_oo, mshr_oo, colors_oo, 'Out-of-Order', 'AMAT_textbook_MSHR', 'mediumseagreen'),
]

for ax, x_arr, y_arr, cols, proc, variant, col in panels:
    ax.scatter(x_arr, y_arr, c=cols, s=60, edgecolors='black', linewidths=0.4, zorder=3)

    # y=x perfect alignment line
    lim_min = min(x_arr.min(), y_arr.min()) * 0.95
    lim_max = max(x_arr.max(), y_arr.max()) * 1.05
    ax.plot([lim_min, lim_max], [lim_min, lim_max], 'k--', linewidth=1.2,
            label='Perfect agreement (y=x)')

    # Pearson r annotation
    r, _ = pearsonr(x_arr, y_arr)
    mae  = np.mean(np.abs(x_arr - y_arr))
    ax.text(0.05, 0.92, f'Pearson r = {r:.4f}\nMAE = {mae:.4f}',
            transform=ax.transAxes, fontsize=9,
            bbox=dict(boxstyle='round,pad=0.3', facecolor='white', alpha=0.7))

    ax.set_xlabel('AMAT_stat ratio (vs Baseline)', fontsize=10)
    ax.set_ylabel(f'{variant} ratio (vs Baseline)', fontsize=10)
    ax.set_title(f'{proc} — {variant}', fontsize=10, fontweight='bold')
    ax.set_xlim(lim_min, lim_max)
    ax.set_ylim(lim_min, lim_max)
    ax.grid(alpha=0.3)

    # Legend for setup colors
    from matplotlib.patches import Patch
    legend_elements = [
        Patch(facecolor='steelblue',    edgecolor='black', label='Baseline'),
        Patch(facecolor='tomato',       edgecolor='black', label='40ns'),
        Patch(facecolor='mediumseagreen', edgecolor='black', label='40ns_2GB'),
    ]
    ax.legend(handles=legend_elements, fontsize=8, loc='lower right')

plt.tight_layout()
plt.savefig('AMAT_trend_scatter.png', dpi=150, bbox_inches='tight')
plt.close()
print('\nSaved: AMAT_trend_scatter.png')

# ─── Figure 2: Per-benchmark trend line comparison ───────────────────────────
# For each processor type, show 3 metrics as line plots across setups
# One subplot per benchmark — shows whether the 3 lines move together

for proc_label, data_dict, benchmarks in [
    ('In-Order',     inorder_data, benchmarks_in),
    ('Out-of-Order', ooo_data,     benchmarks_oo),
]:
    n = len(benchmarks)
    ncols = 4
    nrows = int(np.ceil(n / ncols))

    fig, axes = plt.subplots(nrows, ncols, figsize=(16, nrows * 3.5), sharey=False)
    fig.suptitle(
        f'Per-Benchmark Trend Lines — {proc_label}  (ht_mode={MODE})\n'
        f'Normalized to Baseline=1.0 | Lines should move together if textbook tracks stat',
        fontsize=12, fontweight='bold'
    )
    axes_flat = axes.flatten()

    x_ticks = np.arange(len(setup_labels))

    for idx, b in enumerate(benchmarks):
        ax = axes_flat[idx]
        baseline_in = data_dict['Baseline'].loc[b]

        ratios_stat     = [data_dict[s].loc[b, 'amat_stats_cyc']          / baseline_in['amat_stats_cyc']          for s in setup_labels]
        ratios_textbook = [data_dict[s].loc[b, 'amat_textbook_cyc']       / baseline_in['amat_textbook_cyc']       for s in setup_labels]
        ratios_mshr     = [data_dict[s].loc[b, 'amat_textbook_mshr_cyc']  / baseline_in['amat_textbook_mshr_cyc']  for s in setup_labels]

        ax.plot(x_ticks, ratios_stat,     'o-',  color='steelblue',     linewidth=2,   label='AMAT_stat',      markersize=6)
        ax.plot(x_ticks, ratios_textbook, 's--', color='tomato',        linewidth=1.5, label='AMAT_textbook',  markersize=5)
        ax.plot(x_ticks, ratios_mshr,     '^:',  color='mediumseagreen',linewidth=1.5, label='AMAT_MSHR',      markersize=5)

        ax.set_xticks(x_ticks)
        ax.set_xticklabels(setup_labels, fontsize=8)
        ax.set_title(b, fontsize=9, fontweight='bold')
        ax.set_ylabel('Normalized ratio', fontsize=8)
        ax.axhline(1.0, color='grey', linestyle='--', linewidth=0.7, alpha=0.6)
        ax.grid(alpha=0.3)

        if idx == 0:
            ax.legend(fontsize=7, loc='best')

    # Hide unused subplots
    for j in range(n, len(axes_flat)):
        axes_flat[j].set_visible(False)

    plt.tight_layout()
    safe = proc_label.replace('-', '').replace(' ', '_')
    fname = f'AMAT_trend_lines_{safe}.png'
    plt.savefig(fname, dpi=150, bbox_inches='tight')
    plt.close()
    print(f'Saved: {fname}')

# ─── Figure 3: Summary bar chart — MAE per benchmark ─────────────────────────
# Shows which benchmarks have highest disagreement between textbook variants and stat

fig, axes = plt.subplots(1, 2, figsize=(15, 5))
fig.suptitle(
    'Per-Benchmark MAE of Normalized Ratios vs AMAT_stat\n'
    '(lower = trend of textbook variant tracks stat more closely)',
    fontsize=12, fontweight='bold'
)

for ax, data_dict, benchmarks, proc_label in [
    (axes[0], inorder_data, benchmarks_in, 'In-Order'),
    (axes[1], ooo_data,     benchmarks_oo, 'Out-of-Order'),
]:
    mae_tb   = []
    mae_mshr = []

    for b in benchmarks:
        baseline = data_dict['Baseline'].loc[b]
        s_vals  = np.array([data_dict[s].loc[b, 'amat_stats_cyc']         / baseline['amat_stats_cyc']         for s in setup_labels])
        tb_vals = np.array([data_dict[s].loc[b, 'amat_textbook_cyc']      / baseline['amat_textbook_cyc']      for s in setup_labels])
        ms_vals = np.array([data_dict[s].loc[b, 'amat_textbook_mshr_cyc'] / baseline['amat_textbook_mshr_cyc'] for s in setup_labels])

        mae_tb.append(np.mean(np.abs(s_vals - tb_vals)))
        mae_mshr.append(np.mean(np.abs(s_vals - ms_vals)))

    x = np.arange(len(benchmarks))
    w = 0.35
    ax.bar(x - w/2, mae_tb,   w, label='AMAT_textbook',      color='tomato',         edgecolor='black', linewidth=0.5)
    ax.bar(x + w/2, mae_mshr, w, label='AMAT_textbook_MSHR', color='mediumseagreen', edgecolor='black', linewidth=0.5)

    ax.set_xticks(x)
    ax.set_xticklabels(benchmarks, rotation=35, ha='right', fontsize=9)
    ax.set_ylabel('MAE of normalized ratio', fontsize=10)
    ax.set_title(proc_label, fontsize=11)
    ax.legend(fontsize=9)
    ax.grid(axis='y', alpha=0.3)
    ax.set_ylim(0, max(max(mae_tb), max(mae_mshr)) * 1.2)

plt.tight_layout()
plt.savefig('AMAT_trend_MAE_perbenchmark.png', dpi=150, bbox_inches='tight')
plt.close()
print('Saved: AMAT_trend_MAE_perbenchmark.png')