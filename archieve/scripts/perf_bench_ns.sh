#!/bin/bash
# perf_bench_ns.sh
# 纳秒级计时（date +%s%N），解决 /usr/bin/time 精度问题
cd /tmp/bench2

bench_ns() {
    local exe=$1 test_file=$2 runs=${3:-10}
    ./${exe} < ${test_file} > /dev/null 2>&1  # warmup
    local total_ns=0
    for i in $(seq 1 ${runs}); do
        local start=$(date +%s%N)
        ./${exe} < ${test_file} > /dev/null 2>&1
        local end=$(date +%s%N)
        total_ns=$((total_ns + end - start))
    done
    # 输出毫秒
    echo $((total_ns / runs / 1000000))
}

echo "============================================"
echo "  NS Precision Benchmark (wall time, 10 runs avg)"
echo "  Date: $(date '+%Y-%m-%d %H:%M:%S')"
echo "============================================"

echo ""
echo "=== moptm vs best (LC -O2) ==="
echo "--- ADD ---"
for test in test_add_100k_100k.txt test_add_500k_500k.txt test_add_1M_1M.txt; do
    [ -f ${test} ] || continue
    m=$(bench_ns moptm_ADD ${test})
    b=$(bench_ns best_ADD ${test})
    r=$(awk "BEGIN{printf \"%.2f\", ${m} / ${b}}")
    echo "  ${test}: moptm=${m}ms best=${b}ms ratio=${r}x"
done

echo "--- MUL ---"
for test in test_mul_100k_100k.txt test_mul_300k_300k.txt test_mul_500k_500k.txt; do
    [ -f ${test} ] || continue
    m=$(bench_ns moptm_MUL ${test})
    b=$(bench_ns best_MUL ${test})
    r=$(awk "BEGIN{printf \"%.2f\", ${m} / ${b}}")
    echo "  ${test}: moptm=${m}ms best=${b}ms ratio=${r}x"
done

echo "--- DIV ---"
for test in test_div_100k_10k.txt test_div_1M_100k.txt test_div_1M_500k.txt test_div_1M_900k.txt test_div_1M_999k.txt; do
    [ -f ${test} ] || continue
    m=$(bench_ns moptm_DIV ${test})
    b=$(bench_ns best_DIV ${test})
    r=$(awk "BEGIN{printf \"%.2f\", ${m} / ${b}}")
    echo "  ${test}: moptm=${m}ms best=${b}ms ratio=${r}x"
done

echo ""
echo "=== Pragma 对比 (10 runs avg) ==="

echo ""
echo "  DIV 1M_500k:"
a=$(bench_ns moptm_DIV test_div_1M_500k.txt)
b=$(bench_ns moptm_DIV_avx2 test_div_1M_500k.txt)
c=$(bench_ns moptm_DIV_o3 test_div_1M_500k.txt)
d=$(bench_ns moptm_DIV_o3avx2 test_div_1M_500k.txt)
echo "  [A] -O2:              ${a}ms"
echo "  [B] -O2+target(avx2): ${b}ms"
echo "  [C] -O2+opt(O3):      ${c}ms"
echo "  [D] -O2+opt(O3)+avx2: ${d}ms"

echo ""
echo "  MUL 500k_500k:"
a=$(bench_ns moptm_MUL test_mul_500k_500k.txt)
b=$(bench_ns moptm_MUL_avx2 test_mul_500k_500k.txt)
c=$(bench_ns moptm_MUL_o3 test_mul_500k_500k.txt)
d=$(bench_ns moptm_MUL_o3avx2 test_mul_500k_500k.txt)
echo "  [A] -O2:              ${a}ms"
echo "  [B] -O2+target(avx2): ${b}ms"
echo "  [C] -O2+opt(O3):      ${c}ms"
echo "  [D] -O2+opt(O3)+avx2: ${d}ms"

echo ""
echo "  ADD 1M_1M:"
a=$(bench_ns moptm_ADD test_add_1M_1M.txt)
b=$(bench_ns moptm_ADD_avx2 test_add_1M_1M.txt)
c=$(bench_ns moptm_ADD_o3 test_add_1M_1M.txt)
d=$(bench_ns moptm_ADD_o3avx2 test_add_1M_1M.txt)
echo "  [A] -O2:              ${a}ms"
echo "  [B] -O2+target(avx2): ${b}ms"
echo "  [C] -O2+opt(O3):      ${c}ms"
echo "  [D] -O2+opt(O3)+avx2: ${d}ms"

echo ""
echo "============================================"
echo "  Done"
echo "============================================"
