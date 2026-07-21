#!/bin/bash
# Task 2: Benchmark DIV modes with different absInvNewton thresholds
cd /tmp/bench

echo "=========================================="
echo "Task 2: absInvNewton threshold experiment"
echo "=========================================="

for th in 32 64 128 256; do
  echo ""
  echo "########## threshold $th ##########"
  BIN=./moptm_th${th}_DIV

  # MD5 on 1M/500k
  echo "--- th=$th DIV 1M/500k MD5 ---"
  md5sum <($BIN < div_1M_500k.txt) | awk '{print $1}'

  # 1M/500k timing (2 warmup + 5 runs)
  echo "--- th=$th DIV 1M/500k timing (5 runs, 2 warmup) ---"
  $BIN < div_1M_500k.txt > /dev/null
  $BIN < div_1M_500k.txt > /dev/null
  T1_TIMES=""
  for i in 1 2 3 4 5; do
    start=$(date +%s.%N)
    $BIN < div_1M_500k.txt > /dev/null
    end=$(date +%s.%N)
    t=$(echo "scale=3; $end - $start" | bc)
    echo "  run $i: $t s"
    T1_TIMES="$T1_TIMES $t"
  done
  T1_MED=$(echo "$T1_TIMES" | tr ' ' '\n' | grep -v '^$' | sort -n | sed -n '3p')
  echo "th=$th DIV 1M/500k median: $T1_MED s"

  # 200k/100k timing (2 warmup + 5 runs)
  echo "--- th=$th DIV 200k/100k timing (5 runs, 2 warmup) ---"
  $BIN < div_200k_100k.txt > /dev/null
  $BIN < div_200k_100k.txt > /dev/null
  T2_TIMES=""
  for i in 1 2 3 4 5; do
    start=$(date +%s.%N)
    $BIN < div_200k_100k.txt > /dev/null
    end=$(date +%s.%N)
    t=$(echo "scale=3; $end - $start" | bc)
    echo "  run $i: $t s"
    T2_TIMES="$T2_TIMES $t"
  done
  T2_MED=$(echo "$T2_TIMES" | tr ' ' '\n' | grep -v '^$' | sort -n | sed -n '3p')
  echo "th=$th DIV 200k/100k median: $T2_MED s"
done

echo ""
echo "=== TASK2_DONE ==="
