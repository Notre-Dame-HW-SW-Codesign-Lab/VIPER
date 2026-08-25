#!/usr/bin/env python3
"""Real-hardware figures for the VIPER UPMEM case study.
Baseline and offload both measured on the UPMEM host (Xeon Silver 4215 +
2,560 DPUs). Reads sweep_final.dat, writes 3 figures into this directory."""
import numpy as np
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt

T_FULL, T_STUB = 20.37, 3.10                 # measured on UPMEM host (ms)
MACS = 25_165_824
Tk1_pub, Tmem_pub = MACS/45e6*1e3, 1.40      # VIPER published-constant inputs

d = np.array([[float(x) for x in l.split()]
              for l in open("sweep_final.dat") if l.strip() and not l.startswith("#")])
alloc, tin, tl, tout, tmm = d[:,1], d[:,3], d[:,4], d[:,5], d[:,6]
sp_hw   = T_FULL / (T_STUB + tmm)
ceiling = T_FULL / T_STUB
ipk     = int(np.argmax(sp_hw))
N = np.logspace(0, np.log10(2560), 400)
def sp_pred(n): return T_FULL / (T_STUB + Tk1_pub/n + Tmem_pub)

# N-dependent transfer: affine fit T_transfer(N) = t0 + alpha*N on the rising
# region (N>=64) where per-DPU scatter/gather overhead dominates.
_m = alloc >= 64
_t0, _alpha = np.linalg.lstsq(np.vstack([np.ones(_m.sum()), alloc[_m]]).T,
                              (tin + tout)[_m], rcond=None)[0]
def t_transfer(n): return _t0 + _alpha * n
def sp_pred_nf(n): return T_FULL / (T_STUB + Tk1_pub/n + t_transfer(n))
Nstar = (Tk1_pub / _alpha) ** 0.5           # closed-form optimum sqrt(Tk1/alpha)

plt.rcParams.update({"font.size":16,"axes.titlesize":17,"axes.labelsize":16,
                     "legend.fontsize":12,"xtick.labelsize":14,"ytick.labelsize":14})

# Figure 1: speedup, VIPER prediction vs measured
fig1, ax1 = plt.subplots(figsize=(10, 6))
ax1.semilogx(N, sp_pred(N), "b-", lw=2, label="VIPER (fixed 1.40 ms transfer)")
ax1.axhline(ceiling, color="r", ls="--", lw=1.3, label=f"Amdahl ceiling = {ceiling:.2f}x")
ax1.axhline(1.0, color="k", ls=":", lw=1, label="break-even (1x)")
ax1.plot(alloc, sp_hw, "o-", color="tab:red", lw=1.6, ms=8, zorder=5,
         label="Measured on UPMEM (int8)")
ax1.annotate(f"peak {sp_hw[ipk]:.2f}x @ {int(alloc[ipk])} DPUs",
             xy=(alloc[ipk], sp_hw[ipk]), xytext=(330, 1.3),
             fontsize=13, arrowprops=dict(arrowstyle="->"))
ax1.set_xlabel("# DPUs"); ax1.set_ylabel("Speedup vs. CPU baseline")
ax1.set_title("LLaMA2 matmul offload to UPMEM — predicted vs measured", fontweight="bold")
ax1.grid(True, which="both", alpha=0.3); ax1.legend(loc="upper left")
fig1.tight_layout(); fig1.savefig("viper_speedup.png", dpi=150)

