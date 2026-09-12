#!/bin/bash
# AMD VM: 编译 baseline(div.cpp) 与 cache 实验(div_cache.cpp), 测 instructions + cycles
set -u
cd /home/azzr/precious_speed
g++ -O3 -std=c++17 -march=znver3 -mtune=znver3 -o hex_best/div_zn.bin hex_best/div.cpp 2>&1 | tail -1
g++ -O3 -std=c++17 -march=znver3 -mtune=znver3 -o hex_best/div_cache_zn.bin hex_best/div_cache.cpp 2>&1 | tail -1
echo "BUILD_DONE"

GRPS="length_ratio_2 length_ratio_5 amax_1 medium_0 large_0"
for g in $GRPS; do
  echo "===== $g ====="
  perf stat -e instructions,cycles -x, ./hex_best/div_zn.bin < cases_hex/$g.in >/dev/null 2>/tmp/b.txt
  bi=$(grep '^instructions,' /tmp/b.txt | cut -d, -f1); bc=$(grep '^cycles,' /tmp/b.txt | cut -d, -f1)
  perf stat -e instructions,cycles -x, ./hex_best/div_cache_zn.bin < cases_hex/$g.in >/dev/null 2>/tmp/c.txt
  ci=$(grep '^instructions,' /tmp/c.txt | cut -d, -f1); cc=$(grep '^cycles,' /tmp/c.txt | cut -d, -f1)
  echo "baseline  inst=$bi cyc=$bc"
  echo "cacheexp  inst=$ci cyc=$cc"
done
