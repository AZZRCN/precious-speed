#!/bin/bash
# 函数级 + 行级 callgrind 剖分: 只打最高点
set -u
cd /home/azzr/divbench
IN=/home/azzr/lcp/big_integer/division_of_big_integers/in
CASE=${1:-length_ratio_integer_02}
CACHE="--cache-sim=yes --D1=32768,8,64 --I1=32768,8,64 --LL=33554432,16,64"

echo "=== BUILD d41g (-g) ==="
g++ -O2 -g -std=c++23 -march=x86-64-v3 -o bin/d41g src/div_D41.cpp 2>/tmp/eg.txt \
  && echo OK || { echo FAIL; head -20 /tmp/eg.txt; exit 1; }

echo "=== CALLGRIND on $CASE ==="
rm -f cg.func.out
valgrind --tool=callgrind $CACHE --branch-sim=no \
  --callgrind-out-file=cg.func.out \
  ./bin/d41g < $IN/$CASE.in > /dev/null 2>/tmp/cgf.err
grep -E "refs|misses" /tmp/cgf.err | head -6

echo "=== TOP FUNCTIONS (self Ir) ==="
callgrind_annotate --threshold=96 cg.func.out 2>/dev/null \
  | sed -n '/Ir *file:function/,/^$/p' | head -40
echo "=== ALLDONE ==="
