#!/bin/bash
# I/O 解析分支less 实测: 基线(原 parser) vs 变体(分支less) — instructions:u @ LC 全量 26 点
cd ~
FLAGS="-O3 -std=c++20 -march=znver3 -mtune=znver3 -w"
echo "=== build baseline (original parser, FFT_R8=0) ==="
g++-15 $FLAGS -DFFT_R8=0 -o /tmp/dv_io_base div_io_base.cpp 2>/tmp/io_base.log || { echo "BASE_BUILD_FAIL"; cat /tmp/io_base.log; exit 1; }
echo "=== build variant (branchless parser, FFT_R8=0) ==="
g++-15 $FLAGS -DFFT_R8=0 -o /tmp/dv_io_var div_io_var.cpp 2>/tmp/io_var.log || { echo "VAR_BUILD_FAIL"; cat /tmp/io_var.log; exit 1; }
echo "=== built OK ==="
python3 ~/lc_max.py /tmp/dv_io_base /tmp/dv_io_var
