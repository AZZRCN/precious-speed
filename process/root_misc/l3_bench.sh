#!/bin/bash
# 393027_opt_L3 双门禁 + L3 宏调试 (VM66, Intel Ultra 9 285H)
set -e
cd /tmp/l3bench
echo "=== build ==="
g++ -O3 -std=c++20 -march=znver3 -mtune=znver3 -w 393027_opt.cpp  -o opt   2>&1 | head
g++ -O3 -std=c++20 -march=znver3 -mtune=znver3 -w 393027_opt_L3.cpp -o optL3 2>&1 | head
g++ -O3 -std=c++20 -march=znver3 -mtune=znver3 -w 393027.cpp        -o orig  2>&1 | head

echo "=== mk cases ==="
python3 mkcases.py

echo "=== GATE1 correctness (L3 vs v27 byte-identical + python oracle) ==="
./opt   < cases26.txt > out_opt.txt
./optL3 < cases26.txt > out_L3.txt
./orig  < cases26.txt > out_orig.txt
if diff -q out_opt.txt out_L3.txt >/dev/null; then echo "L3 == v27 : OK"; else echo "L3 != v27 : FAIL"; fi
python3 l3_verify.py cases26.txt out_L3.txt
python3 l3_verify.py cases26.txt out_opt.txt

echo "=== GATE2 perf: callgrind --cache-sim=yes on heavy (MAX) ==="
valgrind --tool=callgrind --cache-sim=yes --callgrind-out-file=cg_opt.out  ./opt   heavy.txt >/dev/null 2>&1
valgrind --tool=callgrind --cache-sim=yes --callgrind-out-file=cg_L3.out  ./optL3 heavy.txt >/dev/null 2>&1
echo "--- v27 (baseline) ---"; python3 l3_cgparse.py cg_opt.out
echo "--- L3 variant   ---"; python3 l3_cgparse.py cg_L3.out

echo "=== L3 macro sweep (wall-clock 5r median + cache-sim miss) ==="
for MB in 8 16 24 32; do
  g++ -DL3_BYTES=$((MB*1024*1024)) -O3 -std=c++20 -march=znver3 -mtune=znver3 -w 393027_opt_L3.cpp -o optL3_$MB 2>&1 | head
  # wall-clock: 5 rounds, taskset -c 2, median
  w=()
  for r in 1 2 3 4 5; do
    s=$(date +%s%N); taskset -c 2 ./optL3_$MB heavy.txt >/dev/null; e=$(date +%s%N)
    w+=($(( (e - s) / 1000000 )))
  done
  IFS=$'\n' w_sorted=($(sort -n <<<"${w[*]}")); unset IFS
  med=${w_sorted[2]}
  valgrind --tool=callgrind --cache-sim=yes --callgrind-out-file=cg_${MB}.out ./optL3_$MB heavy.txt >/dev/null 2>&1
  echo "--- L3_BYTES=${MB}MB  wall_med=${med}ms ---"
  python3 l3_cgparse.py cg_${MB}.out
done
echo "DONE"
