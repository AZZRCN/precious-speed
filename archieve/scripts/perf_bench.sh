#!/bin/bash
# perf_bench.sh
# 性能基准测试（数据已修正为 LF 格式）
cd /tmp/bench2

bench_cpu() {
    local exe=$1 test_file=$2 runs=${3:-5}
    ./${exe} < ${test_file} > /dev/null 2>&1  # warmup
    local total=0
    for i in $(seq 1 ${runs}); do
        local t=$( { /usr/bin/time -f "%U %S" ./${exe} < ${test_file} > /dev/null; } 2>&1 )
        total=$(echo ${t} | awk -v tot=${total} '{print tot + $1 + $2}')
    done
    echo ${total} ${runs} | awk '{printf "%.4f", $1 / $2}'
}

echo "============================================"
echo "  Performance: moptm vs best (LC -O2)"
echo "  Date: $(date '+%Y-%m-%d %H:%M:%S')"
echo "============================================"

echo ""
echo "--- ADD ---"
for test in test_add_100k_100k.txt test_add_500k_500k.txt test_add_1M_1M.txt; do
    [ -f ${test} ] || continue
    m=$(bench_cpu moptm_ADD ${test})
    b=$(bench_cpu best_ADD ${test})
    r=$(echo ${m} ${b} | awk '{printf "%.2f", $1 / $2}')
    echo "  ${test}: moptm=${m}s best=${b}s ratio=${r}x"
done

echo "--- MUL ---"
for test in test_mul_100k_100k.txt test_mul_300k_300k.txt test_mul_500k_500k.txt; do
    [ -f ${test} ] || continue
    m=$(bench_cpu moptm_MUL ${test})
    b=$(bench_cpu best_MUL ${test})
    r=$(echo ${m} ${b} | awk '{printf "%.2f", $1 / $2}')
    echo "  ${test}: moptm=${m}s best=${b}s ratio=${r}x"
done

echo "--- DIV ---"
for test in test_div_100k_10k.txt test_div_1M_100k.txt test_div_1M_500k.txt test_div_1M_900k.txt test_div_1M_999k.txt; do
    [ -f ${test} ] || continue
    m=$(bench_cpu moptm_DIV ${test})
    b=$(bench_cpu best_DIV ${test})
    r=$(echo ${m} ${b} | awk '{printf "%.2f", $1 / $2}')
    echo "  ${test}: moptm=${m}s best=${b}s ratio=${r}x"
done

echo ""
echo "============================================"
echo "  Pragma 对比 (moptm 4 种配置)"
echo "============================================"

# 确保 Part 3 的可执行文件已编译
LC_FLAGS="-O2 -std=gnu++20 -static -DONLINE_JUDGE"
[ -f moptm_DIV_avx2 ] || g++ ${LC_FLAGS} -DHINT_OP_DIV -o moptm_DIV_avx2 moptm_avx2.cpp
[ -f moptm_DIV_o3 ] || g++ ${LC_FLAGS} -DHINT_OP_DIV -o moptm_DIV_o3 moptm_o3.cpp
[ -f moptm_DIV_o3avx2 ] || g++ ${LC_FLAGS} -DHINT_OP_DIV -o moptm_DIV_o3avx2 moptm_o3avx2.cpp
[ -f moptm_MUL_avx2 ] || g++ ${LC_FLAGS} -DHINT_OP_MUL -o moptm_MUL_avx2 moptm_avx2.cpp
[ -f moptm_MUL_o3 ] || g++ ${LC_FLAGS} -DHINT_OP_MUL -o moptm_MUL_o3 moptm_o3.cpp
[ -f moptm_MUL_o3avx2 ] || g++ ${LC_FLAGS} -DHINT_OP_MUL -o moptm_MUL_o3avx2 moptm_o3avx2.cpp
[ -f moptm_ADD_avx2 ] || g++ ${LC_FLAGS} -DHINT_OP_ADD -o moptm_ADD_avx2 moptm_avx2.cpp
[ -f moptm_ADD_o3 ] || g++ ${LC_FLAGS} -DHINT_OP_ADD -o moptm_ADD_o3 moptm_o3.cpp
[ -f moptm_ADD_o3avx2 ] || g++ ${LC_FLAGS} -DHINT_OP_ADD -o moptm_ADD_o3avx2 moptm_o3avx2.cpp

echo ""
echo "  DIV 1M_500k:"
a=$(bench_cpu moptm_DIV test_div_1M_500k.txt)
b=$(bench_cpu moptm_DIV_avx2 test_div_1M_500k.txt)
c=$(bench_cpu moptm_DIV_o3 test_div_1M_500k.txt)
d=$(bench_cpu moptm_DIV_o3avx2 test_div_1M_500k.txt)
echo "  [A] -O2:              ${a}s"
echo "  [B] -O2+target(avx2): ${b}s"
echo "  [C] -O2+opt(O3):      ${c}s"
echo "  [D] -O2+opt(O3)+avx2: ${d}s"

echo ""
echo "  DIV 1M_100k:"
a=$(bench_cpu moptm_DIV test_div_1M_100k.txt)
b=$(bench_cpu moptm_DIV_avx2 test_div_1M_100k.txt)
c=$(bench_cpu moptm_DIV_o3 test_div_1M_100k.txt)
d=$(bench_cpu moptm_DIV_o3avx2 test_div_1M_100k.txt)
echo "  [A] -O2:              ${a}s"
echo "  [B] -O2+target(avx2): ${b}s"
echo "  [C] -O2+opt(O3):      ${c}s"
echo "  [D] -O2+opt(O3)+avx2: ${d}s"

echo ""
echo "  MUL 500k_500k:"
a=$(bench_cpu moptm_MUL test_mul_500k_500k.txt)
b=$(bench_cpu moptm_MUL_avx2 test_mul_500k_500k.txt)
c=$(bench_cpu moptm_MUL_o3 test_mul_500k_500k.txt)
d=$(bench_cpu moptm_MUL_o3avx2 test_mul_500k_500k.txt)
echo "  [A] -O2:              ${a}s"
echo "  [B] -O2+target(avx2): ${b}s"
echo "  [C] -O2+opt(O3):      ${c}s"
echo "  [D] -O2+opt(O3)+avx2: ${d}s"

echo ""
echo "  ADD 1M_1M:"
a=$(bench_cpu moptm_ADD test_add_1M_1M.txt)
b=$(bench_cpu moptm_ADD_avx2 test_add_1M_1M.txt)
c=$(bench_cpu moptm_ADD_o3 test_add_1M_1M.txt)
d=$(bench_cpu moptm_ADD_o3avx2 test_add_1M_1M.txt)
echo "  [A] -O2:              ${a}s"
echo "  [B] -O2+target(avx2): ${b}s"
echo "  [C] -O2+opt(O3):      ${c}s"
echo "  [D] -O2+opt(O3)+avx2: ${d}s"

echo ""
echo "============================================"
echo "  Done"
echo "============================================"
