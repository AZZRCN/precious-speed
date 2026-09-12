#!/bin/bash
set -u
cd /home/azzr/precious_speed

echo "=== compile DEC div.cpp (c++17) ==="
g++ -O3 -std=c++17 -march=znver3 -mtune=znver3 -o dec_best/origin/dec_div_zn.bin dec_best/origin/div.cpp 2>&1 | tail -8
echo "DEC_BUILD_DONE rc=$?"

: > /tmp/cmp2.tsv
for g in length_ratio_0 length_ratio_1 length_ratio_2 length_ratio_3 length_ratio_4 length_ratio_5 amax_0 amax_1 amax_2 max_0 max_1 max_2 bzbound_0 bzbound_1 bzbound_2 bzbound_3 large_0 large_1 power_0 rnear_1 rnear_2 medium_0 small_0; do
  perf stat -e instructions -x, ./dec_best/origin/dec_div_zn.bin < cases_dec/$g.in >/dev/null 2>/tmp/pd.txt
  dv=$(grep instructions /tmp/pd.txt | cut -d, -f1)
  perf stat -e instructions -x, ./hex_best/div_zn.bin < cases_hex/$g.in >/dev/null 2>/tmp/ph.txt
  hv=$(grep instructions /tmp/ph.txt | cut -d, -f1)
  echo "$g $dv $hv" >> /tmp/cmp2.tsv
done
echo "=== CMP (group DEC_inst HEX_inst) ==="
cat /tmp/cmp2.tsv
