#!/bin/bash
# 编译多个版本: THRESHOLD 和 O3 组合
cd /tmp/bench

echo "=== Compiling variants ==="

# 基线 O2 (THRESHOLD=64)
g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV moptm_fusion.cpp -o moptm_div_t64_o2 2>&1 | tail -3
echo "t64_o2 done"

# THRESHOLD=256 O2
g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV -DINV_NEWTON_BASE_THRESHOLD=256 moptm_fusion.cpp -o moptm_div_t256_o2 2>&1 | tail -3
echo "t256_o2 done"

# THRESHOLD=512 O2
g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV -DINV_NEWTON_BASE_THRESHOLD=512 moptm_fusion.cpp -o moptm_div_t512_o2 2>&1 | tail -3
echo "t512_o2 done"

# 基线 O3
g++ -O3 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV moptm_fusion.cpp -o moptm_div_t64_o3 2>&1 | tail -3
echo "t64_o3 done"

echo "=== Verify correctness (MD5) ==="
md5_base=$(./moptm_div_t64_o2 < div_1M_500k.in | md5sum)
md5_t256=$(./moptm_div_t256_o2 < div_1M_500k.in | md5sum)
md5_t512=$(./moptm_div_t512_o2 < div_1M_500k.in | md5sum)
md5_o3=$(./moptm_div_t64_o3 < div_1M_500k.in | md5sum)
echo "base  : $md5_base"
echo "t256  : $md5_t256"
echo "t512  : $md5_t512"
echo "o3    : $md5_o3"

echo ""
echo "=== Benchmark DIV 1M/500k (50 loops, 5 runs) ==="
LOOPS=50
for variant in t64_o2 t256_o2 t512_o2 t64_o3; do
    echo "--- $variant ---"
    for r in 1 2 3 4 5; do
        bash -c '
            TIMEFORMAT="%U"
            time {
                for i in {1..'"$LOOPS"'}; do
                    ./moptm_div_'"$variant"' < div_1M_500k.in > /dev/null
                done
            }
        ' 2>&1
    done
done
