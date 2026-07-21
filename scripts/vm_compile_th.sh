#!/bin/bash
# Compile 4 DIV threshold versions in background, log to compile_th.log
cd /tmp/bench
rm -f compile_th.log
{
  for th in 32 64 128 256; do
    echo "=== compiling th${th} ==="
    g++ -x c++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV -o moptm_th${th}_DIV moptm_th${th}.cpp 2>&1
    echo "=== done th${th} rc=$? ==="
  done
  echo "ALL_DONE"
} > compile_th.log 2>&1
