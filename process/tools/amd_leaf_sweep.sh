#!/bin/bash
# amd_leaf_sweep.sh - 在 .55 上用 cachegrind + AMD EPYC 7B13 几何,
# 扫 FFT_LEAF_LOG 找 AMD 最优值 (解决 #393027 在 AMD 仍 39ms 的 cache 错配).
# 指令数 (I refs) 架构无关 = 真值; miss 数用 AMD 几何模拟.
set -u
cd /home/azzr/precious_speed
GRPS="length_ratio_2 amax_1 large_0 length_ratio_5"
printf "%-5s %-16s %14s %12s %12s %12s\n" LEAF case Irefs D1miss LLdmiss LLimiss
for LEAF in 6 7 8 9 10 11 12 13 14; do
  g++ -O3 -std=c++17 -march=znver3 -mtune=znver3 -DFFT_LEAF_LOG=$LEAF \
      -o /tmp/div_leaf.bin hex_best/div.cpp 2>/dev/null
  for g in $GRPS; do
    valgrind --tool=callgrind --cache-sim=yes --branch-sim=no \
      --I1=32768,8,64 --D1=32768,8,64 --LL=33554432,16,64 \
      --callgrind-out-file=/dev/null \
      /tmp/div_leaf.bin < "cases_hex/$g.in" >/dev/null 2>"/tmp/cg_${LEAF}_$g.txt"
    ir=$(grep -E 'I +refs:'       "/tmp/cg_${LEAF}_$g.txt" | sed -E 's/.*I +refs: *//; s/,//g')
    d1=$(grep -E 'D1 +misses:'   "/tmp/cg_${LEAF}_$g.txt" | sed -E 's/.*D1 +misses: *//; s/,//g')
    ll=$(grep -E 'LLd +misses:'  "/tmp/cg_${LEAF}_$g.txt" | sed -E 's/.*LLd +misses: *//; s/,//g')
    lli=$(grep -E 'LLi +misses:' "/tmp/cg_${LEAF}_$g.txt" | sed -E 's/.*LLi +misses: *//; s/,//g')
    printf "%-5s %-16s %14s %12s %12s %12s\n" "$LEAF" "$g" "$ir" "$d1" "$ll" "$lli"
  done
done
echo AMD_LEAF_SWEEP_DONE
