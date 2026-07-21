#!/bin/bash
# run_linux_test2.sh
# 修复 bench_cpu 重定向 bug + 增加 pragma 配置对比
# 用法: bash run_linux_test2.sh [源码目录]

set -e

SRC_DIR=${1:-/mnt/hgfs/precious_speed}
WORK_DIR=/tmp/bench2
mkdir -p ${WORK_DIR}
cd ${WORK_DIR}

echo "============================================"
echo "  Linux Benchmark v2: moptm pragma 对比"
echo "  Date: $(date '+%Y-%m-%d %H:%M:%S')"
echo "============================================"

echo ""
echo "=== Environment ==="
g++ --version | head -1
echo "CPU: $(grep 'model name' /proc/cpuinfo | head -1 | cut -d: -f2 | xargs)"
echo "AVX2: $(grep -o 'avx2' /proc/cpuinfo | head -1 || echo 'NO')"
echo "FMA:  $(grep -o 'fma' /proc/cpuinfo | head -1 || echo 'NO')"
echo "AVX512: $(grep -o 'avx512f' /proc/cpuinfo | head -1 || echo 'NO')"

# 复制源码
echo ""
echo "=== Copying files ==="
cp ${SRC_DIR}/moptm.cpp .
cp ${SRC_DIR}/best/add.cpp best_add.cpp
cp ${SRC_DIR}/best/mul.cpp best_mul.cpp
cp ${SRC_DIR}/best/div.cpp best_div.cpp
cp ${SRC_DIR}/test_*.txt . 2>/dev/null || echo "Warning: no test_*.txt found"
echo "Done."

# bench_cpu 函数：修复重定向 bug
# 程序 stdout → /dev/null，time stderr → 被捕获
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

LC_FLAGS="-O2 -std=gnu++20 -static -DONLINE_JUDGE"

# ============================================
# Part 1: 正确性验证 (moptm -O2 vs best -O2)
# ============================================
echo ""
echo "=== Part 1: Compile & Correctness (LC -O2) ==="
g++ ${LC_FLAGS} -DHINT_OP_ADD -o moptm_ADD moptm.cpp && echo "  moptm_ADD OK"
g++ ${LC_FLAGS} -DHINT_OP_MUL -o moptm_MUL moptm.cpp && echo "  moptm_MUL OK"
g++ ${LC_FLAGS} -DHINT_OP_DIV -o moptm_DIV moptm.cpp && echo "  moptm_DIV OK"
g++ ${LC_FLAGS} -o best_ADD best_add.cpp && echo "  best_ADD OK"
g++ ${LC_FLAGS} -o best_MUL best_mul.cpp && echo "  best_MUL OK"
g++ ${LC_FLAGS} -o best_DIV best_div.cpp && echo "  best_DIV OK"

verify() {
    local name=$1 exe_m=$2 exe_b=$3 test=$4
    ./${exe_m} < ${test} > out_m.txt 2>/dev/null
    ./${exe_b} < ${test} > out_b.txt 2>/dev/null
    if diff -q out_m.txt out_b.txt > /dev/null 2>&1; then
        echo "  ${name}: PASS"
    else
        echo "  ${name}: FAIL"
        diff out_m.txt out_b.txt | head -3
    fi
}
verify "ADD_1M"   moptm_ADD best_ADD test_add_1M_1M.txt
verify "MUL_100k" moptm_MUL best_MUL test_mul_100k_100k.txt
verify "DIV_1M_100k" moptm_DIV best_DIV test_div_1M_100k.txt
verify "DIV_1M_500k" moptm_DIV best_DIV test_div_1M_500k.txt

# ============================================
# Part 2: moptm vs best 性能对比 (纯 -O2)
# ============================================
echo ""
echo "=== Part 2: Performance moptm vs best (LC -O2) ==="
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

# ============================================
# Part 3: Pragma 配置对比 (moptm 在 4 种配置下)
# ============================================
echo ""
echo "=== Part 3: Pragma 配置对比 (moptm DIV) ==="
echo "对比 4 种配置在 DIV 1M_500k 上的性能:"

# 创建 4 个版本的 moptm
# 配置 A: 纯 -O2 (已编译为 moptm_DIV)
echo "  [A] -O2 (LC default) — 已编译为 moptm_DIV"

# 配置 B: -O2 + #pragma GCC target("avx2,fma")
sed '1i\#pragma GCC target("avx2,fma")' moptm.cpp > moptm_avx2.cpp
g++ ${LC_FLAGS} -DHINT_OP_DIV -o moptm_DIV_avx2 moptm_avx2.cpp && echo "  [B] -O2 + target(avx2,fma) OK"

