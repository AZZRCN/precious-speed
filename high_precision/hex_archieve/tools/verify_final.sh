#!/bin/bash
# 终验: 用实际 submit_ready 候选在 VM Linux g++ 编译, 跑全官方闸门 + 最坏用例 idle best-of-5
set -u
cd /home/azzr/divbench
MULDATA=/home/azzr/hexbench/data/mul
DIVDATA=/home/azzr/hexbench/data/div
g++ -O2 -march=native -std=c++23 submit_mul.cpp -o SM 2>/tmp/sm.err || { echo "BUILD SM FAIL"; cat /tmp/sm.err; exit 1; }
g++ -O2 -march=native -std=c++23 submit_div.cpp -o SD 2>/tmp/sd.err || { echo "BUILD SD FAIL"; cat /tmp/sd.err; exit 1; }
# MUL 22 闸门
ok=0; bad=0
for f in "$MULDATA"/*.in; do
  e="${f%.in}.exp"
  python3 mulcheck.py run ./SM "$f" "$e" >/dev/null 2>&1 && ok=$((ok+1)) || bad=$((bad+1))
done
echo "GATE MUL(SM) OK=$ok BAD=$bad"
for c in max_max_06 large_02 fft_killer_06; do
  inf="$MULDATA/$c.in"; [ -f "$inf" ] || continue
  best=999999
  for r in 1 2 3 4 5; do s=$(date +%s%N); ./SM < "$inf" >/dev/null; e=$(date +%s%N); d=$(( (e-s)/1000000 )); [ $d -lt $best ] && best=$d; done
  echo "TIME SM $c best=${best}ms"
done
# DIV 27 闸门
ok=0; bad=0
for f in "$DIVDATA"/*.in; do
  e="${f%.in}.exp"
  python3 hexcheck.py run ./SD "$f" "$e" >/dev/null 2>&1 && ok=$((ok+1)) || bad=$((bad+1))
done
echo "GATE DIV(SD) OK=$ok BAD=$bad"
for c in length_ratio_integer_00 length_ratio_integer_02 a_max_b_random_02 max_00; do
  inf="$DIVDATA/$c.in"; [ -f "$inf" ] || continue
  best=999999
  for r in 1 2 3 4 5; do s=$(date +%s%N); ./SD < "$inf" >/dev/null; e=$(date +%s%N); d=$(( (e-s)/1000000 )); [ $d -lt $best ] && best=$d; done
  echo "TIME SD $c best=${best}ms"
done
echo ALL_DONE
