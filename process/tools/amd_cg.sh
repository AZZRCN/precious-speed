#!/bin/bash
# amd_cg.sh - 在任意 x86-64 机 (本机/Intel VM) 上用 cachegrind 模拟
# AMD EPYC 7B13 (Zen3 Milan) 的指令数 + 缓存 miss 数.
#
# 原理:
#  - 指令数 (I refs) 与 CPU 微架构无关, 同二进制下 Intel/AMD 完全一致 -> 真值.
#  - 缓存 miss 数用 AMD 几何模拟 (callgrind --cache-sim=yes + AMD L1/L3 参数):
#      I1 = 32KB, 8-way, 64B   (Zen3 L1I)
#      D1 = 32KB, 8-way, 64B   (Zen3 L1D)
#      LL = 32MB, 16-way, 64B  (Zen3 L3 per CCD; callgrind 末级统一模型, L2 并入)
#  - 编译用 -march=znver3 生成 AMD 二进制, 指令即 AMD 真值.
#
# 用法: GRPS="g1 g2 ..." bash amd_cg.sh
set -u
cd /home/azzr/precious_speed
GRPS="${GRPS:-length_ratio_2 length_ratio_5 amax_1 medium_0 large_0 power_0}"
: > /tmp/amd_cg_result.txt
printf "%-16s %14s %12s %12s %12s\n" case Irefs D1miss LLdmiss LLimiss | tee -a /tmp/amd_cg_result.txt
for g in $GRPS; do
  valgrind --tool=callgrind --cache-sim=yes --branch-sim=no \
    --I1=32768,8,64 --D1=32768,8,64 --LL=33554432,16,64 \
    --callgrind-out-file=/dev/null \
    ./hex_best/div_zn.bin < "cases_hex/$g.in" >/dev/null 2>"/tmp/cg_$g.txt"
  ir=$(grep 'I   refs:'  "/tmp/cg_$g.txt" | sed 's/.*I   refs: *//; s/,//g')
  d1=$(grep 'D1  miss:'  "/tmp/cg_$g.txt" | sed 's/.*D1  miss: *//; s/,//g')
  ll=$(grep 'LLd  miss:'  "/tmp/cg_$g.txt" | sed 's/.*LLd  miss: *//; s/,//g')
  lli=$(grep 'LLi  miss:' "/tmp/cg_$g.txt" | sed 's/.*LLi  miss: *//; s/,//g')
  printf "%-16s %14s %12s %12s %12s\n" "$g" "$ir" "$d1" "$ll" "$lli" | tee -a /tmp/amd_cg_result.txt
done
echo "AMD_CG_DONE"
