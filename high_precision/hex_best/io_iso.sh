#!/bin/bash
# I/O 解析隔离实测: BASE vs fromCharRange-only / BASE vs hexTokenLen-only
cd ~
FLAGS="-O3 -std=c++20 -march=znver3 -mtune=znver3 -w"
g++-15 $FLAGS -DFFT_R8=0 -o /tmp/dv_io_fc div_io_fc.cpp 2>/tmp/io_fc.log || { echo FC_BUILD_FAIL; cat /tmp/io_fc.log; exit 1; }
g++-15 $FLAGS -DFFT_R8=0 -o /tmp/dv_io_ht div_io_ht.cpp 2>/tmp/io_ht.log || { echo HT_BUILD_FAIL; cat /tmp/io_ht.log; exit 1; }
echo "=== BASE vs fromCharRange-only ==="
python3 ~/lc_max.py /tmp/dv_io_base /tmp/dv_io_fc
echo "=== BASE vs hexTokenLen-only ==="
python3 ~/lc_max.py /tmp/dv_io_base /tmp/dv_io_ht
