#!/bin/bash
# 交替 benchmark：new 与 prev 交替执行，消除时间漂移
# 31 次交替（每个二进制 31 次），报告 min / median / p25 / p75
cd /tmp/o2compare

run_alt() {
    local new_bin="$1"
    local prev_bin="$2"
    local input="$3"
    local i
    # warmup
    for i in 1 2 3 4 5; do
        ./"$new_bin"  < "$input" > /dev/null 2>&1
        ./"$prev_bin" < "$input" > /dev/null 2>&1
    done
    local new_times=()
    local prev_times=()
    for i in $(seq 1 31); do
        local s e ns ms
        s=$(date +%s%N); ./"$new_bin"  < "$input" > /dev/null 2>&1; e=$(date +%s%N); ns=$((e-s)); ms=$(awk -v ns="$ns" 'BEGIN{printf "%.3f",ns/1000000.0}'); new_times+=("$ms")
        s=$(date +%s%N); ./"$prev_bin" < "$input" > /dev/null 2>&1; e=$(date +%s%N); ns=$((e-s)); ms=$(awk -v ns="$ns" 'BEGIN{printf "%.3f",ns/1000000.0}'); prev_times+=("$ms")
    done
    local new_sorted=$(printf '%s\n' "${new_times[@]}"  | sort -n)
    local prev_sorted=$(printf '%s\n' "${prev_times[@]}" | sort -n)
    local new_min=$(echo "$new_sorted"  | head -1)
    local new_med=$(echo "$new_sorted"  | sed -n '16p')
    local new_p25=$(echo "$new_sorted" | sed -n '8p')
    local prev_min=$(echo "$prev_sorted" | head -1)
    local prev_med=$(echo "$prev_sorted" | sed -n '16p')
    local prev_p25=$(echo "$prev_sorted" | sed -n '8p')
    printf "  NEW  %s: min=%s  p25=%s  median=%s\n" "$new_bin"  "$new_min"  "$new_p25"  "$new_med"
    printf "  PREV %s: min=%s  p25=%s  median=%s\n" "$prev_bin" "$prev_min" "$prev_p25" "$prev_med"
    # 计算 min 提升百分比（负=变慢）
    local delta=$(awk -v n="$new_min" -v p="$prev_min" 'BEGIN{printf "%+.2f%%", (p-n)/p*100}')
    printf "  min delta (prev->new): %s\n" "$delta"
}

echo "=== ADD (1M+1M) ==="
run_alt mf_ADD mf_ADD.prev /tmp/fusion/add_1M.in
echo ""
echo "=== MUL (500k*500k) ==="
run_alt mf_MUL mf_MUL.prev /tmp/fusion/mul_500k.in
echo ""
echo "=== DIV (1M/500k) ==="
run_alt mf_DIV mf_DIV.prev /tmp/fusion/div_1M_500k.in
echo ""
echo "=== DONE ==="
