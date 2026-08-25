#!/bin/bash
#$ -N viper_GraphBIG_pim_very_large
#$ -M hgeng@nd.edu
#$ -m abe
#$ -q long
#$ -pe smp 8

set -euo pipefail

# ---------------- Host paths ----------------
GEM5_DIR=/users/hgeng/Behemoth/Data_trigger/gem5

# ---------------- Container image ----------------
IMG=docker://ghcr.io/gem5/ubuntu-24.04_min-dependencies:v24-0

# ---------------- Container paths ----------------
GEM5_BIN=/mnt/gem5/build/X86/gem5.opt
CFG=/mnt/gem5/configs/se_config.py
GRAPH_DIR=/mnt/gem5/Graph

# Put outputs in gem5/outputs (absolute path so cd doesn't affect it)
OUTROOT=/mnt/gem5/outputs/GraphBIG_pim_very_large

run_one () {
  local name="$1"
  local outdir="${OUTROOT}/${name}"

  apptainer exec --bind "${GEM5_DIR}:/mnt/gem5" "${IMG}" \
    bash -lc "
      set -e

      # --- sanity: dataset exists where you expect ---
      test -f /mnt/gem5/dataset/small/vertex.csv

      # --- KEY FIX: make ../../dataset/... work even if gem5 runs from outdir ---
      mkdir -p /mnt/gem5/outputs
      ln -sfn /mnt/gem5/dataset /mnt/gem5/outputs/dataset

      mkdir -p '${outdir}'

      # run gem5 (no --cwd supported by your se_config.py)
      ${GEM5_BIN} --outdir='${outdir}' ${CFG} --cmd=${GRAPH_DIR}/${name}
    "

  # ---- collect PIM files ----
  if [ -f /tmp/PIM_MISS_RATE.txt ]; then
    mv /tmp/PIM_MISS_RATE.txt "${outdir}/PIM_MISS_RATE_${name}.txt"
  else
    echo "WARNING: /tmp/PIM_MISS_RATE.txt not found for ${name}" >&2
  fi

} 

# ---------------- Run order (as you want) ----------------
run_one bfs
run_one dfs
run_one connectedcomponent
run_one dc
run_one kcore
run_one pagerank
run_one sssp
run_one tc
run_one graphconstruct

echo "All GraphBIG gem5 SE runs completed."

