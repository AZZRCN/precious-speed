#!/bin/bash
# 在 VM 上编译 div_mudiv.cpp 并测各 HEX_FORCE 模式指令数 (na>nb 路由对比)
set -u
cd /home/azzr/precious_speed
g++ -O3 -std=c++17 -march=znver3 -mtune=znver3 -o hex_best/div_mudiv_zn.bin hex_best/div_mudiv.cpp 2>&1 | tail -3
echo "BUILD_DONE"

# 测试组: na>nb 瓶颈组 + na≈nb 边界组
GRPS="length_ratio_2 length_ratio_3 length_ratio_5 length_ratio_10 length_ratio_20 length_ratio_50 amax_0 amax_1 amax_2 max_0 max_1 max_2 large_0 large_1 medium_0 medium_1 medium_2"

for mode in 0 1 2 3; do
  echo "===== HEX_FORCE=$mode ====="
  for g in $GRPS; do
    HEX_FORCE=$mode perf stat -e instructions -x, ./hex_best/div_mudiv_zn.bin < cases_hex/$g.in >/dev/null 2>/tmp/p.txt
    v=$(grep instructions /tmp/p.txt | cut -d, -f1)
    echo "$g $v"
  done
done
