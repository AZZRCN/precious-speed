#!/bin/bash
# compile_best_bench.sh
cd /tmp/bench2

# 从原始源码重新复制（之前可能被破坏）
cp /mnt/hgfs/precious_speed/best/add.cpp best_add.cpp
cp /mnt/hgfs/precious_speed/best/mul.cpp best_mul.cpp
cp /mnt/hgfs/precious_speed/best/div.cpp best_div.cpp

# 加 clock() 计时
python3 /tmp/add_clock_to_best2.py

echo "=== compile ==="
LC_FLAGS="-O2 -std=gnu++20 -static -DONLINE_JUDGE"
g++ ${LC_FLAGS} -o best_ADD best_add.cpp && echo "  best_ADD OK"
g++ ${LC_FLAGS} -o best_MUL best_mul.cpp && echo "  best_MUL OK"
g++ ${LC_FLAGS} -o best_DIV best_div.cpp && echo "  best_DIV OK"

echo "=== test ==="
printf '1\n1234567890\n9876543210\n' | ./best_ADD 2>&1
printf '1\n1234567890\n9876543210\n' | ./best_MUL 2>&1
printf '1\n1234567890\n9876543210\n' | ./best_DIV 2>&1
