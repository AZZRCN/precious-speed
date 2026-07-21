#!/bin/bash
# 测试 fftMulModBm1 正确性
set -e
cd /tmp/bench

echo "=== 编译测试版本 ==="
g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_TESTMOD \
    -o test_modbm1 moptm_fusion.cpp 2>&1 | head -30

if [ ! -f test_modbm1 ]; then
    echo "编译失败"
    exit 1
fi
echo "编译成功"

echo ""
echo "=== 生成测试数据 ==="
python3 gen_testmod.py 42 > test_modbm1.in
echo "生成 $(wc -l < test_modbm1.in) 行测试数据"

echo ""
echo "=== 运行测试 ==="
./test_modbm1 < test_modbm1.in 2>&1

echo ""
echo "=== 多 seed 测试 ==="
for seed in 1 2 3 4 5 100 200; do
    echo "--- seed=$seed ---"
    python3 gen_testmod.py $seed > test_modbm1_seed.in
    ./test_modbm1 < test_modbm1_seed.in 2>&1 | tail -1
done
