#!/bin/bash
cd /home/azzr/divbench
# build leaf variants
g++ -O2 -march=x86-64-v3 -std=c++23 -o v9_leaf11 /home/azzr/divbench/v9.cpp 2>/dev/null
VG="valgrind --tool=cachegrind --cache-sim=yes --branch-sim=yes --I1=32768,8,64 --D1=32768,8,64 --LL=33554432,16,64"
bins="v9_leaf10 v9_leaf11 hb_div"
inputs="max_00 large_00 a_max_b_random_02 bench_mid"
for bin in $bins; do
  [ -x "./$bin" ] || { echo "$bin :: MISSING BIN"; continue; }
  for nm in $inputs; do
    inf=/home/azzr/hexbench/data/div/$nm.in
    [ -f "$inf" ] || inf=/home/azzr/divbench/$nm.in
    [ -f "$inf" ] || { echo "$bin $nm :: NOINPUT"; continue; }
    $VG --cachegrind-out-file=cg_${bin}_${nm}.out ./$bin < $inf >/dev/null 2>cg_${bin}_${nm}.log || true
    Ir=$(grep "I refs" cg_${bin}_${nm}.log | tr -s ' ' | sed 's/.*I refs: //;s/ .*//')
    LL=$(grep "LLd misses" cg_${bin}_${nm}.log | tr -s ' ' | sed 's/.*LLd misses: //;s/ .*//')
    D1=$(grep "D1  misses" cg_${bin}_${nm}.log | tr -s ' ' | sed 's/.*D1  misses: //;s/ .*//')
    echo "$bin $nm :: Irefs=$Ir LLd=$LL D1=$D1"
  done
done
echo "ALL_CG_DONE"
