#!/bin/bash
set -u
cd /home/azzr/precious_speed

echo "=== compile DEC div.cpp (c++20 -O2 -DHP_ARENA_OFF) ==="
g++ -O2 -std=c++20 -DHP_ARENA_OFF -o dec_best/origin/dec_div_zn.bin dec_best/origin/div.cpp 2>&1 | tail -6
echo "DEC_BUILD_DONE rc=$?"

# spot-check correctness (need Q*B+R==A, 0<=R<B); output -> files for local verify
for g in amax_0 length_ratio_2 amax_1; do
  ./dec_best/origin/dec_div_zn.bin < cases_dec1/$g.in > /tmp/dec_$g.out 2>/dev/null
  echo "DEC_$g rc=$? lines=$(wc -l < /tmp/dec_$g.out)"
done

: > /tmp/cmp1.tsv
for g in length_ratio_0 length_ratio_1 length_ratio_2 length_ratio_3 length_ratio_4 length_ratio_5 amax_0 amax_1 amax_2 max_0 max_1 max_2 bzbound_0 large_0 rnear_2 medium_0; do
  perf stat -e instructions -x, ./dec_best/origin/dec_div_zn.bin < cases_dec1/$g.in >/dev/null 2>/tmp/pd.txt
  dv=$(grep instructions /tmp/pd.txt | cut -d, -f1)
  perf stat -e instructions -x, ./hex_best/div_zn.bin < cases_hex1/$g.in >/dev/null 2>/tmp/ph.txt
  hv=$(grep instructions /tmp/pd.txt | cut -d, -f1)
  hv=$(grep instructions /tmp/ph.txt | cut -d, -f1)
  echo "$g $dv $hv" >> /tmp/cmp1.tsv
done
echo "=== CMP single-pair (-DHP_ARENA_OFF) (group DEC_inst HEX_inst) ==="
cat /tmp/cmp1.tsv
