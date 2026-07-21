#!/bin/bash
cd /tmp/bench
echo "=== DIV correctness ==="
for f in div_1M_500k.txt div_200k_100k.txt div_1M_100k.txt; do
    md5_old=$(./moptm_fusion_v2_DIV_baseline < "$f" | md5sum)
    md5_new=$(./moptm_fusion_v3_DIV < "$f" | md5sum)
    if [ "$md5_old" = "$md5_new" ]; then status="PASS"; else status="FAIL"; fi
    echo "$f: $status  old=$md5_old new=$md5_new"
done
echo "=== ADD correctness ==="
for f in add_1M.txt; do
    md5_old=$(./moptm_fusion_O2_ADD < "$f" | md5sum)
    md5_new=$(./moptm_fusion_v3_ADD < "$f" | md5sum)
    if [ "$md5_old" = "$md5_new" ]; then status="PASS"; else status="FAIL"; fi
    echo "$f: $status  old=$md5_old new=$md5_new"
done
echo "=== MUL correctness ==="
for f in mul_500k.txt; do
    md5_old=$(./moptm_fusion_O2_MUL < "$f" | md5sum)
    md5_new=$(./moptm_fusion_v3_MUL < "$f" | md5sum)
    if [ "$md5_old" = "$md5_new" ]; then status="PASS"; else status="FAIL"; fi
    echo "$f: $status  old=$md5_old new=$md5_new"
done
echo "=== Done ==="
