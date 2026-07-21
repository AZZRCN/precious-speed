#!/bin/bash
# Benchmark: new mf_{ADD,MUL,DIV} vs prev (.prev 备份)
# 方法：5 warmup + 21 timed，取中位数 / min / max
# 每个二进制独立 warmup，避免缓存干扰
cd /tmp/o2compare

run_bench() {
    local binary="$1"
    local input="$2"
    local i
    for i in 1 2 3 4 5; do
        ./"$binary" < "$input" > /dev/null 2>&1
    done
    local times=()
    for i in $(seq 1 21); do
        local s=$(date +%s%N)
        ./"$binary" < "$input" > /dev/null 2>&1
        local e=$(date +%s%N)
        local ns=$((e - s))
        local ms=$(awk -v ns="$ns" 'BEGIN{printf "%.3f", ns/1000000.0}')
        times+=("$ms")
    done
    local sorted=$(printf '%s\n' "${times[@]}" | sort -n)
    # 21 个值，中位数是第 11 个
    local median=$(echo "$sorted" | sed -n '11p')
    local mn=$(echo "$sorted" | head -1)
    local mx=$(echo "$sorted" | tail -1)
    printf "%-28s median=%s min=%s max=%s\n" "$binary" "$median" "$mn" "$mx"
}

echo "=== ADD (1M+1M) ==="
run_bench mf_ADD       /tmp/fusion/add_1M.in
run_bench mf_ADD.prev  /tmp/fusion/add_1M.in

echo ""
echo "=== MUL (500k*500k) ==="
run_bench mf_MUL       /tmp/fusion/mul_500k.in
run_bench mf_MUL.prev  /tmp/fusion/mul_500k.in

echo ""
echo "=== DIV (1M/500k) ==="
run_bench mf_DIV       /tmp/fusion/div_1M_500k.in
run_bench mf_DIV.prev  /tmp/fusion/div_1M_500k.in

echo ""
echo "=== DONE ==="
