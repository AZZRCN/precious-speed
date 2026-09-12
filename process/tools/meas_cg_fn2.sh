#!/bin/bash
# 函数级 callgrind 占比 (.66): frame-pointer + noinline 让 callgrind 能 unwind 到 div.cpp 内函数.
# 用法: bash meas_cg_fn2.sh <case>
CASE="${1:?case}"
OUT=/tmp/cg_fn.out
if [ ! -x hex_best/profp_zn.bin ]; then
  echo "== build profiling binary =="
  g++ -O2 -std=c++17 -march=znver3 -mtune=znver3 -g -fno-inline -fno-inline-functions -fno-omit-frame-pointer -rdynamic \
      -o hex_best/profp_zn.bin hex_best/div.cpp 2>&1 | grep -i error | head -3
fi
echo "== callgrind $CASE (Ir per function, div.cpp only) =="
valgrind --tool=callgrind --cache-sim=no --branch-sim=no --callgrind-out-file=$OUT \
    ./hex_best/profp_zn.bin < cases_hex/$CASE.in > /dev/null 2>/dev/null
callgrind_annotate --show=Ir --include=div.cpp $OUT 2>/dev/null | grep -E '\.cpp' | head -45
echo "== total Ir (I refs) =="
grep -E '^I   refs:' $OUT | head -1
