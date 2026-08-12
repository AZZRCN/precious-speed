#!/bin/bash
# DIV LEAF 扫描 9..13 (在 v11 hugify 基础上, -DFFT_LEAF_LOG 覆盖), 27 闸门 + 大用例 idle best-of-5
set -u
cd /home/azzr/divbench
SRC=div_v11_hugify.cpp
DATA=/home/azzr/hexbench/data/div
for L in 9 10 11 12 13; do
  B=DL$L
  g++ -O2 -march=native -std=c++23 -DFFT_LEAF_LOG=$L "$SRC" -o "$B" 2>/tmp/dcl_$L.err || { echo "BUILD DL$L FAIL"; cat /tmp/dcl_$L.err; continue; }
  ok=0; bad=0
  for f in "$DATA"/*.in; do
    e="${f%.in}.exp"
    python3 hexcheck.py run ./"$B" "$f" "$e" >/dev/null 2>&1 && ok=$((ok+1)) || bad=$((bad+1))
  done
  echo "GATE DL$L OK=$ok BAD=$bad"
  for c in length_ratio_integer_00 length_ratio_integer_02 a_max_b_random_01 a_max_b_random_02 medium_00 max_00; do
    inf="$DATA/$c.in"
    [ -f "$inf" ] || continue
    best=999999
    for r in 1 2 3 4 5; do
      s=$(date +%s%N); ./"$B" < "$inf" >/dev/null; e=$(date +%s%N)
      d=$(( (e-s)/1000000 ))
      [ $d -lt $best ] && best=$d
    done
    echo "TIME DL$L $c best=${best}ms"
  done
done
echo ALL_DONE
