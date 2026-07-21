#!/bin/bash
# benchmark_v5_pragma.sh - 验证 v5 git 版本 + pragma O3 效果
cd /tmp/fusion

# 创建 v5 pragma 版本
echo '#pragma GCC optimize("O3,unroll-loops")' > moptm_v5_pragma.cpp
cat moptm_v5.cpp >> moptm_v5_pragma.cpp

echo "=== Compiling v5 pragma versions ==="
g++ -x c++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_ADD -UHINT_OP_DIV -o v5_ADD_PRG moptm_v5_pragma.cpp 2>&1 | tail -2
echo "[1/3] v5_ADD_PRG done"
g++ -x c++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_MUL -UHINT_OP_DIV -o v5_MUL_PRG moptm_v5_pragma.cpp 2>&1 | tail -2
echo "[2/3] v5_MUL_PRG done"
g++ -x c++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV -o v5_DIV_PRG moptm_v5_pragma.cpp 2>&1 | tail -2
echo "[3/3] v5_DIV_PRG done"

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
echo "  ADD 1M+1M (20 iters) - v5 O2 vs O3 vs O2+pragma"
echo "========================================"
run_bench "v5 -O2       " ./v5_ADD_O2  test_add_1M.txt 20
run_bench "v5 -O3       " ./v5_ADD_O3  test_add_1M.txt 20
run_bench "v5 -O2+pragma" ./v5_ADD_PRG test_add_1M.txt 20
run_bench "ar -O2       " ./moptm_ADD_O2 test_add_1M.txt 20
run_bench "ar -O2+pragma" ./moptm_ADD_PRG test_add_1M.txt 20
run_bench "best -O2     " ./best_ADD   test_add_1M.txt 20

echo ""
echo "========================================"
echo "  ADD 100k+100k (50 iters)"
echo "========================================"
run_bench "v5 -O2       " ./v5_ADD_O2  test_add_100k.txt 50
run_bench "v5 -O3       " ./v5_ADD_O3  test_add_100k.txt 50
run_bench "v5 -O2+pragma" ./v5_ADD_PRG test_add_100k.txt 50
run_bench "ar -O2       " ./moptm_ADD_O2 test_add_100k.txt 50
run_bench "ar -O2+pragma" ./moptm_ADD_PRG test_add_100k.txt 50
run_bench "best -O2     " ./best_ADD   test_add_100k.txt 50

echo ""
echo "========================================"
echo "  MUL 500k*500k (20 iters)"
echo "========================================"
run_bench "v5 -O2       " ./v5_MUL_PRG test_mul_500k.txt 20
# 需要编译 v5 MUL O2 和 O3
g++ -x c++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_MUL -UHINT_OP_DIV -o v5_MUL_O2 moptm_v5.cpp 2>&1 | tail -1
g++ -x c++ -O3 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_MUL -UHINT_OP_DIV -o v5_MUL_O3 moptm_v5.cpp 2>&1 | tail -1
run_bench "v5 -O2       " ./v5_MUL_O2  test_mul_500k.txt 20
run_bench "v5 -O3       " ./v5_MUL_O3  test_mul_500k.txt 20
run_bench "v5 -O2+pragma" ./v5_MUL_PRG test_mul_500k.txt 20
run_bench "ar -O2       " ./moptm_MUL_O2 test_mul_500k.txt 20
run_bench "best -O2     " ./best_MUL   test_mul_500k.txt 20

echo ""
echo "========================================"
echo "  DIV 1M/500k (10 iters)"
echo "========================================"
g++ -x c++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV -o v5_DIV_O2 moptm_v5.cpp 2>&1 | tail -1
g++ -x c++ -O3 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV -o v5_DIV_O3 moptm_v5.cpp 2>&1 | tail -1
run_bench "v5 -O2       " ./v5_DIV_O2  test_div_1M_500k.txt 10
run_bench "v5 -O3       " ./v5_DIV_O3  test_div_1M_500k.txt 10
run_bench "v5 -O2+pragma" ./v5_DIV_PRG test_div_1M_500k.txt 10
run_bench "ar -O2       " ./moptm_DIV_O2 test_div_1M_500k.txt 10
run_bench "ar -O2+pragma" ./moptm_DIV_PRG test_div_1M_500k.txt 10

echo ""
echo "=== Correctness ==="
./v5_ADD_PRG < test_add_100k.txt > out_v5prg_add.txt 2>&1
./v5_ADD_O2  < test_add_100k.txt > out_v5o2_add.txt 2>&1
diff -q out_v5prg_add.txt out_v5o2_add.txt > /dev/null && echo "v5 ADD pragma vs O2: PASS" || echo "v5 ADD pragma vs O2: FAIL"

./v5_MUL_PRG < test_mul_100k.txt > out_v5prg_mul.txt 2>&1
./v5_MUL_O2  < test_mul_100k.txt > out_v5o2_mul.txt 2>&1
diff -q out_v5prg_mul.txt out_v5o2_mul.txt > /dev/null && echo "v5 MUL pragma vs O2: PASS" || echo "v5 MUL pragma vs O2: FAIL"

./v5_DIV_PRG < test_div_200k_100k.txt > out_v5prg_div.txt 2>&1
./v5_DIV_O2  < test_div_200k_100k.txt > out_v5o2_div.txt 2>&1
diff -q out_v5prg_div.txt out_v5o2_div.txt > /dev/null && echo "v5 DIV pragma vs O2: PASS" || echo "v5 DIV pragma vs O2: FAIL"

echo ""
echo "=== Done ==="
