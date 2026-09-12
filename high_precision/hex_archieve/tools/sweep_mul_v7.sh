#!/bin/bash
# HEX mul sweep v5/v6/v7: 22官方闸门 + Zen3 cachegrind (I refs / LLd misses) + 空闲墙钟交叉校验
cd /home/azzr/divbench
FLAGS="-O2 -march=x86-64-v3 -std=c++23"
VG="valgrind --tool=cachegrind --cache-sim=yes --branch-sim=yes --I1=32768,8,64 --D1=32768,8,64 --LL=33554432,16,64"
DATA=/home/azzr/hexbench/data/mul

echo "===== COMPILE ====="
for b in v5 v6 v7; do
  g++ $FLAGS -o $b ${b}.cpp 2>/tmp/e_${b}.err
  echo "$b RC=$? $(tail -1 /tmp/e_${b}.err)"
done

echo "===== GATE (22 官方, 含符号) ====="
for bin in v5 v6 v7; do
  ok=0; bad=0; bl=""
  for f in $DATA/*.in; do
    e=${f%.in}.exp
    if python3 mulcheck.py run ./$bin "$f" "$e" >/dev/null 2>&1; then ok=$((ok+1)); else bad=$((bad+1)); bl="$bl $(basename $f .in)"; fi
  done
  echo "GATE $bin: OK=$ok BAD=$bad$bl"
done

echo "===== CACHEGRIND (Zen3 几何: I refs / LLd misses) ====="
for bin in v5 v6 v7; do
  for c in max_max_06 max_max_00 large_00 medium_00; do
    inf=$DATA/$c.in
    [ -f "$inf" ] || { echo "IR $bin $c: NO_INPUT"; continue; }
    $VG --cachegrind-out-file=/tmp/cm_${bin}_${c}.out ./$bin < $inf >/dev/null 2>/tmp/cm_${bin}_${c}.log || true
    Ir=$(grep "I refs" /tmp/cm_${bin}_${c}.log | tr -s ' ')
    LL=$(grep "LLd misses" /tmp/cm_${bin}_${c}.log | tr -s ' ')
    DR=$(grep "D refs" /tmp/cm_${bin}_${c}.log | tr -s ' ')
    echo "IR $bin $c :: $Ir | $LL | $DR"
  done
done

echo "===== WALL (VM 空闲, max_max_06, 3 跑取最优) ====="
for bin in v5 v6 v7; do
  inf=$DATA/max_max_06.in
  best=999999
  for run in 1 2 3; do
    s=$(date +%s%N); ./$bin < $inf >/dev/null; e=$(date +%s%N); d=$(( (e-s)/1000000 ))
    [ $d -lt $best ] && best=$d
  done
  echo "WALL $bin max_max_06 best=${best}ms"
done
