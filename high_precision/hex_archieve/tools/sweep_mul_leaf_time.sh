#!/bin/bash
# 干净 MUL LEAF 计时扫描 (不用 cachegrind, 避免 valgrind 3.26 事件名变更)
# 编译 391969 源在 LEAF=10/11/12, 22 官方闸门, 关键用例 idle 墙钟 best-of-5
set -u
cd /home/azzr/divbench
SRC=391969_mul.cpp
DATA=/home/azzr/hexbench/data/mul
BIN=""
for L in 10 11 12; do
  B=B17_L$L
  g++ -O2 -march=native -std=c++23 -DFFT_LEAF_LOG=$L "$SRC" -o "$B" 2>/tmp/cc_$L.err || { echo "BUILD $B FAIL"; cat /tmp/cc_$L.err; continue; }
  # 闸门
  ok=0; bad=0
  for f in "$DATA"/*.in; do
    e="${f%.in}.exp"
    python3 mulcheck.py run ./"$B" "$f" "$e" >/dev/null 2>&1 && ok=$((ok+1)) || bad=$((bad+1))
  done
  echo "GATE $B OK=$ok BAD=$bad"
  # 计时: 关键计分用例 best-of-5
  for c in max_max_06 large_02 fft_killer_06; do
    inf="$DATA/$c.in"
    [ -f "$inf" ] || continue
    best=999999
    for r in 1 2 3 4 5; do
      s=$(date +%s%N); ./"$B" < "$inf" >/dev/null; e=$(date +%s%N)
      d=$(( (e-s)/1000000 ))
      [ $d -lt $best ] && best=$d
    done
    echo "TIME $B $c best=${best}ms"
  done
done
echo ALL_DONE
