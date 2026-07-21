#!/bin/bash
cd /tmp/bench
echo "=== moptm_fusion vs moptm_O2 (7 runs, drop min/max, median of 5) ==="
for f in div_1M_500k.txt div_200k_100k.txt div_1M_100k.txt; do
    echo "--- $f ---"
    for bin in moptm_fusion_DIV moptm_O2_DIV; do
        # 2 warmup
        ./$bin < $f > /dev/null 2>&1
        ./$bin < $f > /dev/null 2>&1
        # 7 runs
        times=()
        for i in 1 2 3 4 5 6 7; do
            t=$( { /usr/bin/time -f '%e' ./$bin < $f > /dev/null; } 2>&1 )
            times+=($t)
        done
        # sort and pick median (4th of 7)
        sorted=($(printf '%s\n' "${times[@]}" | sort -n))
        median=${sorted[3]}
        printf '%-20s: median=%s  all=%s\n' "$bin" "$median" "${times[*]}"
    done
done
