#!/bin/bash
# run_linux_test.sh
# Linux 基准测试脚本：moptm vs best（对齐 LC 环境）
# 用法: bash run_linux_test.sh [源码目录]
# 默认源码目录: /mnt/hgfs/precious_speed

set -e

# 前置检查
if ! command -v /usr/bin/time > /dev/null 2>&1; then
    echo "Error: /usr/bin/time not found. Install: sudo apt install time"
    exit 1
fi

SRC_DIR=${1:-/mnt/hgfs/precious_speed}
WORK_DIR=/tmp/bench
mkdir -p ${WORK_DIR}
cd ${WORK_DIR}

echo "============================================"
echo "  Linux Benchmark: moptm vs best"
echo "  Date: $(date '+%Y-%m-%d %H:%M:%S')"
echo "============================================"

# 环境信息
echo ""
echo "=== Environment ==="
g++ --version | head -1
echo "CPU: $(grep 'model name' /proc/cpuinfo | head -1 | cut -d: -f2 | xargs)"
echo "AVX2: $(grep -o 'avx2' /proc/cpuinfo | head -1 || echo 'NO')"
echo "FMA:  $(grep -o 'fma' /proc/cpuinfo | head -1 || echo 'NO')"
echo "AVX512: $(grep -o 'avx512f' /proc/cpuinfo | head -1 || echo 'NO')"
echo "Mem: $(free -h | awk '/Mem:/{print $2}')"

# 复制源码和测试数据
echo ""
echo "=== Copying files ==="
cp ${SRC_DIR}/moptm.cpp .
cp ${SRC_DIR}/best/add.cpp best_add.cpp
cp ${SRC_DIR}/best/mul.cpp best_mul.cpp
cp ${SRC_DIR}/best/div.cpp best_div.cpp
cp ${SRC_DIR}/test_*.txt . 2>/dev/null || echo "Warning: no test_*.txt found"
echo "Done."

# 编译（对齐 LC 编译选项）
echo ""
echo "=== Compiling (LC flags: -O2 -std=gnu++20 -static -DONLINE_JUDGE) ==="
FLAGS="-O2 -std=gnu++20 -static -DONLINE_JUDGE"
g++ ${FLAGS} -DHINT_OP_ADD -o moptm_ADD moptm.cpp && echo "  moptm_ADD OK"
g++ ${FLAGS} -DHINT_OP_MUL -o moptm_MUL moptm.cpp && echo "  moptm_MUL OK"
g++ ${FLAGS} -DHINT_OP_DIV -o moptm_DIV moptm.cpp && echo "  moptm_DIV OK"
g++ ${FLAGS} -o best_ADD best_add.cpp && echo "  best_ADD OK"
g++ ${FLAGS} -o best_MUL best_mul.cpp && echo "  best_MUL OK"
g++ ${FLAGS} -o best_DIV best_div.cpp && echo "  best_DIV OK"

# 正确性验证
echo ""
echo "=== Correctness (byte-by-byte) ==="
verify() {
    local name=$1 exe_m=$2 exe_b=$3 test=$4
    ./${exe_m} < ${test} > out_m.txt 2>/dev/null
    ./${exe_b} < ${test} > out_b.txt 2>/dev/null
    if diff -q out_m.txt out_b.txt > /dev/null 2>&1; then
        echo "  ${name}: PASS"
    else
        echo "  ${name}: FAIL"
        diff out_m.txt out_b.txt | head -5
    fi
}
verify "ADD_1M"   moptm_ADD best_ADD test_add_1M_1M.txt
verify "MUL_100k" moptm_MUL best_MUL test_mul_100k_100k.txt
verify "DIV_1M_100k" moptm_DIV best_DIV test_div_1M_100k.txt
verify "DIV_1M_500k" moptm_DIV best_DIV test_div_1M_500k.txt

# 基准测试函数：返回 CPU 时间 (user+sys 秒)
bench_cpu() {
    local exe=$1 test_file=$2 runs=${3:-5}
    ./${exe} < ${test_file} > /dev/null 2>&1  # warmup
    local total=0
    for i in $(seq 1 ${runs}); do
        local t=$(/usr/bin/time -f "%U %S" ./${exe} < ${test_file} 2>&1 > /dev/null)
        total=$(echo ${t} | awk -v tot=${total} '{print tot + $1 + $2}')
    done
    echo ${total} ${runs} | awk '{printf "%.4f", $1 / $2}'
}

# 性能基准
echo ""
echo "=== Performance (CPU time, 5 runs avg) ==="

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
for test in test_div_100k_10k.txt test_div_1M_100k.txt test_div_1M_500k.txt test_div_1M_900k.txt; do
    [ -f ${test} ] || continue
    m=$(bench_cpu moptm_DIV ${test})
    b=$(bench_cpu best_DIV ${test})
    r=$(echo ${m} ${b} | awk '{printf "%.2f", $1 / $2}')
    echo "  ${test}: moptm=${m}s best=${b}s ratio=${r}x"
done

# BSS 影响测试：不同 oBuffer 大小对 DIV 启动时间的影响
echo ""
echo "=== BSS Impact Test (DIV startup time) ==="
# 用最小输入测进程启动开销
echo "0 1" > tiny_div.txt

for size in 8 16 32; do
    sed "s/oBuffer\[32 << 20\]/oBuffer[${size} << 20]/" moptm.cpp > moptm_div_${size}mb.cpp
    g++ ${FLAGS} -DHINT_OP_DIV -o moptm_div_${size}mb moptm_div_${size}mb.cpp
    t=$(bench_cpu moptm_div_${size}mb tiny_div.txt 10)
    echo "  oBuffer=${size}MB: ${t}s (avg 10 runs, tiny input)"
done

echo ""
echo "============================================"
echo "  Benchmark Complete"
echo "============================================"
