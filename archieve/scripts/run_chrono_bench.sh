#!/bin/bash
# run_chrono_bench.sh
# 运行带内部 clock() 计时的 moptm，提取 CPU 时间
cd /tmp/bench2

bench_chrono() {
    local exe=$1 test_file=$2 runs=${3:-20}
    ./${exe} < ${test_file} > /dev/null 2>&1  # warmup
    local total=0
    for i in $(seq 1 ${runs}); do
        local t=$(./${exe} < ${test_file} 2>&1 > /dev/null | grep -oP 'CPU: \K[0-9.]+')
        total=$(awk -v tot=${total} -v t=${t} '{print tot + t}')
    done
    awk -v tot=${total} -v r=${runs} '{printf "%.3f", tot / r}'
}

echo "============================================"
echo "  Internal clock() Benchmark (CPU time, 20 runs avg)"
echo "  Date: $(date '+%Y-%m-%d %H:%M:%S')"
echo "============================================"

echo ""
echo "=== moptm -O2 vs moptm -O2+O3+AVX2 vs best -O2 ==="
echo "--- ADD ---"
for test in test_add_100k_100k.txt test_add_500k_500k.txt test_add_1M_1M.txt; do
    [ -f ${test} ] || continue
    a=$(bench_chrono moptm_ADD_bench ${test})
    d=$(bench_chrono moptm_ADD_bench_o3avx2 ${test})
    echo "  ${test}: -O2=${a}ms  -O3+avx2=${d}ms"
done

echo "--- MUL ---"
for test in test_mul_100k_100k.txt test_mul_300k_300k.txt test_mul_500k_500k.txt; do
    [ -f ${test} ] || continue
    a=$(bench_chrono moptm_MUL_bench ${test})
    d=$(bench_chrono moptm_MUL_bench_o3avx2 ${test})
    echo "  ${test}: -O2=${a}ms  -O3+avx2=${d}ms"
done

echo "--- DIV ---"
for test in test_div_100k_10k.txt test_div_1M_100k.txt test_div_1M_500k.txt test_div_1M_900k.txt test_div_1M_999k.txt; do
    [ -f ${test} ] || continue
    a=$(bench_chrono moptm_DIV_bench ${test})
    d=$(bench_chrono moptm_DIV_bench_o3avx2 ${test})
    echo "  ${test}: -O2=${a}ms  -O3+avx2=${d}ms"
done

echo ""
echo "=== best (for reference) ==="
# best 没有 chrono 计时，用 date +%s%N 测墙钟时间
bench_ns() {
    local exe=$1 test_file=$2 runs=${3:-20}
    ./${exe} < ${test_file} > /dev/null 2>&1
    local total_ns=0
    for i in $(seq 1 ${runs}); do
        local start=$(date +%s%N)
        ./${exe} < ${test_file} > /dev/null 2>&1
        local end=$(date +%s%N)
        total_ns=$((total_ns + end - start))
    done
    echo $((total_ns / runs / 1000000))
}

echo "  best ADD 1M:    $(bench_ns best_ADD test_add_1M_1M.txt)ms"
echo "  best MUL 500k:  $(bench_ns best_MUL test_mul_500k_500k.txt)ms"
echo "  best DIV 1M_500k: $(bench_ns best_DIV test_div_1M_500k.txt)ms"
echo "  best DIV 1M_100k: $(bench_ns best_DIV test_div_1M_100k.txt)ms"

echo ""
echo "============================================"
echo "  Done"
echo "============================================"
