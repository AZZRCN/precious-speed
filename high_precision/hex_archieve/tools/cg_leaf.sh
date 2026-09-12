#!/bin/bash
# cachegrind 扫描 FFT_LEAF_LOG 变体 (权威口径: Ir + D1/LL misses, 逐位确定性)
cd /home/azzr/hexbench || exit 1
CASE=${1:-data/mul/max_max_01.in}
LOG=/tmp/cg_leaf.log
: > $LOG
for L in 9 10 11 12; do
  echo "#### L$L" >> $LOG
  taskset -c 5 valgrind --tool=cachegrind --cache-sim=yes \
      --cachegrind-out-file=/dev/null /tmp/v9L$L < "$CASE" > /dev/null 2>> $LOG
done
grep -E '^####|I +refs:|D +refs:|D1 +misses:|LLd +misses:' $LOG
