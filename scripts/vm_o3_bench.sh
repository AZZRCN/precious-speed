#!/bin/bash
# VM 基准测试：g++ 15.2 vs g++ 11 (模拟 LC)
# 5 版本 × 2 编译器 = 10 个二进制
set -e
cd /tmp/o3_bench

RUNS=7
INPUT="123 456"
echo "$INPUT" > input.txt

echo "=== VM Benchmark: g++ 15.2 vs g++ 11 ==="
echo "Input: '$INPUT', runs: $RUNS"
echo ""

# 编译函数
compile_all() {
    local CC=$1
    local PREFIX=$2
    echo "--- Compiling with $CC ($PREFIX) ---"

    # O2 baseline
    $CC -x c++ -O2 -std=gnu++20 -static -DONLINE_JUDGE o3_verify.cpp -o ${PREFIX}_O2 2>&1 | tail -1
    echo "  [1/5] ${PREFIX}_O2 done"

    # O2 -fno-tree-loop-vectorize (模拟 GCC11 的 -O2 行为)
    $CC -x c++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -fno-tree-loop-vectorize -fno-tree-slp-vectorize o3_verify.cpp -o ${PREFIX}_O2NOVEC 2>&1 | tail -1
    echo "  [2/5] ${PREFIX}_O2NOVEC done"

    # O2 + pragma O3,unroll-loops
    $CC -x c++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DENABLE_O3_MODE o3_verify.cpp -o ${PREFIX}_PRG 2>&1 | tail -1
    echo "  [3/5] ${PREFIX}_PRG done"

    # O3 cmdline
    $CC -x c++ -O3 -std=gnu++20 -static -DONLINE_JUDGE o3_verify.cpp -o ${PREFIX}_O3 2>&1 | tail -1
    echo "  [4/5] ${PREFIX}_O3 done"

    # O2-novec + pragma (测试 pragma 是否覆盖显式 -fno-)
    $CC -x c++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -fno-tree-loop-vectorize -fno-tree-slp-vectorize -DENABLE_O3_MODE o3_verify.cpp -o ${PREFIX}_NOVEC_PRG 2>&1 | tail -1
    echo "  [5/5] ${PREFIX}_NOVEC_PRG done"
    echo ""
}

# 基准函数
run_bench() {
    local name=$1
    local bin=$2
    # 预热
    ./$bin < input.txt > /dev/null 2>&1
    # 计时
    local times=""
    for i in $(seq 1 $RUNS); do
        local start=$(date +%s%N)
        ./$bin < input.txt > /dev/null 2>&1
        local end=$(date +%s%N)
        local ms=$(awk "BEGIN {printf \"%.2f\", ($end - $start) / 1000000}")
        times="$times $ms"
    done
    # 计算统计
    local min=$(echo $times | tr ' ' '\n' | sort -n | head -1)
    local max=$(echo $times | tr ' ' '\n' | sort -n | tail -1)
    local avg=$(echo $times | tr ' ' '\n' | awk "{sum+=\$1; n++} END {printf \"%.2f\", sum/n}")
    printf "  %-30s min=%7s  avg=%7s  max=%7s  ms  [raw:%s]\n" "$name" "$min" "$avg" "$max" "$times"
}

# 正确性检查
check_correctness() {
    local CC=$1
    local PREFIX=$2
    echo "--- Correctness ($PREFIX) ---"
    for ver in O2 O2NOVEC PRG O3 NOVEC_PRG; do
        local out=$(./${PREFIX}_${ver} < input.txt 2>&1)
        if [ "$out" = "579" ]; then
            echo "  ${PREFIX}_${ver}: PASS (output=579)"
        else
            echo "  ${PREFIX}_${ver}: FAIL (output=$out)"
        fi
    done
    echo ""
}

# 检查汇编向量化
check_asm() {
    local CC=$1
    local PREFIX=$2
    echo "--- Assembly vectorization ($PREFIX) ---"
    $CC -x c++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -S -o ${PREFIX}_asm_O2.s o3_verify.cpp 2>/dev/null
    $CC -x c++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DENABLE_O3_MODE -S -o ${PREFIX}_asm_PRG.s o3_verify.cpp 2>/dev/null
    $CC -x c++ -O3 -std=gnu++20 -static -DONLINE_JUDGE -S -o ${PREFIX}_asm_O3.s o3_verify.cpp 2>/dev/null
    $CC -x c++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -fno-tree-loop-vectorize -fno-tree-slp-vectorize -S -o ${PREFIX}_asm_O2NOVEC.s o3_verify.cpp 2>/dev/null
    $CC -x c++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -fno-tree-loop-vectorize -fno-tree-slp-vectorize -DENABLE_O3_MODE -S -o ${PREFIX}_asm_NOVEC_PRG.s o3_verify.cpp 2>/dev/null

    for ver in O2 O2NOVEC PRG O3 NOVEC_PRG; do
        local packed=$(grep -cE '(mulps|vmulps|addps|vaddps)' ${PREFIX}_asm_${ver}.s || echo 0)
        local scalar=$(grep -cE '(mulss|vmulss|addss|vaddss)' ${PREFIX}_asm_${ver}.s || echo 0)
        printf "  %-12s packed=%3s  scalar=%3s\n" "${ver}" "$packed" "$scalar"
    done
    echo ""
}

# === 主流程 ===

# 编译
compile_all "g++" "gcc15"
compile_all "g++-11" "gcc11"

# 正确性
check_correctness "g++" "gcc15"
check_correctness "g++-11" "gcc11"

# 汇编向量化检查
check_asm "g++" "gcc15"
check_asm "g++-11" "gcc11"

# 基准测试
echo "========================================"
echo "  g++ 15.2.0 Benchmark"
echo "========================================"
run_bench "O2 (default)         " gcc15_O2
run_bench "O2-novec (sim GCC11) " gcc15_O2NOVEC
run_bench "O2+pragma O3,unroll  " gcc15_PRG
run_bench "O3 (cmdline)         " gcc15_O3
run_bench "O2-novec+pragma      " gcc15_NOVEC_PRG

echo ""
echo "========================================"
echo "  g++ 11.5.0 Benchmark (LC simulation)"
echo "========================================"
run_bench "O2 (LC default)      " gcc11_O2
run_bench "O2-novec             " gcc11_O2NOVEC
run_bench "O2+pragma O3,unroll  " gcc11_PRG
run_bench "O3 (cmdline)         " gcc11_O3
run_bench "O2-novec+pragma      " gcc11_NOVEC_PRG

echo ""
echo "=== Done ==="
