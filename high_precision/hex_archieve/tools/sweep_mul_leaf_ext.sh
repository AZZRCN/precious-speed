#!/bin/bash
# 扩 MUL LEAF 扫描 9..13 (idle 墙钟 best-of-5, 22 官方闸门)
set -u
cd /home/azzr/divbench
SRC=ref391969.cpp
DATA=/home/azzr/hexbench/data/mul
for L in 9 10 11 12 13; do
  B=BL$L
  g++ -O2 -march=native -std=c++23 -DFFT_LEAF_LOG=$L "$SRC" -o "$B" 2>/tmp/cc_$L.err || { echo "BUILD $B FAIL"; cat /tmp/cc_$L.err; continue; }
  ok=0; bad=0
  for f in "$DATA"/*.in; do
    e="${f%.in}.exp"
    python3 mulcheck.py run ./"$B" "$f" "$e" >/dev/null 2>&1 && ok=$((ok+1)) || bad=$((bad+1))
  done
  echo "GATE BL$L OK=$ok BAD=$bad"
  for c in max_max_06 large_02 fft_killer_06 max_max_05; do
    inf="$DATA/$c.in"
    [ -f "$inf" ] || continue
    best=999999
    for r in 1 2 3 4 5; do
      s=$(date +%s%N); ./"$B" < "$inf" >/dev/null; e=$(date +%s%N)
      d=$(( (e-s)/1000000 ))
      [ $d -lt $best ] && best=$d
    done
    echo "TIME BL$L $c best=${best}ms"
  done
done
echo ALL_DONE
