#!/bin/bash
# simple_chrono_bench.sh
# 简化版：不用函数，直接内联，10 次平均
cd /tmp/bench2

RUNS=10

bench() {
    local exe=$1 test=$2
    ./$exe < $test > /dev/null 2>&1  # warmup
    local total=0
    for i in $(seq 1 $RUNS); do
        t=$(./$exe < $test 2>&1 1>/dev/null | awk '/CPU:/{print $2}')
        total=$(awk -v t=$total -v n=$t 'BEGIN{print t+n}')
    done
    awk -v t=$total -v r=$RUNS 'BEGIN{printf "%.3f", t/r}'
}

echo "=== ADD ==="
for t in test_add_100k_100k.txt test_add_500k_500k.txt test_add_1M_1M.txt; do
    [ -f $t ] || continue
    a=$(bench moptm_ADD_bench $t)
    d=$(bench moptm_ADD_bench_o3avx2 $t)
    echo "  $t: -O2=${a}ms -O3+avx2=${d}ms"
done

echo "=== MUL ==="
for t in test_mul_100k_100k.txt test_mul_300k_300k.txt test_mul_500k_500k.txt; do
    [ -f $t ] || continue
    a=$(bench moptm_MUL_bench $t)
    d=$(bench moptm_MUL_bench_o3avx2 $t)
    echo "  $t: -O2=${a}ms -O3+avx2=${d}ms"
done

echo "=== DIV ==="
for t in test_div_100k_10k.txt test_div_1M_100k.txt test_div_1M_500k.txt test_div_1M_900k.txt test_div_1M_999k.txt; do
    [ -f $t ] || continue
    a=$(bench moptm_DIV_bench $t)
    d=$(bench moptm_DIV_bench_o3avx2 $t)
    echo "  $t: -O2=${a}ms -O3+avx2=${d}ms"
done

echo "=== Done ==="
