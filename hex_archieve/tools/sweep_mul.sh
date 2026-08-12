#!/bin/bash
# HEX mul sweep: 正确性闸门(22官方, 已含符号用例) + Zen3 cachegrind I refs / LLd misses
cd /home/azzr/divbench
FLAGS="-O2 -march=x86-64-v3 -std=c++23"
VG="valgrind --tool=cachegrind --cache-sim=yes --branch-sim=yes --I1=32768,8,64 --D1=32768,8,64 --LL=33554432,16,64"
DATA=/home/azzr/hexbench/data/mul

g++ $FLAGS -o v5 mul_v5.cpp 2>/tmp/e_v5.err; echo "v5 RC=$? $(tail -1 /tmp/e_v5.err)"
g++ $FLAGS -o v6 mul_v6.cpp 2>/tmp/e_v6.err; echo "v6 RC=$? $(tail -1 /tmp/e_v6.err)"

for bin in v5 v6; do
  ok=0; bad=0; bl=""
  for f in $DATA/*.in; do
    e=${f%.in}.exp
    if python3 mulcheck.py run ./$bin "$f" "$e" >/dev/null 2>&1; then ok=$((ok+1)); else bad=$((bad+1)); bl="$bl $(basename $f .in)"; fi
  done
  echo "GATE $bin: OK=$ok BAD=$bad$bl"
done

for bin in v5 v6; do
  for c in max_max_06 max_max_00 large_00 medium_00; do
    inf=$DATA/$c.in
    $VG --cachegrind-out-file=/tmp/cm_${bin}_${c}.out ./$bin < $inf >/dev/null 2>/tmp/cm_${bin}_${c}.log || true
    Ir=$(grep "I refs" /tmp/cm_${bin}_${c}.log | tr -s ' ')
    LL=$(grep "LLd misses" /tmp/cm_${bin}_${c}.log | tr -s ' ')
    echo "IR $bin $c :: $Ir | $LL"
  done
done
