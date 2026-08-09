#!/bin/sh
IN=/home/azzr/lcp/big_integer/division_of_big_integers/in
CACHE="--cache-sim=yes --D1=32768,8,64 --I1=32768,8,64 --LL=33554432,16,64"
cd /home/azzr/divbench
for c in burnikel_ziegler_bound_00 r_nearly_zero_01 length_ratio_integer_00; do
  valgrind --tool=callgrind $CACHE --callgrind-out-file=/tmp/cgf_$c.out ./bin/d39 < $IN/$c.in > /dev/null 2>/dev/null
  echo "===== $c ====="
  callgrind_annotate --threshold=90 --auto=no /tmp/cgf_$c.out 2>/dev/null | sed -n "1,60p"
done
echo ALLDONE
