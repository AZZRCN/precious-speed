#!/bin/bash
# DIV: base(现 #392043, LEAF=10) vs div_v11_hugify (巨页), 27 官方闸门 + 大计分用例 idle 墙钟 best-of-5
set -u
cd /home/azzr/divbench
DATA=/home/azzr/hexbench/data/div
g++ -O2 -march=native -std=c++23 base_div.cpp -o Bbase 2>/tmp/cb.err || { echo "BUILD Bbase FAIL"; cat /tmp/cb.err; exit 1; }
g++ -O2 -march=native -std=c++23 div_v11_hugify.cpp -o B11 2>/tmp/c11.err || { echo "BUILD B11 FAIL"; cat /tmp/c11.err; exit 1; }
for B in Bbase B11; do
  ok=0; bad=0
  for f in "$DATA"/*.in; do
    e="${f%.in}.exp"
    python3 hexcheck.py run ./"$B" "$f" "$e" >/dev/null 2>&1 && ok=$((ok+1)) || bad=$((bad+1))
  done
  echo "GATE $B OK=$ok BAD=$bad"
done
for c in length_ratio_integer_00 length_ratio_integer_02 a_max_b_random_01 a_max_b_random_02 r_nearly_zero_00 medium_00 burnikel_ziegler_bound_01 max_00; do
  inf="$DATA/$c.in"
  [ -f "$inf" ] || continue
  for B in Bbase B11; do
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
