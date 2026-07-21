#!/bin/bash
# benchmark_history.sh - 测试各历史版本的 O2 vs O3 效果
cd /tmp/fusion

echo "=== Compiling historical versions ==="
echo ""

# v1 (masonxiong_opt 底座, DIV only)
g++ -x c++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -o v1_DIV_O2 moptm_v1.cpp 2>&1 | tail -2
echo "[1/6] v1_DIV_O2 done"
g++ -x c++ -O3 -std=gnu++20 -static -DONLINE_JUDGE -o v1_DIV_O3 moptm_v1.cpp 2>&1 | tail -2
echo "[2/6] v1_DIV_O3 done"

# v3 (hint 底座, 三模式)
g++ -x c++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_ADD -UHINT_OP_DIV -o v3_ADD_O2 moptm_v3.cpp 2>&1 | tail -2
echo "[3/6] v3_ADD_O2 done"
g++ -x c++ -O3 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_ADD -UHINT_OP_DIV -o v3_ADD_O3 moptm_v3.cpp 2>&1 | tail -2
echo "[4/6] v3_ADD_O3 done"
g++ -x c++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV -o v3_DIV_O2 moptm_v3.cpp 2>&1 | tail -2
echo "[5/6] v3_DIV_O2 done"
g++ -x c++ -O3 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV -o v3_DIV_O3 moptm_v3.cpp 2>&1 | tail -2
echo "[6/6] v3_DIV_O3 done"

# v5 (hint 底座, 三模式, 最新 git)
g++ -x c++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_ADD -UHINT_OP_DIV -o v5_ADD_O2 moptm_v5.cpp 2>&1 | tail -2
echo "[7/8] v5_ADD_O2 done"
g++ -x c++ -O3 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_ADD -UHINT_OP_DIV -o v5_ADD_O3 moptm_v5.cpp 2>&1 | tail -2
echo "[8/8] v5_ADD_O3 done"

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
echo "  v1 (masonxiong_opt) DIV 1M/500k (10 iters)"
echo "========================================"
run_bench "v1 -O2" ./v1_DIV_O2 test_div_1M_500k.txt 10
run_bench "v1 -O3" ./v1_DIV_O3 test_div_1M_500k.txt 10
run_bench "v5 -O2" ./moptm_DIV_O2 test_div_1M_500k.txt 10
run_bench "v5 -O3" ./moptm_DIV_O3 test_div_1M_500k.txt 10

echo ""
echo "========================================"
echo "  v1 (masonxiong_opt) DIV 200k/100k (20 iters)"
echo "========================================"
run_bench "v1 -O2" ./v1_DIV_O2 test_div_200k_100k.txt 20
run_bench "v1 -O3" ./v1_DIV_O3 test_div_200k_100k.txt 20
run_bench "v5 -O2" ./moptm_DIV_O2 test_div_200k_100k.txt 20
run_bench "v5 -O3" ./moptm_DIV_O3 test_div_200k_100k.txt 20

echo ""
echo "========================================"
echo "  v3 vs v5 ADD 1M+1M (20 iters)"
echo "========================================"
run_bench "v3 -O2" ./v3_ADD_O2 test_add_1M.txt 20
run_bench "v3 -O3" ./v3_ADD_O3 test_add_1M.txt 20
run_bench "v5 -O2" ./v5_ADD_O2 test_add_1M.txt 20
run_bench "v5 -O3" ./v5_ADD_O3 test_add_1M.txt 20
run_bench "ar -O2" ./moptm_ADD_O2 test_add_1M.txt 20

echo ""
echo "========================================"
echo "  v3 vs v5 DIV 1M/500k (10 iters)"
echo "========================================"
run_bench "v3 -O2" ./v3_DIV_O2 test_div_1M_500k.txt 10
run_bench "v3 -O3" ./v3_DIV_O3 test_div_1M_500k.txt 10
run_bench "v5 -O2" ./moptm_DIV_O2 test_div_1M_500k.txt 10
run_bench "v5 -O3" ./moptm_DIV_O3 test_div_1M_500k.txt 10

echo ""
echo "=== Correctness v1 vs v5 DIV ==="
./v1_DIV_O2 < test_div_200k_100k.txt > out_v1_div.txt 2>&1
./moptm_DIV_O2 < test_div_200k_100k.txt > out_v5_div.txt 2>&1
diff -q out_v1_div.txt out_v5_div.txt > /dev/null && echo "v1 vs v5 DIV: PASS" || echo "v1 vs v5 DIV: FAIL"

echo ""
echo "=== Done ==="
