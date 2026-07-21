#!/bin/bash
# 编译 O2 + funroll-loops 版本，benchmark 对比
cd /tmp/bench
set -e

echo "=== 编译 O2 + funroll-loops ==="
g++ -O2 -funroll-loops -std=gnu++20 -static -DONLINE_JUDGE moptm_fusion.cpp -o moptm_div_unroll -DHINT_OP_DIV
g++ -O2 -funroll-loops -std=gnu++20 -static -DONLINE_JUDGE moptm_fusion.cpp -o moptm_add_unroll -DHINT_OP_ADD
g++ -O2 -funroll-loops -std=gnu++20 -static -DONLINE_JUDGE moptm_fusion.cpp -o moptm_mul_unroll -DHINT_OP_MUL
echo "编译完成"

echo ""
echo "=== 正确性验证 (MD5) ==="
md5_div_base=$(./moptm_div < div_1M_500k.in | md5sum | awk '{print $1}')
md5_div_unroll=$(./moptm_div_unroll < div_1M_500k.in | md5sum | awk '{print $1}')
echo "DIV base:    $md5_div_base"
echo "DIV unroll:  $md5_div_unroll"
[ "$md5_div_base" = "$md5_div_unroll" ] && echo "DIV: MATCH" || echo "DIV: MISMATCH"

echo ""
echo "=== Benchmark DIV 1M/500k ==="
bash /tmp/bench/bench_quick.sh moptm_div div_1M_500k.in 50 5
bash /tmp/bench/bench_quick.sh moptm_div_unroll div_1M_500k.in 50 5

echo ""
echo "=== Benchmark ADD 1M ==="
bash /tmp/bench/bench_quick.sh moptm_add add_1M.in 50 5
bash /tmp/bench/bench_quick.sh moptm_add_unroll add_1M.in 50 5

echo ""
echo "=== Benchmark MUL 500k ==="
bash /tmp/bench/bench_quick.sh moptm_mul mul_500k.in 50 5
bash /tmp/bench/bench_quick.sh moptm_mul_unroll mul_500k.in 50 5
