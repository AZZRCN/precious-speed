#!/bin/bash
# Head-to-head benchmark: OLD (.bak) vs NEW binary
# 2 warmup + 9 timed, take median (5th of 9 sorted)
cd /tmp/o2compare

RUNS=9
WARMUP=2

bench() {
    local bin=$1
    local input=$2
    local times=()
    # warmup
    for i in $(seq 1 $WARMUP); do
        $bin < $input > /dev/null 2>&1
    done
    # timed
    for i in $(seq 1 $RUNS); do
        local start=$(date +%s.%N)
        $bin < $input > /dev/null 2>&1
        local end=$(date +%s.%N)
        local elapsed=$(awk "BEGIN{printf \"%.3f\", ($end - $start)*1000}")
        times+=($elapsed)
    done
    # sort and take median (index 4, 0-based, since 9 runs)
    local sorted=($(printf '%s\n' "${times[@]}" | sort -n))
    echo "${sorted[4]}"
}

echo "==============================================="
echo "  Head-to-Head Benchmark: OLD vs NEW (Barrett)"
echo "  $RUNS runs + $WARMUP warmup, median reported"
echo "==============================================="
echo ""

# ADD: 1M + 1M
echo "[ADD 1M+1M]"
old_add=$(bench ./mf_ADD.bak /tmp/fusion/add_1M.in)
new_add=$(bench ./mf_ADD /tmp/fusion/add_1M.in)
echo "  OLD: ${old_add}ms"
echo "  NEW: ${new_add}ms"
awk "BEGIN{printf \"  delta: %.2f%%\n\", ($new_add-$old_add)/$old_add*100}"
echo ""

# MUL: 500k * 500k
echo "[MUL 500k*500k]"
old_mul=$(bench ./mf_MUL.bak /tmp/fusion/mul_500k.in)
new_mul=$(bench ./mf_MUL /tmp/fusion/mul_500k.in)
echo "  OLD: ${old_mul}ms"
echo "  NEW: ${new_mul}ms"
awk "BEGIN{printf \"  delta: %.2f%%\n\", ($new_mul-$old_mul)/$old_mul*100}"
echo ""

# DIV: 1M / 500k
echo "[DIV 1M/500k]"
old_div=$(bench ./mf_DIV.bak /tmp/fusion/div_1M_500k.in)
new_div=$(bench ./mf_DIV /tmp/fusion/div_1M_500k.in)
echo "  OLD: ${old_div}ms"
echo "  NEW: ${new_div}ms"
awk "BEGIN{printf \"  delta: %.2f%%\n\", ($new_div-$old_div)/$old_div*100}"
echo ""

echo "==============================================="
echo "Summary:"
awk "BEGIN{
    old_add=$old_add; new_add=$new_add
    old_mul=$old_mul; new_mul=$new_mul
    old_div=$old_div; new_div=$new_div
    printf \"  ADD: %s -> %s (%.2f%%)\n\", old_add, new_add, (new_add-old_add)/old_add*100
    printf \"  MUL: %s -> %s (%.2f%%)\n\", old_mul, new_mul, (new_mul-old_mul)/old_mul*100
    printf \"  DIV: %s -> %s (%.2f%%)\n\", old_div, new_div, (new_div-old_div)/old_div*100
}"
