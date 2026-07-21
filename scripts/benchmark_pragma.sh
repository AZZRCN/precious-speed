#!/bin/bash
# benchmark_pragma.sh - 对比纯O2 vs O2+pragma(O3,avx2,fma)
cd /tmp/fusion

# 创建带 pragma 的版本
for f in add mul div; do
    echo '#pragma GCC optimize("O3,unroll-loops")' > ${f}_pragma.cpp
    echo '#pragma GCC target("avx2,fma")' >> ${f}_pragma.cpp
    cat ${f}.cpp >> ${f}_pragma.cpp
done

# 编译 pragma 版本
echo "=== Compiling pragma versions ==="
g++ -x c++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -o best_ADD_praga add_pragma.cpp 2>&1 | tail -3
echo "best_ADD_praga done"
g++ -x c++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -o best_MUL_praga mul_pragma.cpp 2>&1 | tail -3
echo "best_MUL_praga done"
g++ -x c++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_ADD -o hint_ADD_praga div_pragma.cpp 2>&1 | tail -3
echo "hint_ADD_praga done"
g++ -x c++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_MUL -o hint_MUL_praga div_pragma.cpp 2>&1 | tail -3
echo "hint_MUL_praga done"
g++ -x c++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV -o hint_DIV_praga div_pragma.cpp 2>&1 | tail -3
echo "hint_DIV_praga done"

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
    echo "$name: ${avg_ms}ms"
}

echo ""
echo "========================================"
echo "  ADD 1M+1M (20 iters) - O2 vs O2+pragma"
echo "========================================"
run_bench "best_ADD  O2       " ./best_ADD       test_add_1M.txt 20
run_bench "best_ADD  O2+prg   " ./best_ADD_praga test_add_1M.txt 20
run_bench "hint_ADD O2       " ./hint_ADD       test_add_1M.txt 20
run_bench "hint_ADD O2+prg   " ./hint_ADD_praga test_add_1M.txt 20

echo ""
echo "========================================"
echo "  MUL 500k*500k (20 iters) - O2 vs O2+pragma"
echo "========================================"
run_bench "best_MUL  O2       " ./best_MUL       test_mul_500k.txt 20
run_bench "best_MUL  O2+prg   " ./best_MUL_praga test_mul_500k.txt 20
run_bench "hint_MUL O2       " ./hint_MUL       test_mul_500k.txt 20
run_bench "hint_MUL O2+prg   " ./hint_MUL_praga test_mul_500k.txt 20

echo ""
echo "========================================"
echo "  DIV 1M/500k (10 iters) - O2 vs O2+pragma"
echo "========================================"
run_bench "hint_DIV O2       " ./hint_DIV       test_div_1M_500k.txt 10
run_bench "hint_DIV O2+prg   " ./hint_DIV_praga test_div_1M_500k.txt 10

echo ""
echo "========================================"
echo "  Correctness (pragma vs non-pragma)"
echo "========================================"
./best_ADD_praga < test_add_100k.txt > out_praga_add.txt 2>&1
./best_ADD       < test_add_100k.txt > out_plain_add.txt 2>&1
diff -q out_praga_add.txt out_plain_add.txt > /dev/null && echo "best_ADD: PASS" || echo "best_ADD: FAIL"

./hint_ADD_praga < test_add_100k.txt > out_praga_hadd.txt 2>&1
./hint_ADD       < test_add_100k.txt > out_plain_hadd.txt 2>&1
diff -q out_praga_hadd.txt out_plain_hadd.txt > /dev/null && echo "hint_ADD: PASS" || echo "hint_ADD: FAIL"

./best_MUL_praga < test_mul_100k.txt > out_praga_mul.txt 2>&1
./best_MUL       < test_mul_100k.txt > out_plain_mul.txt 2>&1
diff -q out_praga_mul.txt out_plain_mul.txt > /dev/null && echo "best_MUL: PASS" || echo "best_MUL: FAIL"

./hint_MUL_praga < test_mul_100k.txt > out_praga_hmul.txt 2>&1
./hint_MUL       < test_mul_100k.txt > out_plain_hmul.txt 2>&1
diff -q out_praga_hmul.txt out_plain_hmul.txt > /dev/null && echo "hint_MUL: PASS" || echo "hint_MUL: FAIL"

echo ""
echo "=== Done ==="
