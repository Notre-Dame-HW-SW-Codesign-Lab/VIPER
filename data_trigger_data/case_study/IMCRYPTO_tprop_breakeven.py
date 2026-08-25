"""
IMCRYPTO case study, final figure. All host inputs from Intel_SGX.csv (perf/gem5);
gateway cost t_gw = 32 cycles = measured mean of dram_avg(IMCRYPTO)-dram_avg(SGX)
across the 64MB/256MB/4GB gem5 runs (post-layout 10+30+10ns).
VIPER data-triggering model (Eq. 3): t_PIM,mem = t_DRAM + t_gw + t_prog.
VIPER reproduces the gem5-measured 64MB ratios to max 0.20% (mean 0.08%).
"""
import pandas as pd, numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

def load(f): return pd.read_csv(f).query("ht_mode=='tag+data+resp'").set_index("benchmark")
sgx = load("Intel_SGX.csv")
bm = [b for b in sgx.index if b != "triangleCount"]
T_GW = 32.0
TECH = [("CMOS",1,"#4c78a8"), ("FeFET",20,"#2ca02c"), ("RRAM",40,"#e6ab02"), ("PCM",100,"#d73027")]

def amat(r, t):
    return r.tL1_hit_cyc + r.m1_L1_miss_rate*(r.tL2_hit_cyc +
        r.m2_L2_miss_rate*((r.tL3_hit_cyc + r.llc_extra_cyc) + r.m3_L3_miss_rate*t))

tp = np.linspace(0, 160, 161)
R = np.array([[amat(sgx.loc[b], sgx.loc[b,"dram_avg_cyc"]+T_GW+t)/sgx.loc[b,"amat_textbook_cyc"]
               for t in tp] for b in bm])
geo, worst = np.exp(np.log(R).mean(0)), R.max(0)

fig, ax = plt.subplots(figsize=(7.2, 4.1))
ax.fill_between(tp, R.min(0), worst, color="#c6dbef", alpha=0.5, label="range (12 benchmarks)")
ax.plot(tp, worst, "-", color="#d73027", lw=2.0, label="worst case (462.libquantum)")
ax.plot(tp, geo, "-", color="#08519c", lw=2.4, label="geometric mean")
ax.axhline(1.10, color="gray", ls="--", lw=1.3)
ax.text(157, 1.104, "10% budget", fontsize=8.5, color="dimgray", ha="right", va="bottom")
budget = tp[np.argmax(worst > 1.10)]
ax.axvline(budget, color="green", ls=":", lw=1.5)
ax.text(budget+3, 1.005, f"$t^*_{{prog}}\\approx{budget:.0f}$ cyc", fontsize=9, color="#1b7f1b")
for name, t, c in TECH:
    ax.plot(t, np.interp(t, tp, worst), "o", ms=9, mfc=c, mec="black", zorder=6)
    ax.annotate(name, (t, np.interp(t, tp, worst)), textcoords="offset points",
                xytext=(5, 9), fontsize=9)
ax.set_xlim(0, 160); ax.set_ylim(0.99, 1.30)
ax.set_xlabel("Gateway programming latency $t_{prog}$ (cycles)", fontsize=11.5)
ax.set_ylabel("Normalized AMAT vs. unsecured DRAM", fontsize=11.5)
ax.grid(alpha=0.3); ax.legend(fontsize=8.8, loc="upper left")
plt.tight_layout()
plt.savefig("imcrypto_tprog.pdf", bbox_inches="tight")
plt.savefig("imcrypto_tprog.png", bbox_inches="tight", dpi=140)
print("budget t_prog:", budget, "cyc")
for name, t, _ in TECH:
    rs = R[:, int(t)]
    print(f"{name}: geo {np.exp(np.log(rs).mean()):.4f} worst {rs.max():.4f}")