# Figure 2: time decomposition (measured)
fig2, ax2 = plt.subplots(figsize=(7.5, 5.2))
ax2.loglog(alloc, T_STUB+tmm, "o-", color="b", lw=2, ms=6, label="T_total (measured)")
ax2.loglog(alloc, tl, "s-", color="g", lw=1.4, ms=5, label="T_kernel (UPMEM compute)")
ax2.loglog(alloc, tin+tout, "^-", color="purple", lw=1.4, ms=5, label="T_transfer (measured)")
ax2.axhline(T_STUB, color="orange", ls="--", lw=1.4, label=f"T_CPU_rest = {T_STUB:.2f} ms")
ax2.axhline(T_FULL, color="r", ls=":", lw=1.4, label=f"T_CPU_full = {T_FULL:.2f} ms")
ax2.axhline(Tmem_pub, color="purple", ls=":", lw=1, alpha=0.6, label="assumed 1.40 ms")
ax2.set_xlabel("# DPUs"); ax2.set_ylabel("Time (ms)")
ax2.set_title("Time decomposition vs. DPU count (measured)")
ax2.grid(True, which="both", alpha=0.3); ax2.legend(loc="lower left", fontsize=9)
fig2.tight_layout(); fig2.savefig("viper_time_decomp.png", dpi=150)

# Figure 3: transfer term is not constant
fig3, ax3 = plt.subplots(figsize=(7.5, 5.2))
ax3.loglog(alloc, tl, "o-", color="tab:red", ms=6, lw=1.4, label="Measured DPU compute")
ax3.loglog(N, Tk1_pub/N, "-", color="tab:red", lw=1.0, alpha=0.5, label="VIPER $T_{kernel}(1)/N$")
ax3.loglog(alloc, tin+tout, "s-", color="tab:blue", ms=6, lw=1.4, label="Measured transfer (in+out)")
ax3.axhline(Tmem_pub, ls="--", color="tab:blue", lw=1.4, label="VIPER fixed transfer 1.40 ms")
ax3.set_xlabel("# DPUs"); ax3.set_ylabel("Time per forward (ms)")
ax3.set_title("Measured offload time: compute (1/N) vs transfer (grows with N)", fontsize=13)
ax3.grid(True, which="both", alpha=0.3); ax3.legend(loc="upper right", fontsize=10)
fig3.tight_layout(); fig3.savefig("viper_transfer.png", dpi=150)

# Figure 4: speedup with N-dependent (non-fixed) transfer in the prediction
sp_nf_curve = sp_pred_nf(N); jpk = int(np.argmax(sp_nf_curve))
fig4, ax4 = plt.subplots(figsize=(10, 6))
ax4.semilogx(N, sp_pred(N), "b--", lw=1.6, alpha=0.5, label="VIPER (fixed 1.40 ms transfer)")
ax4.semilogx(N, sp_nf_curve, "b-", lw=2.2,
             label=f"VIPER (N-dependent transfer: {_t0:.2f}+{_alpha:.4f}·N ms)")
ax4.axhline(ceiling, color="r", ls="--", lw=1.3, label=f"Amdahl ceiling = {ceiling:.2f}x")
ax4.axhline(1.0, color="k", ls=":", lw=1, label="break-even (1x)")
ax4.plot(alloc, sp_hw, "o-", color="tab:red", lw=1.6, ms=8, zorder=5,
         label="Measured on UPMEM (int8)")
ax4.set_xlabel("# DPUs"); ax4.set_ylabel("Speedup vs. CPU baseline")
ax4.set_title("LLaMA2 matmul offload to UPMEM — predicted vs measured", fontweight="bold")
ax4.grid(True, which="both", alpha=0.3); ax4.legend(loc="upper left", fontsize=11)
fig4.tight_layout(); fig4.savefig("viper_speedup_nonfixed.png", dpi=150)

print(f"peak {sp_hw[ipk]:.2f}x @ {int(alloc[ipk])} DPUs; 2560-DPU {sp_hw[-1]:.2f}x; ceiling {ceiling:.2f}x")
print(f"non-fixed transfer: T(N)={_t0:.3f}+{_alpha:.5f}*N; N*={Nstar:.0f}; pred peak {sp_nf_curve[jpk]:.2f}x @ {N[jpk]:.0f}")
print("wrote viper_speedup.png, viper_time_decomp.png, viper_transfer.png, viper_speedup_nonfixed.png")
