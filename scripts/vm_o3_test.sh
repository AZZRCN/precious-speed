#!/bin/bash
cd /home/azzr
echo "=== Compile O2 and O3 versions (cyclic + dir3 + dir2+7) ==="
g++ -O2 -std=gnu++20 -static -DHINT_OP_DIV -o moptm_o2 moptm_fusion.cpp 2>&1 | tail -3
echo "O2 compiled"
g++ -O3 -std=gnu++20 -static -DHINT_OP_DIV -o moptm_o3 moptm_fusion.cpp 2>&1 | tail -3
echo "O3 compiled"
g++ -O2 -std=gnu++20 -static -DHINT_OP_DIV -mavx2 -o moptm_o2_avx2 moptm_fusion.cpp 2>&1 | tail -3
echo "O2+avx2 compiled"
g++ -O3 -std=gnu++20 -static -DHINT_OP_DIV -mavx2 -o moptm_o3_avx2 moptm_fusion.cpp 2>&1 | tail -3
echo "O3+avx2 compiled"
echo ""

echo "=== Correctness check ==="
md5_o2=$(./moptm_o2 < div_1M_500k.in | md5sum | awk '{print $1}')
md5_o3=$(./moptm_o3 < div_1M_500k.in | md5sum | awk '{print $1}')
md5_o2a=$(./moptm_o2_avx2 < div_1M_500k.in | md5sum | awk '{print $1}')
md5_o3a=$(./moptm_o3_avx2 < div_1M_500k.in | md5sum | awk '{print $1}')
echo "O2:       $md5_o2"
echo "O3:       $md5_o3"
echo "O2+avx2:  $md5_o2a"
echo "O3+avx2:  $md5_o3a"
if [ "$md5_o2" = "$md5_o3" ] && [ "$md5_o2" = "$md5_o2a" ] && [ "$md5_o2" = "$md5_o3a" ]; then
    echo "All MD5 MATCH"
else
    echo "MD5 MISMATCH!!!"
fi
echo ""

echo "=== Benchmark 4 versions (15 runs x 50 loops, ms/loop) ==="
LOOPS=50
RUNS=15
for IN in div_1M_500k.in div_200k_100k.in; do
    echo "--- $IN ---"
    printf '%-20s' 'version'
    for run in $(seq 1 $RUNS); do printf 'run%-6d' $run; done
    printf '%-10s%-10s\n' 'median' 'min'
    for V in moptm_o2 moptm_o3 moptm_o2_avx2 moptm_o3_avx2; do
        printf '%-20s' $V
        times=''
        for run in $(seq 1 $RUNS); do
            start=$(date +%s.%N)
            for i in $(seq 1 $LOOPS); do
                ./$V < $IN > /dev/null
            done
            end=$(date +%s.%N)
            elapsed=$(awk "BEGIN{print ($end - $start) * 1000 / $LOOPS}")
            printf '%.3f    ' $elapsed
            times="$times $elapsed"
        done
        median=$(echo $times | tr ' ' '\n' | sort -n | awk 'NR==8{print}')
        min=$(echo $times | tr ' ' '\n' | sort -n | head -1)
        printf '%-10s%-10s\n' $median $min
    done
    echo ''
done
