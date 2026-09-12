#!/bin/bash
# HEX mul LEAF sweep on v7: LEAF=10/11/12 对比 (22 闸门 + Zen3 cachegrind I refs/LLd misses + 空闲墙钟)
cd /home/azzr/divbench
FLAGS="-O2 -march=x86-64-v3 -std=c++23"
VG="valgrind --tool=cachegrind --cache-sim=yes --branch-sim=yes --I1=32768,8,64 --D1=32768,8,64 --LL=33554432,16,64"
DATA=/home/azzr/hexbench/data/mul

echo "===== COMPILE ====="
for spec in "L11:v7.cpp" "L10:v7_leaf10.cpp" "L12:v7_leaf12.cpp"; do
  bin=${spec%%:*}; src=${spec##*:}
  g++ $FLAGS -o $bin $src 2>/tmp/e_$bin.err
  echo "$bin($src) RC=$? $(tail -1 /tmp/e_$bin.err)"
done

echo "===== GATE (22 官方) ====="
for bin in L11 L10 L12; do
  ok=0; bad=0; bl=""
  for f in $DATA/*.in; do
    e=${f%.in}.exp
    if python3 mulcheck.py run ./$bin "$f" "$e" >/dev/null 2>&1; then ok=$((ok+1)); else bad=$((bad+1)); bl="$bl $(basename $f .in)"; fi
  done
  echo "GATE $bin: OK=$ok BAD=$bad$bl"
done

echo "===== CACHEGRIND (Zen3: I refs / LLd misses) ====="
for bin in L11 L10 L12; do
  for c in max_max_06 max_max_00 large_00 medium_00; do
    inf=$DATA/$c.in
    [ -f "$inf" ] || { echo "IR $bin $c: NO_INPUT"; continue; }
    $VG --cachegrind-out-file=/tmp/cl_${bin}_${c}.out ./$bin < $inf >/dev/null 2>/tmp/cl_${bin}_${c}.log || true
    Ir=$(grep "I refs" /tmp/cl_${bin}_${c}.log | tr -s ' ')
    LL=$(grep "LLd misses" /tmp/cl_${bin}_${c}.log | tr -s ' ')
    echo "IR $bin $c :: $Ir | $LL"
  done
done

echo "===== WALL (VM 空闲, max_max_06, 3 跑取最优) ====="
for bin in L11 L10 L12; do
  inf=$DATA/max_max_06.in; best=999999
  for run in 1 2 3; do
    s=$(date +%s%N); ./$bin < $inf >/dev/null; e=$(date +%s%N); d=$(( (e-s)/1000000 )); [ $d -lt $best ] && best=$d
  done
  echo "WALL $bin max_max_06 best=${best}ms"
done
