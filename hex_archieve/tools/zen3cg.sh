#!/bin/bash
# Zen3 双视角 cachegrind (照 ZEN3_CACHE_SIM_ON_INTEL_VM.md §5)
cd /home/azzr/hexbench
BIN=${1:-/tmp/v9L11}
CASE=${2:-data/mul/max_max_01.in}
TAG=${3:-x}
echo "### $TAG  bin=$BIN"
echo "--- Pass1: L1 + Zen3 L2 (512K/8) ---"
taskset -c 5 valgrind --tool=cachegrind --cache-sim=yes \
  --I1=32768,8,64 --D1=32768,8,64 --LL=524288,8,64 \
  --cachegrind-out-file=/dev/null "$BIN" < "$CASE" > /dev/null 2>/tmp/z2_$TAG.txt
grep -E "I  *refs:|D  *refs:|D1  *misses:|LLd *misses:" /tmp/z2_$TAG.txt
echo "--- Pass2: L1 + Zen3 L3 (32M/16) ---"
taskset -c 5 valgrind --tool=cachegrind --cache-sim=yes \
  --I1=32768,8,64 --D1=32768,8,64 --LL=33554432,16,64 \
  --cachegrind-out-file=/dev/null "$BIN" < "$CASE" > /dev/null 2>/tmp/z3_$TAG.txt
grep -E "I  *refs:|D  *refs:|D1  *misses:|LLd *misses:" /tmp/z3_$TAG.txt
