#!/bin/bash
# 在 VM 上构建 r4dfs（融合 radix-4 + 深度优先递归）各叶子规模。
# 递归每级吃 2 层，transformSize=2^19 => 有效叶子只能是奇数指数 9/11/13，
# 故 LEAF_LOG 取 9/10 落到 2^9，11/12 落到 2^11，13/14 落到 2^13。
set -e
cd /home/azzr/mulbench
FLAGS="-O2 -std=c++23 -march=x86-64-v3"

for L in 9 11 13 15; do
    echo "== build r4dfs leaf 2^$L =="
    g++ $FLAGS -DMUL_FFT_LEAF_LOG=$L -o bin/v3_r4dfs$L src/mul_r4dfs.cpp
done

echo "== sizes =="
ls -l bin/v3_r4dfs* | awk '{print $5, $9}'
echo BUILD_DONE
