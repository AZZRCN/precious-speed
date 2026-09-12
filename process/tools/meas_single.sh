#!/bin/bash
set -u
cd /home/azzr/precious_speed

# spot-check correctness on na>>nb single-pair (DEC output -> verify Q*B+R==A)
./dec_best/origin/dec_div_zn.bin < cases_dec1/length_ratio_2.in > /tmp/dec_lr2.out 2>/dev/null; echo "DEC_lr2 rc=$? out=$(wc -l < /tmp/dec_lr2.out)"
./dec_best/origin/dec_div_zn.bin < cases_dec1/amax_1.in > /tmp/dec_amax1.out 2>/dev/null; echo "DEC_amax1 rc=$? out=$(wc -l < /tmp/dec_amax1.out)"

: > /tmp/cmp1.tsv
for g in length_ratio_0 length_ratio_1 length_ratio_2 length_ratio_3 length_ratio_4 length_ratio_5 amax_0 amax_1 amax_2 max_0 max_1 max_2 bzbound_0 large_0 rnear_2 medium_0; do
  perf stat -e instructions -x, ./dec_best/origin/dec_div_zn.bin < cases_dec1/$g.in >/dev/null 2>/tmp/pd.txt
  dv=$(grep instructions /tmp/pd.txt | cut -d, -f1)
  perf stat -e instructions -x, ./hex_best/div_zn.bin < cases_hex1/$g.in >/dev/null 2>/tmp/ph.txt
  hv=$(grep instructions /tmp/ph.txt | cut -d, -f1)
  echo "$g $dv $hv" >> /tmp/cmp1.tsv
done
echo "=== CMP single-pair (group DEC_inst HEX_inst) ==="
cat /tmp/cmp1.tsv
