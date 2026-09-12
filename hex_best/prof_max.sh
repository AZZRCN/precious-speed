#!/bin/bash
# 对 LC max 点 (length_ratio_integer#2, 429M instr) 做指令数热点剖析
cd ~
CASE=/tmp/lccases/length_ratio_integer_2.in
BIN=/tmp/dv_0_32
ls -la $CASE
head -1 $CASE
awk 'NR==2{print "A_len="length($1)"  B_len="length($2)}' $CASE
awk 'NR==3{print "case2: A_len="length($1)"  B_len="length($2)}' $CASE
echo "=== total instructions:u ==="
perf stat -e instructions:u -x, $BIN < $CASE 2>&1 >/dev/null | grep instr | cut -d, -f1
echo "=== hotspots by instructions:u (dense sampling) ==="
perf record -q -e instructions:u -c 20000 -o /tmp/mx.data $BIN < $CASE >/dev/null 2>&1
perf report -i /tmp/mx.data --stdio --no-children -q --percent-limit 0.4 2>/dev/null \
  | grep -E '^\s+[0-9]' | head -28
