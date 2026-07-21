#!/bin/bash
# Stable benchmark: 5 warmup + 15 timed, interleaved, median (8th of 15)
cd /tmp/o2compare

run_bench() {
    local binary="$1"
    local input="$2"
    # 5 warmup
    for i in 1 2 3 4 5; do
        ./"$binary" < "$input" > /dev/null 2>&1
    done
    # 15 timed
    local times=()
    for i in $(seq 1 15); do
        local s=$(date +%s%N)
        ./"$binary" < "$input" > /dev/null 2>&1
        local e=$(date +%s%N)
        local ns=$((e - s))
        local ms=$(awk -v ns="$ns" 'BEGIN{printf "%.3f", ns/1000000.0}')
        times+=("$ms")
    done
    # median = 8th of 15 sorted ascending
    local median=$(printf '%s\n' "${times[@]}" | sort -n | sed -n '8p')
    local mn=$(printf '%s\n' "${times[@]}" | sort -n | head -1)
    local mx=$(printf '%s\n' "${times[@]}" | sort -n | tail -1)
    printf "%-32s median=%s ms  [min=%s max=%s]\n" "$binary" "$median" "$mn" "$mx"
}

echo "=== ADD (1M+1M) ==="
run_bench moptm_O2_ADD        /tmp/fusion/add_1M.in
run_bench moptm_fusion_O2_ADD /tmp/fusion/add_1M.in
run_bench mf_ADD              /tmp/fusion/add_1M.in

echo ""
echo "=== MUL (500k*500k) ==="
run_bench moptm_O2_MUL        /tmp/fusion/mul_500k.in
run_bench moptm_fusion_O2_MUL /tmp/fusion/mul_500k.in
run_bench mf_MUL              /tmp/fusion/mul_500k.in

echo ""
echo "=== DIV (1M/500k) ==="
run_bench moptm_O2_DIV        /tmp/fusion/div_1M_500k.in
run_bench moptm_fusion_O2_DIV /tmp/fusion/div_1M_500k.in
run_bench mf_DIV              /tmp/fusion/div_1M_500k.in

echo ""
echo "=== DONE ==="
