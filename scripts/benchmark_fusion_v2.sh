#!/bin/bash
# benchmark_fusion_v2.sh - 查表法 parse 优化效果
cd /tmp/fusion

run_bench() {
    local name=$1
    local bin=$2
    local input=$3
    local iters=$4
    $bin < $input > /dev/null 2>&1
    $bin < $input > /dev/null 2>&1
    local start=$(date +%s%N)
    for i in $(seq 1 $iters); do
        $bin < $input > /dev/null 2>&1
    done
    local end=$(date +%s%N)
    local elapsed=$((end - start))
    local avg_ms=$(awk "BEGIN {printf \"%.2f\", $elapsed / $iters / 1000000}")
    echo "  $name : ${avg_ms}ms"
}

echo "========================================"
echo "  ADD 1M+1M (20 iters) - 查表法效果"
echo "========================================"
run_bench "fusion v2 (查表)" ./fusion_v2_ADD test_add_1M.txt 20
run_bench "fusion v1 (乘加)" ./fusion_v1_ADD test_add_1M.txt 20
run_bench "best             " ./best_ADD       test_add_1M.txt 20

echo ""
echo "========================================"
echo "  ADD 100k+100k (50 iters)"
echo "========================================"
run_bench "fusion v2 (查表)" ./fusion_v2_ADD test_add_100k.txt 50
run_bench "fusion v1 (乘加)" ./fusion_v1_ADD test_add_100k.txt 50
run_bench "best             " ./best_ADD       test_add_100k.txt 50

echo ""
echo "========================================"
echo "  MUL 500k*500k (20 iters)"
echo "========================================"
run_bench "fusion v2 (查表)" ./fusion_v2_MUL test_mul_500k.txt 20
run_bench "fusion v1 (乘加)" ./fusion_v1_MUL test_mul_500k.txt 20
run_bench "best             " ./best_MUL       test_mul_500k.txt 20

echo ""
echo "========================================"
echo "  MUL 100k*100k (50 iters)"
echo "========================================"
run_bench "fusion v2 (查表)" ./fusion_v2_MUL test_mul_100k.txt 50
run_bench "fusion v1 (乘加)" ./fusion_v1_MUL test_mul_100k.txt 50
run_bench "best             " ./best_MUL       test_mul_100k.txt 50

echo ""
echo "========================================"
echo "  DIV 1M/500k (10 iters)"
echo "========================================"
run_bench "fusion v2 (查表)" ./fusion_v2_DIV test_div_1M_500k.txt 10
run_bench "fusion v1 (乘加)" ./fusion_v1_DIV test_div_1M_500k.txt 10

echo ""
echo "========================================"
echo "  DIV 200k/100k (20 iters)"
echo "========================================"
run_bench "fusion v2 (查表)" ./fusion_v2_DIV test_div_200k_100k.txt 20
run_bench "fusion v1 (乘加)" ./fusion_v1_DIV test_div_200k_100k.txt 20

echo ""
echo "========================================"
echo "  Correctness (v2 vs v1)"
echo "========================================"
./fusion_v2_ADD < test_add_100k.txt > out_v2_add.txt 2>&1
./fusion_v1_ADD < test_add_100k.txt > out_v1_add.txt 2>&1
diff -q out_v2_add.txt out_v1_add.txt > /dev/null && echo "ADD: PASS" || echo "ADD: FAIL"

./fusion_v2_MUL < test_mul_100k.txt > out_v2_mul.txt 2>&1
./fusion_v1_MUL < test_mul_100k.txt > out_v1_mul.txt 2>&1
diff -q out_v2_mul.txt out_v1_mul.txt > /dev/null && echo "MUL: PASS" || echo "MUL: FAIL"

./fusion_v2_DIV < test_div_200k_100k.txt > out_v2_div.txt 2>&1
./fusion_v1_DIV < test_div_200k_100k.txt > out_v1_div.txt 2>&1
diff -q out_v2_div.txt out_v1_div.txt > /dev/null && echo "DIV: PASS" || echo "DIV: FAIL"

echo ""
echo "=== Done ==="
