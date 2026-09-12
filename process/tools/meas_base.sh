#!/bin/bash
# baseline: 各组指令数 + cycles(Intel 代理), 用于定位权重
set -u
cd /home/azzr/precious_speed
g++ -O3 -std=c++17 -march=znver3 -mtune=znver3 -o hex_best/base_zn.bin hex_best/div.cpp 2>&1 | tail -3
echo BUILD_DONE
GRPS="length_ratio_0 length_ratio_1 length_ratio_2 length_ratio_3 length_ratio_4 length_ratio_5 amax_0 amax_1 amax_2 max_0 max_1 max_2 large_0 large_1 medium_0 medium_1 medium_2 bzbound_0 bzbound_1 bzbound_2 bzbound_3 rnear_1 rnear_2 small_0 power_0"
printf "%-16s %12s %12s\n" case instr_M cyc_M
for g in $GRPS; do
  perf stat -e instructions,cycles -x, ./hex_best/base_zn.bin < cases_hex/$g.in >/dev/null 2>/tmp/p.txt
  i=$(grep -m1 instructions /tmp/p.txt | cut -d, -f1)
  c=$(grep -m1 cycles /tmp/p.txt | cut -d, -f1)
  printf "%-16s %12.1f %12.1f\n" "$g" $(awk -v x="$i" 'BEGIN{print x/1e6}') $(awk -v x="$c" 'BEGIN{print x/1e6}')
done
