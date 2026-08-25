#!/bin/bash
#$ -N viper_DATA_TRIGGER_inorder_500ns_100GB
#$ -M hgeng@nd.edu
#$ -m abe
#$ -q long
#$ -pe smp 8

set -euo pipefail

GEM5_DIR=/users/hgeng/Behemoth/Data_trigger/gem5
IMG=docker://ghcr.io/gem5/ubuntu-24.04_min-dependencies:v24-0

GEM5_BIN=/mnt/gem5/build/X86/gem5.opt
CFG=/mnt/gem5/configs/se_config.py
SPEC_DIR=/mnt/gem5/SPEC
OUTROOT=./outputs/inorder_500ns_100GB

run_one () {
  local name="$1"
  local args="$2"
  local outdir="${OUTROOT}/${name}"

  mkdir -p "${outdir}"

  apptainer exec --bind "${GEM5_DIR}:/mnt/gem5" "${IMG}" \
    bash -lc "${GEM5_BIN} --outdir=${outdir} ${CFG} --cmd=${SPEC_DIR}/${name} --args=\"${args}\""

  # ---- collect PIM miss-rate file ----
  if [ -f /tmp/PIM_MISS_RATE.txt ]; then
    mv /tmp/PIM_MISS_RATE.txt "${outdir}/PIM_MISS_RATE_${name}.txt"
  else
    echo "WARNING: /tmp/PIM_MISS_RATE.txt not found for ${name}" >&2
  fi
}

run_one 433milc        "6 12 24 24 20"
run_one 462libquantum  "64 20000000"
run_one 473astar       "192 192 8"
run_one 605mcf_s       "80000 4 15"
run_one 619lbm_s       "96 96 64 20"
run_one 620onmetpp_s   "1024 30000 60000"
run_one 625x264_s      "240 180 6 1"
run_one 638imagick_s   "384 288 8"
run_one degreeCentr    "100000 6 8"
run_one graphColoring  "80000 6 4"
run_one kCore          "150000 6 8 15"
run_one pageRank       "120000 6 8"
run_one triangleCount  "80000 6 200000"

echo "All SPEC (no-AES) runs completed."

