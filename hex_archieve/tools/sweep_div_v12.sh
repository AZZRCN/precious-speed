#!/bin/bash
# v12 mixed-radix port verification vs v11 baseline (= submit_div.cpp)
set -u
cd /home/azzr/divbench
DIVDATA=/home/azzr/hexbench/data/div

# build baseline v11 (current submit candidate) + v12 mixed
g++ -O2 -march=native -std=c++23 -o SD_base submit_div.cpp 2>/tmp/b.err || { echo "BUILD_SD_base FAIL"; cat /tmp/b.err; exit 1; }
g++ -O2 -march=native -std=c++23 -o V12 div_v12_mixed.cpp 2>/tmp/v12.err || { echo "BUILD_V12 FAIL"; cat /tmp/v12.err; exit 1; }
echo "BUILD OK"

gate() {
  local bin="$1"
  local ok=0 bad=0 bl=""
  for f in "$DIVDATA"/*.in; do
    e="${f%.in}.exp"
    if python3 hexcheck.py run "./$bin" "$f" "$e" >/dev/null 2>&1; then ok=$((ok+1)); else bad=$((bad+1)); bl="$bl $(basename "$f" .in)"; fi
  done
  echo "GATE $bin OK=$ok BAD=$bad$bl"
}

echo "=== GATE ==="
gate SD_base
gate V12

echo "=== TIMING (idle best-of-5) ==="
for c in length_ratio_integer_00 length_ratio_integer_02 a_max_b_random_01 a_max_b_random_02 max_00; do
  inf="$DIVDATA/$c.in"; [ -f "$inf" ] || continue
  for bin in SD_base V12; do
    best=999999
    for r in 1 2 3 4 5; do s=$(date +%s%N); "./$bin" < "$inf" >/dev/null; e=$(date +%s%N); d=$(( (e-s)/1000000 )); [ $d -lt $best ] && best=$d; done
    echo "TIME $bin $c best=${best}ms"
  done
done
echo ALL_DONE
