#!/bin/bash
# 在 VM 上重建 6step 递归版各叶子规模 + gold 对照。
# 统一用 -march=x86-64-v3（AVX2+FMA，无 AVX-512）以贴近 LC 的 Zen3 代码生成。
set -e
cd /home/azzr/mulbench
FLAGS="-O2 -std=c++23 -march=x86-64-v3"

echo "== rebuild gold baseline =="
g++ $FLAGS -o bin/v3_gold src/mul_gold.cpp

echo "== rebuild r4 (previous LC submission) =="
g++ $FLAGS -o bin/v3_r4 src/mul_r4.cpp

for L in 19 14 12 11 10 9; do
    echo "== build leaf 2^$L =="
    g++ $FLAGS -DMUL_FFT_LEAF_LOG=$L -o bin/v3_L$L src/mul_6step.cpp
done

echo "== sizes =="
ls -l bin/v3_* | awk '{print $5, $9}'
echo BUILD_DONE
