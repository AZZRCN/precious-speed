#!/bin/bash
# HEX div sweep: 正确性闸门(27官方) + Zen3 cachegrind I refs (max/large/a_max_b)
cd /home/azzr/divbench
FLAGS="-O2 -march=x86-64-v3 -std=c++23"
VG="valgrind --tool=cachegrind --cache-sim=yes --branch-sim=yes --I1=32768,8,64 --D1=32768,8,64 --LL=33554432,16,64"
DATA=/home/azzr/hexbench/data/div

g++ $FLAGS -o S10 v9_scalar.cpp 2>/tmp/e_S10.err; echo "S10 RC=$? $(tail -1 /tmp/e_S10.err)"
sed 's/#define FFT_LEAF_LOG 10/#define FFT_LEAF_LOG 9/' v10.cpp > V9.cpp
g++ $FLAGS -o V9 V9.cpp 2>/tmp/e_V9.err; echo "V9 RC=$? $(tail -1 /tmp/e_V9.err)"
g++ $FLAGS -o V10 v10.cpp 2>/tmp/e_V10.err; echo "V10 RC=$? $(tail -1 /tmp/e_V10.err)"
sed 's/#define FFT_LEAF_LOG 10/#define FFT_LEAF_LOG 11/' v10.cpp > V11.cpp
g++ $FLAGS -o V11 V11.cpp 2>/tmp/e_V11.err; echo "V11 RC=$? $(tail -1 /tmp/e_V11.err)"
g++ $FLAGS -o hb_div /home/azzr/hexbench/div/div.cpp 2>/tmp/e_hb.err; echo "hb RC=$? $(tail -1 /tmp/e_hb.err)"

for bin in S10 V9 V10 V11 hb_div; do
  ok=0; bad=0; badlist=""
  for f in $DATA/*.in; do
    e=${f%.in}.exp
    if python3 hexcheck.py run ./$bin "$f" "$e" >/dev/null 2>&1; then ok=$((ok+1)); else bad=$((bad+1)); badlist="$badlist $(basename $f .in)"; fi
  done
  echo "GATE $bin: OK=$ok BAD=$bad$badlist"
done

for bin in S10 V9 V10 V11 hb_div; do
  for c in max_00 large_00 a_max_b_random_02; do
    inf=$DATA/$c.in
    $VG --cachegrind-out-file=/tmp/cg_${bin}_${c}.out ./$bin < $inf >/dev/null 2>/tmp/cg_${bin}_${c}.log || true
    Ir=$(grep "I refs" /tmp/cg_${bin}_${c}.log | tr -s ' ')
    LL=$(grep "LLd misses" /tmp/cg_${bin}_${c}.log | tr -s ' ')
    echo "IR $bin $c :: $Ir | $LL"
  done
done
