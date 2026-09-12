#!/bin/bash
# T111: radix-8 x FFT_C4_MIN 矩阵 (perf instructions:u, LC 规模单例)
cd ~
CASE=${CASE:-/tmp/one_case.txt}
FLAGS="-O3 -std=c++20 -march=znver3 -mtune=znver3 -w"
for R in 0 1; do
  for L in 16 32 64; do
    OUT=/tmp/dv_${R}_${L}
    g++-15 $FLAGS -DFFT_R8=$R -DFFT_C4_MIN=$L -o $OUT div_r8.cpp 2>/tmp/e_${R}_${L}.log
    if [ $? -ne 0 ]; then echo "R8=$R C4MIN=$L BUILD_FAIL"; head -5 /tmp/e_${R}_${L}.log; fi
  done
done
echo "=== instructions:u ($CASE) ==="
for R in 0 1; do
  for L in 16 32 64; do
    OUT=/tmp/dv_${R}_${L}
    [ -x $OUT ] || continue
    I=$(perf stat -e instructions:u -x, $OUT < $CASE 2>&1 >/dev/null | grep instructions | cut -d, -f1)
    printf 'R8=%s C4MIN=%-3s %s\n' $R $L $I
  done
done
