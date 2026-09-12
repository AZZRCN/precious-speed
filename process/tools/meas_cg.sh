#!/bin/bash
# meas_cg.sh - callgrind 指令数测量 (用于无 PMU 的 VM, 如 285H/.66)
#
# 背景: .66 (Intel 285H) 的 VMware 未开启「虚拟化 CPU 性能计数器」(vPMC),
# perf 硬件事件全部不可用 (perf list hw 为空, sudo 亦然). callgrind 是纯软件
# 插桩, 不依赖 PMU, 且指令数是确定性的 (无噪声), 跨 CPU 一致.
#
# 校准: length_ratio_2 -> callgrind 470,620,043 vs perf(.55) 457.9M (+2.8%).
# 该偏差是系统性的 (valgrind 替换 malloc/startup), A/B 对比中抵消.
#
# 用法: bash meas_cg.sh [bin] [groups...]
set -u
cd /home/azzr/precious_speed

BIN="${1:-hex_best/base_zn.bin}"
shift || true
if [ $# -gt 0 ]; then
  GRPS="$*"
else
  GRPS="length_ratio_0 length_ratio_1 length_ratio_2 length_ratio_3 length_ratio_4 length_ratio_5 amax_0 amax_1 amax_2 max_0 max_1 max_2 large_0 large_1 medium_0 medium_1 medium_2 bzbound_0 bzbound_1 bzbound_2 bzbound_3 small_0 power_0"
fi

printf "%-16s %14s %8s\n" case instr_M cg_s
for g in $GRPS; do
  [ -f "cases_hex/$g.in" ] || { printf "%-16s %14s %8s\n" "$g" MISSING -; continue; }
  t0=$(date +%s%N)
  valgrind --tool=callgrind --cache-sim=no --branch-sim=no \
           --callgrind-out-file=/dev/null \
           "./$BIN" < "cases_hex/$g.in" > "/tmp/cgout_$g.txt" 2>"/tmp/cgerr_$g.txt"
  t1=$(date +%s%N)
  ir=$(grep -m1 'I   refs:' "/tmp/cgerr_$g.txt" | sed 's/.*I   refs: *//; s/,//g')
  [ -n "$ir" ] || ir=0
  printf "%-16s %14.2f %8.2f\n" "$g" \
    "$(echo "$ir" | awk '{print $1/1e6}')" \
    "$(echo "$t0 $t1" | awk '{printf "%.2f", ($2-$1)/1e9}')"
done
