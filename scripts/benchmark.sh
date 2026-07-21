#!/bin/bash
# benchmark.sh - LC 环境下 best vs hint 性能对比
# 50 次循环取总时间，计算平均 ms
cd /tmp/fusion

run_bench() {
    local name=$1
    local bin=$2
    local input=$3
    local iters=$4

    # warmup 2 次
    $bin < $input > /dev/null 2>&1
    $bin < $input > /dev/null 2>&1

    # 计时
    local start=$(date +%s%N)
    for i in $(seq 1 $iters); do
        $bin < $input > /dev/null 2>&1
    done
    local end=$(date +%s%N)
    local elapsed=$((end - start))
    local avg_ms=$(awk "BEGIN {printf \"%.2f\", $elapsed / $iters / 1000000}")
    echo "$name: avg ${avg_ms}ms (${iters} iters)"
}

echo "========================================"
echo "  ADD Benchmark (LC -O2, no avx2)"
echo "========================================"
echo ""
echo "--- 1M + 1M (20 iters) ---"
run_bench "best_ADD " ./best_ADD  test_add_1M.txt  20
run_bench "hint_ADD" ./hint_ADD  test_add_1M.txt  20
echo ""
echo "--- 100k + 100k (50 iters) ---"
run_bench "best_ADD " ./best_ADD  test_add_100k.txt 50
run_bench "hint_ADD" ./hint_ADD  test_add_100k.txt 50

echo ""
echo "========================================"
echo "  MUL Benchmark (LC -O2, no avx2)"
echo "========================================"
echo ""
echo "--- 500k * 500k (20 iters) ---"
run_bench "best_MUL " ./best_MUL  test_mul_500k.txt 20
run_bench "hint_MUL" ./hint_MUL  test_mul_500k.txt 20
echo ""
echo "--- 100k * 100k (50 iters) ---"
run_bench "best_MUL " ./best_MUL  test_mul_100k.txt 50
run_bench "hint_MUL" ./hint_MUL  test_mul_100k.txt 50

echo ""
echo "========================================"
echo "  DIV Benchmark (LC -O2, no avx2)"
echo "========================================"
echo ""
echo "--- 1M / 500k (10 iters) ---"
run_bench "hint_DIV" ./hint_DIV  test_div_1M_500k.txt 10
echo ""
echo "--- 200k / 100k (20 iters) ---"
run_bench "hint_DIV" ./hint_DIV  test_div_200k_100k.txt 20

echo ""
echo "========================================"
echo "  Correctness Check"
echo "========================================"
./best_ADD < test_add_100k.txt > /tmp/fusion/out_best_add.txt 2>&1
./hint_ADD < test_add_100k.txt > /tmp/fusion/out_hint_add.txt 2>&1
if diff -q /tmp/fusion/out_best_add.txt /tmp/fusion/out_hint_add.txt > /dev/null; then
    echo "ADD: PASS (outputs match)"
else
    echo "ADD: FAIL (outputs differ)"
    diff /tmp/fusion/out_best_add.txt /tmp/fusion/out_hint_add.txt | head -5
fi

./best_MUL < test_mul_100k.txt > /tmp/fusion/out_best_mul.txt 2>&1
./hint_MUL < test_mul_100k.txt > /tmp/fusion/out_hint_mul.txt 2>&1
if diff -q /tmp/fusion/out_best_mul.txt /tmp/fusion/out_hint_mul.txt > /dev/null; then
    echo "MUL: PASS (outputs match)"
else
    echo "MUL: FAIL (outputs differ)"
    diff /tmp/fusion/out_best_mul.txt /tmp/fusion/out_hint_mul.txt | head -5
fi

echo ""
echo "========================================"
echo "  Done"
echo "========================================"
