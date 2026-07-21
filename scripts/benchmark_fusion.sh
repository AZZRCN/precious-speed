#!/bin/bash
# benchmark_fusion.sh - fusion.cpp v0 三模式性能测试
cd /tmp/fusion

echo "=== Compiling fusion.cpp v0 (3 modes) ==="
g++ -x c++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_ADD -o fusion_ADD fusion.cpp 2>&1 | tail -3
echo "[1/3] fusion_ADD done"
g++ -x c++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_MUL -o fusion_MUL fusion.cpp 2>&1 | tail -3
echo "[2/3] fusion_MUL done"
g++ -x c++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV -o fusion_DIV fusion.cpp 2>&1 | tail -3
echo "[3/3] fusion_DIV done"

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

echo ""
echo "========================================"
echo "  ADD 1M+1M (20 iters)"
echo "========================================"
run_bench "fusion v0 " ./fusion_ADD test_add_1M.txt 20
run_bench "best      " ./best_ADD    test_add_1M.txt 20
run_bench "ar-O2     " ./moptm_ADD_O2 test_add_1M.txt 20
run_bench "ar-pragma " ./moptm_ADD_PRG test_add_1M.txt 20

echo ""
echo "========================================"
echo "  ADD 100k+100k (50 iters)"
echo "========================================"
run_bench "fusion v0 " ./fusion_ADD test_add_100k.txt 50
run_bench "best      " ./best_ADD    test_add_100k.txt 50

echo ""
echo "========================================"
echo "  MUL 500k*500k (20 iters)"
echo "========================================"
run_bench "fusion v0 " ./fusion_MUL test_mul_500k.txt 20
run_bench "best      " ./best_MUL    test_mul_500k.txt 20

echo ""
echo "========================================"
echo "  MUL 100k*100k (50 iters)"
echo "========================================"
run_bench "fusion v0 " ./fusion_MUL test_mul_100k.txt 50
run_bench "best      " ./best_MUL    test_mul_100k.txt 50

echo ""
echo "========================================"
echo "  DIV 1M/500k (10 iters)"
echo "========================================"
run_bench "fusion v0 " ./fusion_DIV test_div_1M_500k.txt 10

echo ""
echo "========================================"
echo "  DIV 200k/100k (20 iters)"
echo "========================================"
run_bench "fusion v0 " ./fusion_DIV test_div_200k_100k.txt 20

echo ""
echo "========================================"
echo "  Correctness (fusion vs best)"
echo "========================================"
./fusion_ADD < test_add_100k.txt > out_fusion_add.txt 2>&1
./best_ADD    < test_add_100k.txt > out_best_add.txt 2>&1
diff -q out_fusion_add.txt out_best_add.txt > /dev/null && echo "ADD: PASS" || echo "ADD: FAIL"

./fusion_MUL < test_mul_100k.txt > out_fusion_mul.txt 2>&1
./best_MUL    < test_mul_100k.txt > out_best_mul.txt 2>&1
diff -q out_fusion_mul.txt out_best_mul.txt > /dev/null && echo "MUL: PASS" || echo "MUL: FAIL"

./fusion_DIV < test_div_200k_100k.txt > out_fusion_div.txt 2>&1
diff -q out_fusion_div.txt out_v5_div.txt > /dev/null && echo "DIV: PASS" || echo "DIV: FAIL"

echo ""
echo "=== Done ==="
