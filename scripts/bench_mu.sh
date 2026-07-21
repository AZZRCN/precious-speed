#!/bin/bash
cd /tmp/bench

BIN1=${1:-moptm_fusion_O2_DIV}
BIN2=${2:-moptm_ref}
TESTS=${3:-"div_1M_500k div_200k_100k div_1M_100k"}

for t in $TESTS; do
  echo "=== $t ==="
  echo -n "  $BIN1: "
  for i in 1 2 3 4 5; do
    /usr/bin/time -f "%e" ./$BIN1 < $t.txt > /dev/null 2>tmp_time.txt
    cat tmp_time.txt | tr '\n' ' '
  done
  echo
  echo -n "  $BIN2: "
  for i in 1 2 3 4 5; do
    /usr/bin/time -f "%e" ./$BIN2 < $t.txt > /dev/null 2>tmp_time.txt
    cat tmp_time.txt | tr '\n' ' '
  done
  echo
done
rm -f tmp_time.txt
