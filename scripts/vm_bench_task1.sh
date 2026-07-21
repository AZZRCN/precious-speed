#!/bin/bash
# Task 1: Benchmark ADD/MUL modes of moptm_fusion
cd /tmp/bench

median5() {
  # read 5 numbers, output median (3rd sorted)
  sort -n | sed -n '3p'
}

echo "=========================================="
echo "Task 1: ADD/MUL regression check"
echo "=========================================="

# ADD MD5
echo "--- ADD MD5 ---"
md5sum <(./moptm_fusion_O2_ADD < add_1M.txt) | awk '{print $1}'

echo "--- ADD timing (5 runs, 2 warmup) ---"
./moptm_fusion_O2_ADD < add_1M.txt > /dev/null
./moptm_fusion_O2_ADD < add_1M.txt > /dev/null
ADD_TIMES=""
for i in 1 2 3 4 5; do
  start=$(date +%s.%N)
  ./moptm_fusion_O2_ADD < add_1M.txt > /dev/null
  end=$(date +%s.%N)
  t=$(echo "scale=3; $end - $start" | bc)
  echo "  run $i: $t s"
  ADD_TIMES="$ADD_TIMES $t"
done
ADD_MED=$(echo "$ADD_TIMES" | tr ' ' '\n' | grep -v '^$' | sort -n | sed -n '3p')
echo "ADD median: $ADD_MED s"

echo ""

# MUL MD5
echo "--- MUL MD5 ---"
md5sum <(./moptm_fusion_O2_MUL < mul_500k.txt) | awk '{print $1}'

echo "--- MUL timing (5 runs, 2 warmup) ---"
./moptm_fusion_O2_MUL < mul_500k.txt > /dev/null
./moptm_fusion_O2_MUL < mul_500k.txt > /dev/null
MUL_TIMES=""
for i in 1 2 3 4 5; do
  start=$(date +%s.%N)
  ./moptm_fusion_O2_MUL < mul_500k.txt > /dev/null
  end=$(date +%s.%N)
  t=$(echo "scale=3; $end - $start" | bc)
  echo "  run $i: $t s"
  MUL_TIMES="$MUL_TIMES $t"
done
MUL_MED=$(echo "$MUL_TIMES" | tr ' ' '\n' | grep -v '^$' | sort -n | sed -n '3p')
echo "MUL median: $MUL_MED s"

echo ""
echo "=== TASK1_DONE ADD_MED=$ADD_MED MUL_MED=$MUL_MED ==="
