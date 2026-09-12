#!/bin/bash
# 测 CYCB=A vs CYCB=B 在给定用例上的 callgrind Ir (指令数真值)
A=$1; B=$2; F=$3
export OMP_NUM_THREADS=1
run_cg(){ CYCB=$1 valgrind --tool=callgrind --callgrind-out-file=/tmp/cg_$1.txt --quiet /tmp/hexcmp/bin/ref_zhbase < "$2" > /dev/null 2>&1; awk '/^totals:/{print $2; exit}' /tmp/cg_$1.txt; }
IA=$(run_cg "$A" "$F"); IB=$(run_cg "$B" "$F")
echo "CYCB=$A Ir=$IA"
echo "CYCB=$B Ir=$IB"
if [ -n "$IA" ] && [ -n "$IB" ] && [ "$IA" != "0" ]; then
  echo "delta% = $(echo "scale=4; ($IB-$IA)*100/$IA" | bc)%"
fi
