#!/bin/bash
# benchmark_o3_vs_pragma.sh - 对比 -O2 vs -O3 vs -O2+pragma
cd /tmp/fusion

# 创建 pragma 版本（在文件顶部插入 pragma）
for mode in ADD MUL DIV; do
    echo '#pragma GCC optimize("O3,unroll-loops")' > moptm_${mode}_pragma.cpp
    cat moptm.cpp >> moptm_${mode}_pragma.cpp
done

echo "=== Compiling moptm (3 modes x 3 opt levels = 9 binaries) ==="
echo ""

# -O2 版本（LC 默认）
g++ -x c++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_ADD -UHINT_OP_DIV -o moptm_ADD_O2  moptm.cpp 2>&1 | tail -2
echo "[1/9] moptm_ADD_O2 done"
g++ -x c++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_MUL -UHINT_OP_DIV -o moptm_MUL_O2  moptm.cpp 2>&1 | tail -2
echo "[2/9] moptm_MUL_O2 done"
g++ -x c++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV               -o moptm_DIV_O2  moptm.cpp 2>&1 | tail -2
echo "[3/9] moptm_DIV_O2 done"

# -O3 版本（上限参考）
g++ -x c++ -O3 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_ADD -UHINT_OP_DIV -o moptm_ADD_O3  moptm.cpp 2>&1 | tail -2
echo "[4/9] moptm_ADD_O3 done"
g++ -x c++ -O3 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_MUL -UHINT_OP_DIV -o moptm_MUL_O3  moptm.cpp 2>&1 | tail -2
echo "[5/9] moptm_MUL_O3 done"
g++ -x c++ -O3 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV               -o moptm_DIV_O3  moptm.cpp 2>&1 | tail -2
echo "[6/9] moptm_DIV_O3 done"

# -O2 + pragma 版本（LC 提交用）
g++ -x c++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_ADD -UHINT_OP_DIV -o moptm_ADD_PRG  moptm_ADD_pragma.cpp 2>&1 | tail -2
echo "[7/9] moptm_ADD_PRG done"
g++ -x c++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_MUL -UHINT_OP_DIV -o moptm_MUL_PRG  moptm_MUL_pragma.cpp 2>&1 | tail -2
echo "[8/9] moptm_MUL_PRG done"
g++ -x c++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV               -o moptm_DIV_PRG  moptm_DIV_pragma.cpp 2>&1 | tail -2
echo "[9/9] moptm_DIV_PRG done"

echo ""
ls -la moptm_*_O2 moptm_*_O3 moptm_*_PRG 2>&1

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
run_bench "moptm -O2       " ./moptm_ADD_O2  test_add_1M.txt 20
run_bench "moptm -O3       " ./moptm_ADD_O3  test_add_1M.txt 20
run_bench "moptm -O2+pragma" ./moptm_ADD_PRG test_add_1M.txt 20
run_bench "best  -O2       " ./best_ADD      test_add_1M.txt 20

echo ""
echo "========================================"
echo "  ADD 100k+100k (50 iters)"
echo "========================================"
run_bench "moptm -O2       " ./moptm_ADD_O2  test_add_100k.txt 50
run_bench "moptm -O3       " ./moptm_ADD_O3  test_add_100k.txt 50
run_bench "moptm -O2+pragma" ./moptm_ADD_PRG test_add_100k.txt 50
run_bench "best  -O2       " ./best_ADD      test_add_100k.txt 50

echo ""
echo "========================================"
echo "  MUL 500k*500k (20 iters)"
echo "========================================"
run_bench "moptm -O2       " ./moptm_MUL_O2  test_mul_500k.txt 20
run_bench "moptm -O3       " ./moptm_MUL_O3  test_mul_500k.txt 20
run_bench "moptm -O2+pragma" ./moptm_MUL_PRG test_mul_500k.txt 20
run_bench "best  -O2       " ./best_MUL      test_mul_500k.txt 20

echo ""
echo "========================================"
echo "  MUL 100k*100k (50 iters)"
echo "========================================"
run_bench "moptm -O2       " ./moptm_MUL_O2  test_mul_100k.txt 50
run_bench "moptm -O3       " ./moptm_MUL_O3  test_mul_100k.txt 50
run_bench "moptm -O2+pragma" ./moptm_MUL_PRG test_mul_100k.txt 50
run_bench "best  -O2       " ./best_MUL      test_mul_100k.txt 50

echo ""
echo "========================================"
echo "  DIV 1M/500k (10 iters)"
echo "========================================"
run_bench "moptm -O2       " ./moptm_DIV_O2  test_div_1M_500k.txt 10
run_bench "moptm -O3       " ./moptm_DIV_O3  test_div_1M_500k.txt 10
run_bench "moptm -O2+pragma" ./moptm_DIV_PRG test_div_1M_500k.txt 10

echo ""
echo "========================================"
echo "  DIV 200k/100k (20 iters)"
echo "========================================"
run_bench "moptm -O2       " ./moptm_DIV_O2  test_div_200k_100k.txt 20
run_bench "moptm -O3       " ./moptm_DIV_O3  test_div_200k_100k.txt 20
run_bench "moptm -O2+pragma" ./moptm_DIV_PRG test_div_200k_100k.txt 20

echo ""
echo "========================================"
echo "  Correctness (moptm -O3 vs -O2)"
echo "========================================"
./moptm_ADD_O3 < test_add_100k.txt > out_o3_add.txt 2>&1
./moptm_ADD_O2 < test_add_100k.txt > out_o2_add.txt 2>&1
diff -q out_o3_add.txt out_o2_add.txt > /dev/null && echo "ADD: O3 vs O2 PASS" || echo "ADD: O3 vs O2 FAIL"

./moptm_ADD_PRG < test_add_100k.txt > out_prg_add.txt 2>&1
diff -q out_prg_add.txt out_o2_add.txt > /dev/null && echo "ADD: pragma vs O2 PASS" || echo "ADD: pragma vs O2 FAIL"

./moptm_MUL_O3 < test_mul_100k.txt > out_o3_mul.txt 2>&1
./moptm_MUL_O2 < test_mul_100k.txt > out_o2_mul.txt 2>&1
diff -q out_o3_mul.txt out_o2_mul.txt > /dev/null && echo "MUL: O3 vs O2 PASS" || echo "MUL: O3 vs O2 FAIL"

./moptm_DIV_O3 < test_div_200k_100k.txt > out_o3_div.txt 2>&1
./moptm_DIV_O2 < test_div_200k_100k.txt > out_o2_div.txt 2>&1
diff -q out_o3_div.txt out_o2_div.txt > /dev/null && echo "DIV: O3 vs O2 PASS" || echo "DIV: O3 vs O2 FAIL"

echo ""
echo "=== Done ==="
