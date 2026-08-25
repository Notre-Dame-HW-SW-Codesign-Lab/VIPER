#!/bin/bash
#$ -N viper_SPEC_AES_v2
#$ -M hgeng@nd.edu
#$ -m abe
#$ -q long
#$ -pe smp 8

set -euo pipefail

GEM5_DIR=/users/hgeng/Behemoth/Data_trigger/gem5
IMG=docker://ghcr.io/gem5/ubuntu-24.04_min-dependencies:v24-0

GEM5_BIN=/mnt/gem5/build/X86/gem5.opt
CFG=/mnt/gem5/configs/se_config.py

AES_DIR=/mnt/gem5/SPECAES/spec_aes_13_sources

# Put AES outputs in a separate tree
OUTROOT=./outputs/SPECAES

run_one () {
  local name="$1"
  local args="$2"
  local outdir="${OUTROOT}/${name}"
  mkdir -p "${outdir}"

  apptainer exec --bind "${GEM5_DIR}:/mnt/gem5" "${IMG}" \
    bash -lc "${GEM5_BIN} --outdir=${outdir} ${CFG} --cmd=${AES_DIR}/${name} --args='${args}'"
}

# SAME parameters as your non-AES baseline (your earlier list)
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

echo "All AES runs completed. Outputs under ${OUTROOT}/<bench>/"

