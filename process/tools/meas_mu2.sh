#!/bin/bash
# 在 VM 上编译 div.cpp(baseline) 与 div_mudiv2.cpp, 先验正确性(逐例 diff), 再测指令数 MU=0 vs MU=1
set -u
cd /home/azzr/precious_speed
g++ -O3 -std=c++17 -march=znver3 -mtune=znver3 -o hex_best/base_zn.bin hex_best/div.cpp 2>&1 | tail -3
g++ -O3 -std=c++17 -march=znver3 -mtune=znver3 -o hex_best/mu2_zn.bin  hex_best/div_mudiv2.cpp 2>&1 | tail -3
echo "BUILD_DONE"

GRPS="length_ratio_0 length_ratio_1 length_ratio_2 length_ratio_3 length_ratio_4 length_ratio_5 amax_0 amax_1 amax_2 max_0 max_1 max_2 large_0 large_1 medium_0 medium_1 medium_2 bzbound_0 bzbound_1 bzbound_2 bzbound_3 rnear_0 rnear_1 rnear_2 small_0 power_0"

echo "===== CORRECTNESS (mu2 MU=1 vs base) ====="
for g in $GRPS; do
  ./hex_best/base_zn.bin < cases_hex/$g.in > /tmp/o_base.txt 2>/dev/null
  MU=1 ./hex_best/mu2_zn.bin < cases_hex/$g.in > /tmp/o_mu.txt 2>/dev/null
  if cmp -s /tmp/o_base.txt /tmp/o_mu.txt; then echo "$g OK"; else echo "$g MISMATCH"; fi
done

echo "===== INSTRUCTIONS (M) ====="
printf "%-16s %12s %12s %12s %8s\n" case base mu0 mu1 "mu1/base"
for g in $GRPS; do
  perf stat -e instructions -x, ./hex_best/base_zn.bin < cases_hex/$g.in >/dev/null 2>/tmp/p0.txt
  b=$(grep instructions /tmp/p0.txt | cut -d, -f1)
  perf stat -e instructions -x, ./hex_best/mu2_zn.bin  < cases_hex/$g.in >/dev/null 2>/tmp/p1.txt
  m0=$(grep instructions /tmp/p1.txt | cut -d, -f1)
  MU=1 perf stat -e instructions -x, ./hex_best/mu2_zn.bin < cases_hex/$g.in >/dev/null 2>/tmp/p2.txt
  m1=$(grep instructions /tmp/p2.txt | cut -d, -f1)
  r=$(awk -v a="$m1" -v b="$b" 'BEGIN{if(b>0)printf "%.4f", a/b; else print "-"}')
  printf "%-16s %12.1f %12.1f %12.1f %8s\n" "$g" $(awk -v x="$b" 'BEGIN{print x/1e6}') $(awk -v x="$m0" 'BEGIN{print x/1e6}') $(awk -v x="$m1" 'BEGIN{print x/1e6}') "$r"
done
