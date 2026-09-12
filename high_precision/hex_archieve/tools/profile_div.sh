#!/bin/bash
# cachegrind profile of worst DIV case on v11 baseline (SD_base)
set -u
cd /home/azzr/divbench
DIVDATA=/home/azzr/hexbench/data/div
g++ -O2 -march=native -std=c++23 -o SD_base submit_div.cpp 2>/tmp/b.err || { echo "BUILD FAIL"; cat /tmp/b.err; exit 1; }
inf="$DIVDATA/length_ratio_integer_00.in"
echo "=== cachegrind on $inf ==="
valgrind --tool=cachegrind --cache-sim=yes --cachegrind-out-file=cg_div.out ./SD_base < "$inf" >/dev/null 2>cg_div.err
echo "--- summary (I refs line) ---"
grep -E "I refs:|I1 misses:|LLi misses:|D refs:|D1 misses:|LLd misses:" cg_div.err
echo "--- top functions by Ir ---"
cg_annotate --auto=yes --show=Ir cg_div.out 2>/dev/null | sed -n '1,40p'
echo PROFILE_DONE
