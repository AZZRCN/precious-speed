#!/bin/bash
# Benchmark moptm_fusion_O2 vs moptm_O2 (baseline)
# 2 warmup + 5 timed, median in ms
cd /tmp/o2compare

run_bench() {
    local binary="$1"
    local input="$2"
    local label="$3"
    # 3 warmup
    ./"$binary" < "$input" > /dev/null 2>&1
    ./"$binary" < "$input" > /dev/null 2>&1
    ./"$binary" < "$input" > /dev/null 2>&1
    # 9 timed
    local times=()
    for i in 1 2 3 4 5 6 7 8 9; do
        local s=$(date +%s.%N)
        ./"$binary" < "$input" > /dev/null 2>&1
        local e=$(date +%s.%N)
        local ms=$(awk -v s="$s" -v e="$e" 'BEGIN{printf "%.3f", (e-s)*1000}')
        times+=("$ms")
    done
    # median = 5th of 9 sorted ascending
    local median=$(printf '%s\n' "${times[@]}" | sort -n | sed -n '5p')
    printf "%-30s median=%s ms  (runs: %s)\n" "$label" "$median" "${times[*]}"
}

echo "=== ADD (1M) ==="
run_bench moptm_O2_ADD        /tmp/fusion/add_1M.in      "moptm_O2_ADD"
run_bench moptm_fusion_O2_ADD /tmp/fusion/add_1M.in      "moptm_fusion_O2_ADD"

echo "=== MUL (500k) ==="
run_bench moptm_O2_MUL        /tmp/fusion/mul_500k.in     "moptm_O2_MUL"
run_bench moptm_fusion_O2_MUL /tmp/fusion/mul_500k.in     "moptm_fusion_O2_MUL"

echo "=== DIV (1M/500k) ==="
run_bench moptm_O2_DIV        /tmp/fusion/div_1M_500k.in  "moptm_O2_DIV"
run_bench moptm_fusion_O2_DIV /tmp/fusion/div_1M_500k.in  "moptm_fusion_O2_DIV"

echo "=== DONE ==="
