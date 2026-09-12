#!/bin/bash
# Round 1 验证: difC4/iditC4 顶层逆流软件预取 (-DHEX_CACHE_PF)
# 1) oracle 26 例字节比对  2) 全 26 点 instructions:u (BASE vs VAR) 找 MAX 点 delta  3) MAX 点 cache-misses 对比
set -e
cd ~
CXX="g++-15"
FLAGS="-O3 -std=c++20 -march=znver3 -mtune=znver3 -w"

$CXX $FLAGS -o div_base.bin div_canon_now.cpp 2>/tmp/bld_base.log || { echo "BASE_BUILD_FAIL"; cat /tmp/bld_base.log; exit 1; }
$CXX $FLAGS -DHEX_CACHE_PF -o div_cache1.bin div_base16_cache1.cpp 2>/tmp/bld_var.log || { echo "VAR_BUILD_FAIL"; cat /tmp/bld_var.log; exit 1; }

# --- Oracle ---
OC=0
for f in /tmp/lccases/*.in; do
  ./div_base.bin < "$f" > /tmp/o_base.out
  ./div_cache1.bin < "$f" > /tmp/o_var.out
  if ! cmp -s /tmp/o_base.out /tmp/o_var.out; then echo "ORACLE_MISMATCH: $(basename $f)"; OC=1; fi
done
[ $OC -eq 0 ] && echo "ORACLE_CLEAN" || echo "ORACLE_FAIL"

# --- instructions:u per point, 找 MAX ---
printf "%-30s %15s %15s %9s\n" POINT BASE VAR DELTA%
maxname=""; maxbase=0; maxvar=0
declare -a NAMES BASES VARS
i=0
for f in /tmp/lccases/*.in; do
  name=$(basename "$f" .in)
  b=$(perf stat -e instructions:u -r 1 --log-fd 1 ./div_base.bin < "$f" 2>&1 | awk '/instructions:u/{gsub(/,/,"",$1);print $1}')
  v=$(perf stat -e instructions:u -r 1 --log-fd 1 ./div_cache1.bin < "$f" 2>&1 | awk '/instructions:u/{gsub(/,/,"",$1);print $1}')
  [ -z "$b" ] && b=0; [ -z "$v" ] && v=0
  d=$(awk -v b="$b" -v v="$v" 'BEGIN{printf "%.3f", (b>0? (v-b)/b*100 : 0)}')
  printf "%-30s %15s %15s %8s%%\n" "$name" "$b" "$v" "$d"
  NAMES[$i]=$name; BASES[$i]=$b; VARS[$i]=$v; i=$((i+1))
  if [ "${b:-0}" -gt "$maxbase" ]; then maxbase=$b; maxvar=$v; maxname=$name; fi
done
echo "=== MAX point: $maxname  BASE=$maxbase  VAR=$maxvar  delta=$(awk -v b=$maxbase -v v=$maxvar 'BEGIN{printf "%.3f",(v-b)/b*100}')% ==="

# --- MAX 点 cache-misses 对比 (RAM 末级 miss + L1 miss) ---
echo "=== MAX-point cache-misses (BASE vs VAR) ==="
bm=$(perf stat -e instructions:u,L1-dcache-loads,L1-dcache-load-misses,cache-misses,cache-references -r 1 --log-fd 1 ./div_base.bin < /tmp/lccases/${maxname}.in 2>&1)
vm=$(perf stat -e instructions:u,L1-dcache-loads,L1-dcache-load-misses,cache-misses,cache-references -r 1 --log-fd 1 ./div_cache1.bin < /tmp/lccases/${maxname}.in 2>&1)
echo "$bm" | awk -v tag="BASE" '/instructions:u|L1-dcache-loads|L1-dcache-load-misses|cache-misses|cache-references/{gsub(/,/,"",$1);printf "%-8s %-28s %s\n",tag,$2,$1}'
echo "$vm" | awk -v tag="VAR " '/instructions:u|L1-dcache-loads|L1-dcache-load-misses|cache-misses|cache-references/{gsub(/,/,"",$1);printf "%-8s %-28s %s\n",tag,$2,$1}'
