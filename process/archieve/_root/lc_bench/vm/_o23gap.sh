#!/bin/bash
# O2 vs O3 纯编译器差距 (无 pragma, 只靠命令行 flag) —— 量化 GCC G 预展开线的天花板
# 只打最高点族: length_ratio_integer_02 / _03
set -u
cd /home/azzr/divbench
IN=/home/azzr/lcp/big_integer/division_of_big_integers/in
CACHE="--cache-sim=yes --D1=32768,8,64 --I1=32768,8,64 --LL=33554432,16,64"
SRC=src/div_D41.cpp

echo "=== pragma audit ==="
grep -n 'pragma GCC optimize' $SRC || echo "no-optimize-pragma OK"
grep -n 'pragma GCC target' $SRC | head -3

for OPT in O2 O3; do
  echo "=== BUILD $OPT ==="
  /usr/bin/time -f "%e s" g++ -$OPT -std=c++23 -march=x86-64-v3 -o bin/d41_$OPT $SRC 2>&1 | tail -2
done

for CASE in length_ratio_integer_02 length_ratio_integer_03; do
  for OPT in O2 O3; do
    rm -f cg.$OPT.$CASE.out
    valgrind --tool=callgrind $CACHE --branch-sim=no \
      --callgrind-out-file=cg.$OPT.$CASE.out \
      ./bin/d41_$OPT < $IN/$CASE.in > /dev/null 2>/tmp/cg.$OPT.err
    IR=$(grep -oP 'refs:\s+\K[0-9,]+' /tmp/cg.$OPT.err | head -1)
    D1MR=$(grep -oP 'D1  misses:\s+\K[0-9,]+' /tmp/cg.$OPT.err | head -1)
    DLMR=$(grep -oP 'LLd? misses:\s+\K[0-9,]+' /tmp/cg.$OPT.err | head -1)
    echo "RESULT $CASE $OPT Ir=$IR D1mr_line=$D1MR LLmr_line=$DLMR"
    grep -E 'I *refs|D *refs|D1 *misses|LLd? *misses|LL *misses' /tmp/cg.$OPT.err | sed "s/^/  [$OPT $CASE] /"
  done
done
echo "=== ALLDONE ==="
