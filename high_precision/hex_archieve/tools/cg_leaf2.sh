#!/bin/bash
cd /home/azzr/hexbench
rm -f /tmp/cg_leaf2.log
for L in 5 7 9 11; do
  echo "#### L$L" >> /tmp/cg_leaf2.log
  taskset -c 5 valgrind --tool=cachegrind --cache-sim=yes --cachegrind-out-file=/dev/null \
    /tmp/v9L$L < data/mul/max_max_01.in > /dev/null 2>>/tmp/cg_leaf2.log
done
grep -E "^####|I  *refs:|D  *refs:|D1  *misses:|LLd *misses:|LL *refs:|I1 *misses:" /tmp/cg_leaf2.log
