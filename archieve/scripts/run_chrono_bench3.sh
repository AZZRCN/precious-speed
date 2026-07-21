#!/bin/bash
# run_chrono_bench3.sh
# 修复 grep -oP 问题，改用 awk 提取
cd /tmp/bench2

bench_chrono() {
    local exe=$1 test_file=$2 runs=${3:-20}
    ./${exe} < ${test_file} > /dev/null 2>&1  # warmup
    local total=0
    for i in $(seq 1 ${runs}); do
        local t=$(./${exe} < ${test_file} 2>&1 > /dev/null | awk '/CPU:/{print $2}')
        if [ -z "$t" ]; then
            echo "ERROR: no CPU output from $exe" >&2
            t=0
        fi
        total=$(awk -v tot=${total} -v t=${t} '{print tot + t}')
    done
    awk -v tot=${total} -v r=${runs} '{printf "%.3f", tot / r}'
}

echo "============================================"
echo "  Internal clock() Benchmark v3 (CPU time, 20 runs avg)"
echo "  Date: $(date '+%Y-%m-%d %H:%M:%S')"
echo "============================================"

echo ""
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
echo "=== Done ==="
