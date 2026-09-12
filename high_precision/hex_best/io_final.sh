#!/bin/bash
# FINAL: 生产源 (fromCharRange 分支less + 原 hexTokenLen, FFT_R8=0) vs 基线
cd ~
FLAGS="-O3 -std=c++20 -march=znver3 -mtune=znver3 -w"
g++-15 $FLAGS -DFFT_R8=0 -o /tmp/dv_io_var2 div_io_var2.cpp 2>/tmp/io_v2.log || { echo V2_BUILD_FAIL; cat /tmp/io_v2.log; exit 1; }
echo "=== oracle check vs baseline ==="
fail=0
for f in /tmp/lccases/*.in; do o1=$(/tmp/dv_io_base < "$f"); o2=$(/tmp/dv_io_var2 < "$f"); if [ "$o1" != "$o2" ]; then echo "MISMATCH: $(basename $f)"; fail=1; fi; done
[ $fail -eq 0 ] && echo ORACLE_CLEAN
echo "=== BASE vs final (fromCharRange-branchless) ==="
python3 ~/lc_max.py /tmp/dv_io_base /tmp/dv_io_var2
