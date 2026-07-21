#!/bin/bash
# Benchmark: new 16-byte SIMD fusion vs old fusion vs moptm_O2 baseline
# 2 warmup + 9 timed, median (5th of 9 sorted ascending), nanosecond timing -> ms
cd /tmp/o2compare

run_bench() {
    local binary="$1"
    local input="$2"
    # 2 warmup
    ./"$binary" < "$input" > /dev/null 2>&1
    ./"$binary" < "$input" > /dev/null 2>&1
    # 9 timed
    local times=()
    for i in 1 2 3 4 5 6 7 8 9; do
        local s=$(date +%s%N)
        ./"$binary" < "$input" > /dev/null 2>&1
        local e=$(date +%s%N)
        local ns=$((e - s))
        local ms=$(awk -v ns="$ns" 'BEGIN{printf "%.3f", ns/1000000.0}')
        times+=("$ms")
    done
    # median = 5th of 9 sorted ascending
    local median=$(printf '%s\n' "${times[@]}" | sort -n | sed -n '5p')
    printf "%-32s median=%s ms  (runs: %s)\n" "$binary" "$median" "${times[*]}"
}

echo "=== ADD (1M+1M) input=/tmp/fusion/add_1M.in ==="
run_bench moptm_O2_ADD        /tmp/fusion/add_1M.in
run_bench moptm_fusion_O2_ADD /tmp/fusion/add_1M.in
run_bench mf_ADD              /tmp/fusion/add_1M.in

echo ""
echo "=== MUL (500k*500k) input=/tmp/fusion/mul_500k.in ==="
run_bench moptm_O2_MUL        /tmp/fusion/mul_500k.in
run_bench moptm_fusion_O2_MUL /tmp/fusion/mul_500k.in
run_bench mf_MUL              /tmp/fusion/mul_500k.in

echo ""
echo "=== DIV (1M/500k) input=/tmp/fusion/div_1M_500k.in ==="
run_bench moptm_O2_DIV        /tmp/fusion/div_1M_500k.in
run_bench moptm_fusion_O2_DIV /tmp/fusion/div_1M_500k.in
run_bench mf_DIV              /tmp/fusion/div_1M_500k.in

echo ""
echo "=== DONE ==="
