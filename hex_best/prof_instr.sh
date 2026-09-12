#!/bin/bash
# 用 instructions:u 采样看各符号的指令数占比 (不是 cycles)
cd ~
CASE=${CASE:-/tmp/one_case.txt}
for TAG in 0 1; do
  BIN=/tmp/dv_${TAG}_32
  echo "===== FFT_R8=$TAG (C4MIN=32) ====="
  perf record -q -e instructions:u -c 200000 -o /tmp/pi_$TAG.data $BIN < $CASE >/dev/null 2>&1
  perf report -i /tmp/pi_$TAG.data --stdio --no-children -q --percent-limit 0.8 2>/dev/null \
    | grep -E '^\s+[0-9]' | head -22
done
