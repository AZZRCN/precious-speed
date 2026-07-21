#!/bin/bash
# moptm_fusion.cpp O2 baseline benchmark (high-precision ms timing) + moptm.cpp control
cd /tmp/bench || exit 1

run_bench() {
  local exe=$1
  local input=$2
  echo "--- $exe ---"
  ./$exe < $input > /dev/null 2>&1
  ./$exe < $input > /dev/null 2>&1
  for i in 1 2 3 4 5; do
    start=$(date +%s.%N)
    ./$exe < $input > /dev/null 2>&1
    end=$(date +%s.%N)
    awk "BEGIN {printf \"%.2f\n\", ($end - $start) * 1000}"
  done
}

echo "=== benchmark fusion (2 warmup + 5 timed, ms) ==="
run_bench moptm_fusion_O2_ADD add_1M.txt
run_bench moptm_fusion_O2_MUL mul_500k.txt
run_bench moptm_fusion_O2_DIV div_1M_500k.txt

echo "=== benchmark control moptm_O2 (2 warmup + 5 timed, ms) ==="
run_bench moptm_O2_ADD add_1M.txt
run_bench moptm_O2_MUL mul_500k.txt
run_bench moptm_O2_DIV div_1M_500k.txt

echo "=== done ==="
