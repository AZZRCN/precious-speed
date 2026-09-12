#!/bin/bash
# loopbench.sh CASEFILE N binA binB ...
# 累计循环计时: 每个二进制连跑 N 次取总时长 / N, 交替 3 个 block, 报每个 block 的均值与全局 MIN。
# 目的: 摊薄单次 fork/exec 抖动, 用于 <15ms 的短程序 A/B 判优 (zbench wall 固定开销约 6ms, 分辨率不够)。
CPU=${ZB_CPU:-3}
C=$1; shift
N=$1; shift
BINS=("$@")

# warmup
for b in "${BINS[@]}"; do
  for i in $(seq 3); do taskset -c "$CPU" "$b" < "$C" >/dev/null 2>&1; done
done

declare -A best
for b in "${BINS[@]}"; do best["$b"]=999999999; done

for round in 1 2 3 4; do
  for b in "${BINS[@]}"; do
    s=$(date +%s%N)
    for i in $(seq "$N"); do taskset -c "$CPU" "$b" < "$C" >/dev/null 2>&1; done
    e=$(date +%s%N)
    us=$(( (e - s) / N / 1000 ))
    printf "  round%d %-16s %6d us\n" "$round" "$(basename "$b")" "$us"
    if [ "$us" -lt "${best[$b]}" ]; then best["$b"]=$us; fi
  done
done

echo "--- BEST (min of 4 blocks) ---"
base=""
for b in "${BINS[@]}"; do
  if [ -z "$base" ]; then base=${best[$b]}; fi
  rel=$(awk -v a="${best[$b]}" -v c="$base" 'BEGIN{printf "%.4f", a/c}')
  printf "  %-16s %6d us  rel=%s\n" "$(basename "$b")" "${best[$b]}" "$rel"
done
