#!/bin/bash
# Focused MUL benchmark: interleave old/new to reduce thermal bias
cd /tmp/o2compare

run_bench() {
    local binary="$1"
    local input="$2"
    for i in 1 2; do ./"$binary" < "$input" > /dev/null 2>&1; done
    local times=()
    for i in 1 2 3 4 5 6 7; do
        local s=$(date +%s%N)
        ./"$binary" < "$input" > /dev/null 2>&1
        local e=$(date +%s%N)
        times+=($(awk -v ns=$((e - s)) 'BEGIN{printf "%.3f", ns/1000000.0}'))
    done
    local median=$(printf '%s\n' "${times[@]}" | sort -n | sed -n '4p')
    local mn=$(printf '%s\n' "${times[@]}" | sort -n | head -1)
    local mx=$(printf '%s\n' "${times[@]}" | sort -n | tail -1)
    printf "%-28s median=%s ms  [min=%s max=%s]\n" "$binary" "$median" "$mn" "$mx"
}

echo "=== MUL interleaved (7 timed, median=4th) ==="
for round in 1 2 3; do
    echo "--- round $round ---"
    run_bench moptm_fusion_O2_MUL /tmp/fusion/mul_500k.in
    run_bench mf_MUL               /tmp/fusion/mul_500k.in
done
