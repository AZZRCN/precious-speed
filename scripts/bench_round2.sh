#!/bin/bash
# Round 2: ADD focus, 5 warmup + 15 timed, report median + min
cd /tmp/o2compare

run_bench() {
    local binary="$1"
    local input="$2"
    for i in 1 2 3 4 5; do
        ./"$binary" < "$input" > /dev/null 2>&1
    done
    local times=()
    for i in $(seq 1 15); do
        local s=$(date +%s%N)
        ./"$binary" < "$input" > /dev/null 2>&1
        local e=$(date +%s%N)
        local ns=$((e - s))
        local ms=$(awk -v ns="$ns" 'BEGIN{printf "%.3f", ns/1000000.0}')
        times+=("$ms")
    done
    local sorted=$(printf '%s\n' "${times[@]}" | sort -n)
    local median=$(echo "$sorted" | sed -n '8p')
    local mn=$(echo "$sorted" | head -1)
    local mx=$(echo "$sorted" | tail -1)
    printf "%-32s median=%s min=%s max=%s\n" "$binary" "$median" "$mn" "$mx"
}

echo "=== Round 2 ADD (1M+1M) ==="
run_bench moptm_fusion_O2_ADD /tmp/fusion/add_1M.in
run_bench mf_ADD              /tmp/fusion/add_1M.in

echo ""
echo "=== Round 2 MUL (500k*500k) ==="
run_bench moptm_fusion_O2_MUL /tmp/fusion/mul_500k.in
run_bench mf_MUL              /tmp/fusion/mul_500k.in

echo ""
echo "=== Round 2 DIV (1M/500k) ==="
run_bench moptm_fusion_O2_DIV /tmp/fusion/div_1M_500k.in
run_bench mf_DIV              /tmp/fusion/div_1M_500k.in

echo ""
echo "=== DONE ==="
