#!/bin/bash
# rebuild and run chrono bench
cd /tmp/bench2

# 重新生成 o3avx2 版本（包含 ADD 修复）
sed '1i\#pragma GCC target("avx2,fma")\n#pragma GCC optimize("O3,unroll-loops")' moptm_bench.cpp > moptm_bench_o3avx2.cpp
LC_FLAGS="-O2 -std=gnu++20 -static -DONLINE_JUDGE"
g++ ${LC_FLAGS} -DHINT_OP_ADD -o moptm_ADD_bench_o3avx2 moptm_bench_o3avx2.cpp && echo "  ADD O3AVX2 OK"
g++ ${LC_FLAGS} -DHINT_OP_MUL -o moptm_MUL_bench_o3avx2 moptm_bench_o3avx2.cpp && echo "  MUL O3AVX2 OK"
g++ ${LC_FLAGS} -DHINT_OP_DIV -o moptm_DIV_bench_o3avx2 moptm_bench_o3avx2.cpp && echo "  DIV O3AVX2 OK"

# 验证
echo "=== verify ==="
printf '1\n1234567890\n9876543210\n' | ./moptm_ADD_bench_o3avx2 2>&1
printf '1\n1234567890\n9876543210\n' | ./moptm_MUL_bench_o3avx2 2>&1
printf '1\n1234567890\n9876543210\n' | ./moptm_DIV_bench_o3avx2 2>&1

echo ""
echo "=== run chrono bench ==="

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
