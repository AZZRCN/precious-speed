#!/bin/bash
# R2c 验证: 半尺寸 cyclic 解封 (D14 unwrap 接管)
# oracle: R2c 输出 vs canonical(#1) 输出, 26 例字节比对
# perf: 全 26 点 instructions:u, 取 MAX 点 + delta
set -u
cd ~
CXX="${CXX:-g++-15}"
FLAGS=(-O3 -std=c++20 -march=znver3 -mtune=znver3 -w)

echo "=== build BASE (canonical #1) ==="
$CXX "${FLAGS[@]}" div_base16.cpp -o div_base16.bin 2>&1 | tail -3 || { echo "BASE BUILD FAIL"; exit 1; }
echo "=== build VAR (R2c half-size cyclic) ==="
$CXX "${FLAGS[@]}" div_base16_r2c.cpp -o div_base16_r2c.bin 2>&1 | tail -3 || { echo "VAR BUILD FAIL"; exit 1; }

CASEDIR=/tmp/lccases
cases=($(ls "$CASEDIR"/*.in | sort))

echo "=== oracle: R2c vs BASE, 26 cases byte-compare ==="
oracle_clean=1
for c in "${cases[@]}"; do
  ./div_base16.bin < "$c" > /tmp/o_base.out 2>/dev/null
  ./div_base16_r2c.bin < "$c" > /tmp/o_var.out 2>/dev/null
  if ! cmp -s /tmp/o_base.out /tmp/o_var.out; then
    oracle_clean=0
    echo "ORACLE_FAIL: $(basename "$c")"
    cmp /tmp/o_base.out /tmp/o_var.out | head -3
    break
  fi
done
[ "$oracle_clean" = "1" ] && echo "ORACLE_CLEAN"

echo "=== perf instructions:u per case (BASE vs VAR) ==="
printf "%-28s %14s %14s %10s\n" CASE BASE VAR DELTA%
max_base=0; max_base_case=""; max_var=0; max_var_case=""
for c in "${cases[@]}"; do
  b=$(perf stat -e instructions:u -r 1 --log-fd 1 ./div_base16.bin < "$c" 2>&1 | awk '/instructions:u/{gsub(/,/,"",$1); print $1; exit}')
  v=$(perf stat -e instructions:u -r 1 --log-fd 1 ./div_base16_r2c.bin < "$c" 2>&1 | awk '/instructions:u/{gsub(/,/,"",$1); print $1; exit}')
  [ -z "$b" ] && b=0; [ -z "$v" ] && v=0
  d=$(awk -v b="$b" -v v="$v" 'BEGIN{printf "%.3f", (b>0)?(v-b)/b*100:0}')
  printf "%-28s %14s %14s %9s%%\n" "$(basename "$c" .in)" "$b" "$v" "$d"
  if [ "$b" -gt "$max_base" ]; then max_base=$b; max_base_case="$(basename "$c" .in)"; fi
  if [ "$v" -gt "$max_var" ]; then max_var=$v; max_var_case="$(basename "$c" .in)"; fi
done
echo "---"
printf "MAX BASE: %s = %s\n" "$max_base_case" "$max_base"
printf "MAX VAR : %s = %s\n" "$max_var_case" "$max_var"
awk -v b="$max_base" -v v="$max_var" 'BEGIN{printf "MAX-point delta: %.3f%%\n", (v-b)/b*100}'
