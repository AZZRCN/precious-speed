#!/usr/bin/env bash
# cachegrind profile of DIV worst case under Zen3 (EPYC 7B13) geometry.
# Build with -march=x86-64-v3 (valgrind-safe; no AVX-512) so VEX won't SIGILL.
set -uo pipefail
cd /home/azzr/divbench
DIVDATA=/home/azzr/hexbench/data/div
g++ -O2 -march=x86-64-v3 -std=c++23 -o SDv3 submit_div.cpp 2>/tmp/b.err || { echo BUILD_FAIL; cat /tmp/b.err; exit 1; }
echo BUILD_OK
I1="32768,8,64"; D1="32768,8,64"
for tag in l2 l3; do
  if [ "$tag" = l2 ]; then LL="524288,8,64"; else LL="33554432,16,64"; fi
  echo "=== cachegrind Zen3 $tag : length_ratio_integer_00 ==="
  setarch "$(uname -m)" -R valgrind --tool=cachegrind --cache-sim=yes --branch-sim=yes \
    --I1=$I1 --D1=$D1 --LL=$LL --cachegrind-out-file=cg_$tag.out -- ./SDv3 \
    < "$DIVDATA/length_ratio_integer_00.in" >/dev/null 2>cg_$tag.err
  grep -E 'I refs|D1  misses|LLd misses|Mispredicts' cg_$tag.err | sed 's/^==[0-9]*== /  /'
  echo "--- top functions by Ir ($tag) ---"
  cg_annotate --auto=yes --show=Ir cg_$tag.out 2>/dev/null | grep -E 'submit_div.cpp:' | head -30
done
echo CG_DONE
