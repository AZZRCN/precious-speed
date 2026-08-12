#!/bin/bash
# perf hardware-counter profile of worst DIV cases on v11 baseline (submit_div.cpp)
set -u
cd /home/azzr/divbench
DIVDATA=/home/azzr/hexbench/data/div
g++ -O2 -march=native -std=c++23 -o SD submit_div.cpp 2>/tmp/b.err || { echo BUILD_FAIL; cat /tmp/b.err; exit 1; }
echo "BUILD OK"

for c in length_ratio_integer_00 a_max_b_random_02; do
  inf="$DIVDATA/$c.in"
  echo "=== perf stat: $c ==="
  perf stat -e instructions:u,cycles:u,cache-misses:u,L1-dcache-misses:u,branch-misses:u \
    -r 3 -- ./SD < "$inf" >/dev/null 2>stat_$c.txt
  grep -E "instructions:|cycles:|cache-misses:|L1-dcache-misses:|branch-misses:" stat_$c.txt | sed 's/^/  /'
  echo "=== perf record top functions: $c ==="
  perf record -e instructions:u -F 9970 -o perf_$c.data -- ./SD < "$inf" >/dev/null 2>/dev/null
  perf report -i perf_$c.data --stdio --no-children -n --percent-limit=1.0 2>/dev/null | sed -n '1,40p'
  echo
done
echo PROFILE_DONE
