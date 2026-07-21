#!/bin/bash
# Benchmark: 2 warmup + 5 timed, median
cd /tmp/o2compare
run_bench() {
    local binary="$1"
    local input="$2"
    local label="$3"
    ./"$binary" < "$input" > /dev/null 2>&1
    ./"$binary" < "$input" > /dev/null 2>&1
    local times=()
    for i in 1 2 3 4 5; do
        local s=$(date +%s.%N)
        ./"$binary" < "$input" > /dev/null 2>&1
        local e=$(date +%s.%N)
        local ms=$(awk -v s="$s" -v e="$e" 'BEGIN{printf "%.3f", (e-s)*1000}')
        times+=("$ms")
    done
    local median=$(printf '%s\n' "${times[@]}" | sort -n | sed -n '3p')
    printf "%-30s median=%s ms  (runs: %s)\n" "$label" "$median" "${times[*]}"
}
echo "=== ADD (1M) ==="
run_bench mf_ADD /tmp/fusion/add_1M.in "mf_ADD (barrett)"
echo "=== MUL (500k) ==="
run_bench mf_MUL /tmp/fusion/mul_500k.in "mf_MUL (barrett)"
echo "=== DIV (1M/500k) ==="
run_bench mf_DIV /tmp/fusion/div_1M_500k.in "mf_DIV (barrett)"
echo "=== DONE ==="
