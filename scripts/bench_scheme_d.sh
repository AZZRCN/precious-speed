#!/bin/bash
# Benchmark: 2 warmup + 5 timed, take median (3rd of 5 sorted)
# Compares NEW (mf_*) vs OLD baseline (mf_*_base28)
cd /tmp/o2compare

RUNS=5
WARMUP=2

bench() {
    local bin=$1
    local input=$2
    local times=()
    for i in $(seq 1 $WARMUP); do
        $bin < $input > /dev/null 2>&1
    done
    for i in $(seq 1 $RUNS); do
        local start=$(date +%s.%N)
        $bin < $input > /dev/null 2>&1
        local end=$(date +%s.%N)
        local elapsed=$(awk "BEGIN{printf \"%.3f\", ($end - $start)*1000}")
        times+=($elapsed)
    done
    local sorted=($(printf '%s\n' "${times[@]}" | sort -n))
    echo "${sorted[2]}"
}

echo "==============================================="
echo "  Benchmark: OLD (base28) vs NEW (scheme D)"
echo "  $RUNS runs + $WARMUP warmup, median reported"
echo "==============================================="
echo ""

ADD_IN=/tmp/fusion/add_1M.in
MUL_IN=/tmp/fusion/mul_500k.in
DIV_IN=/tmp/fusion/div_1M_500k.in

echo "[ADD 1M+1M]"
old_add=$(bench ./mf_ADD_base $ADD_IN)
new_add=$(bench ./mf_ADD $ADD_IN)
echo "  OLD: ${old_add}ms"
echo "  NEW: ${new_add}ms"
awk "BEGIN{printf \"  delta: %.2f%%\n\", ($new_add-$old_add)/$old_add*100}"
echo ""

echo "[MUL 500k*500k]"
old_mul=$(bench ./mf_MUL_base $MUL_IN)
new_mul=$(bench ./mf_MUL $MUL_IN)
echo "  OLD: ${old_mul}ms"
echo "  NEW: ${new_mul}ms"
awk "BEGIN{printf \"  delta: %.2f%%\n\", ($new_mul-$old_mul)/$old_mul*100}"
echo ""

echo "[DIV 1M/500k]"
old_div=$(bench ./mf_DIV_base28 $DIV_IN)
new_div=$(bench ./mf_DIV $DIV_IN)
echo "  OLD: ${old_div}ms"
echo "  NEW: ${new_div}ms"
awk "BEGIN{printf \"  delta: %.2f%%\n\", ($new_div-$old_div)/$old_div*100}"
echo ""

echo "==============================================="
echo "Summary (NEW vs OLD baseline):"
awk "BEGIN{
    printf \"  ADD: %s -> %s (%.2f%%)\n\", \"$old_add\", \"$new_add\", (\"$new_add\"-\"$old_add\")/\"$old_add\"*100
    printf \"  MUL: %s -> %s (%.2f%%)\n\", \"$old_mul\", \"$new_mul\", (\"$new_mul\"-\"$old_mul\")/\"$old_mul\"*100
    printf \"  DIV: %s -> %s (%.2f%%)\n\", \"$old_div\", \"$new_div\", (\"$new_div\"-\"$old_div\")/\"$old_div\"*100
}"
echo ""
echo "Previous reference: ADD 5.69 / MUL 11.47 / DIV 28.33ms"
