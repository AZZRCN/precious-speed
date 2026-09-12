#!/bin/bash
# 函数级 callgrind: 取每组 top 函数 inclusive Ir (确定性, 无噪声)
BIN=$1; GRP=$2
valgrind --tool=callgrind --cache-sim=no --branch-sim=no \
  --callgrind-out-file=/tmp/cg.out "$BIN" < cases_hex/$GRP.in > /dev/null 2>/dev/null
echo "=== $GRP : top functions by inclusive Ir ==="
callgrind_annotate --show=Ir --threshold=100 /tmp/cg.out 2>/dev/null | \
  grep -E '^\s+[0-9,]+\s+.*:[a-zA-Z_]' | head -30