# 配置 C: -O2 + #pragma GCC optimize("O3,unroll-loops")
sed '1i\#pragma GCC optimize("O3,unroll-loops")' moptm.cpp > moptm_o3.cpp
g++ ${LC_FLAGS} -DHINT_OP_DIV -o moptm_DIV_o3 moptm_o3.cpp && echo "  [C] -O2 + optimize(O3,unroll-loops) OK"

# 配置 D: -O2 + #pragma GCC optimize("O3,unroll-loops") + target("avx2,fma")
sed '1i\#pragma GCC target("avx2,fma")\n#pragma GCC optimize("O3,unroll-loops")' moptm.cpp > moptm_o3avx2.cpp
g++ ${LC_FLAGS} -DHINT_OP_DIV -o moptm_DIV_o3avx2 moptm_o3avx2.cpp && echo "  [D] -O2 + optimize(O3) + target(avx2,fma) OK"

echo ""
echo "  DIV 1M_500k 结果:"
a=$(bench_cpu moptm_DIV test_div_1M_500k.txt)
b=$(bench_cpu moptm_DIV_avx2 test_div_1M_500k.txt)
c=$(bench_cpu moptm_DIV_o3 test_div_1M_500k.txt)
d=$(bench_cpu moptm_DIV_o3avx2 test_div_1M_500k.txt)
echo "  [A] -O2:                ${a}s"
echo "  [B] -O2+target(avx2):   ${b}s"
echo "  [C] -O2+opt(O3):        ${c}s"
echo "  [D] -O2+opt(O3)+avx2:   ${d}s"

echo ""
echo "  DIV 1M_100k 结果:"
a=$(bench_cpu moptm_DIV test_div_1M_100k.txt)
b=$(bench_cpu moptm_DIV_avx2 test_div_1M_100k.txt)
c=$(bench_cpu moptm_DIV_o3 test_div_1M_100k.txt)
d=$(bench_cpu moptm_DIV_o3avx2 test_div_1M_100k.txt)
echo "  [A] -O2:                ${a}s"
echo "  [B] -O2+target(avx2):   ${b}s"
echo "  [C] -O2+opt(O3):        ${c}s"
echo "  [D] -O2+opt(O3)+avx2:   ${d}s"

echo ""
echo "  MUL 500k_500k 结果:"
g++ ${LC_FLAGS} -DHINT_OP_MUL -o moptm_MUL_avx2 moptm_avx2.cpp
g++ ${LC_FLAGS} -DHINT_OP_MUL -o moptm_MUL_o3 moptm_o3.cpp
g++ ${LC_FLAGS} -DHINT_OP_MUL -o moptm_MUL_o3avx2 moptm_o3avx2.cpp
a=$(bench_cpu moptm_MUL test_mul_500k_500k.txt)
b=$(bench_cpu moptm_MUL_avx2 test_mul_500k_500k.txt)
c=$(bench_cpu moptm_MUL_o3 test_mul_500k_500k.txt)
d=$(bench_cpu moptm_MUL_o3avx2 test_mul_500k_500k.txt)
echo "  [A] -O2:                ${a}s"
echo "  [B] -O2+target(avx2):   ${b}s"
echo "  [C] -O2+opt(O3):        ${c}s"
echo "  [D] -O2+opt(O3)+avx2:   ${d}s"

echo ""
echo "  ADD 1M_1M 结果:"
g++ ${LC_FLAGS} -DHINT_OP_ADD -o moptm_ADD_avx2 moptm_avx2.cpp
g++ ${LC_FLAGS} -DHINT_OP_ADD -o moptm_ADD_o3 moptm_o3.cpp
g++ ${LC_FLAGS} -DHINT_OP_ADD -o moptm_ADD_o3avx2 moptm_o3avx2.cpp
a=$(bench_cpu moptm_ADD test_add_1M_1M.txt)
b=$(bench_cpu moptm_ADD_avx2 test_add_1M_1M.txt)
c=$(bench_cpu moptm_ADD_o3 test_add_1M_1M.txt)
d=$(bench_cpu moptm_ADD_o3avx2 test_add_1M_1M.txt)
echo "  [A] -O2:                ${a}s"
echo "  [B] -O2+target(avx2):   ${b}s"
echo "  [C] -O2+opt(O3):        ${c}s"
echo "  [D] -O2+opt(O3)+avx2:   ${d}s"

echo ""
echo "============================================"
echo "  Benchmark Complete"
echo "============================================"
