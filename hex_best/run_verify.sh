#!/bin/bash
# 全量 oracle 验收: 默认精确线性二进制 div_base16_zn 对 9 gen x seed 1..8
cd ~/precious_speed/hex_best
GENS="a_max_b_random burnikel_ziegler_bound large length_ratio_integer max medium power r_nearly_zero small"
for g in $GENS; do
  for s in 1 2 3 4 5 6 7 8; do
    python3 verify_oracle.py "$g" "$s" ./div_base16_zn 2>/dev/null
  done
done > verify_full.txt 2>&1
echo DONE_ALL >> verify_full.txt
# 汇总
cyc=$(grep -oE 'cyc_bad=[0-9]+' verify_full.txt | awk -F= '{s+=$2} END{print s+0}')
lin=$(grep -oE 'lin_bad=[0-9]+' verify_full.txt | awk -F= '{s+=$2} END{print s+0}')
echo "TOTAL cyc_bad=$cyc lin_bad=$lin" >> verify_full.txt
