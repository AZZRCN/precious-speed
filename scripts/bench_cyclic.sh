#!/bin/bash
cd /home/azzr
echo '=== Compile baseline and cyclic versions ==='
g++ -O2 -std=gnu++20 -DHINT_OP_DIV moptm_fusion.cpp -o moptm_baseline 2>&1 | tail -3
echo 'baseline compiled'
g++ -O2 -std=gnu++20 -DHINT_OP_DIV -DUSE_2NXN_CYCLIC moptm_fusion.cpp -o moptm_cyclic 2>&1 | tail -3
echo 'cyclic compiled'
echo ''

VERSIONS='moptm_baseline moptm_cyclic'
TESTS='div_1M_500k.in div_200k_100k.in'
LOOPS=50
RUNS=15
echo '=== baseline vs cyclic benchmark (15 runs x 50 loops, ms/loop) ==='
echo 'Note: VM now exclusive (GCC AI paused), data more stable'
echo ''
for IN in $TESTS; do
    echo "--- $IN ---"
    ref_md5=''
    for V in $VERSIONS; do
        md5=$(./$V < $IN | md5sum | awk '{print $1}')
        if [ -z "$ref_md5" ]; then
            ref_md5=$md5
            echo "MD5[$V]: $md5 (reference)"
        else
            if [ "$md5" = "$ref_md5" ]; then
                echo "MD5[$V]: $md5 MATCH"
            else
                echo "MD5[$V]: $md5 MISMATCH!!!"
            fi
        fi
    done
    echo ''
    # 交错运行 baseline 和 cyclic, 减少系统负载波动影响
    printf '%-20s' 'version'
    for run in $(seq 1 $RUNS); do printf 'run%-4d' $run; done
    printf '%-8s%-8s%-8s\n' 'median' 'min' 'max'
    base_times=''
    cyc_times=''
    for run in $(seq 1 $RUNS); do
        # baseline
        start=$(date +%s.%N)
        for i in $(seq 1 $LOOPS); do ./moptm_baseline < $IN > /dev/null; done
        end=$(date +%s.%N)
        b=$(awk "BEGIN{printf \"%.3f\", ($end - $start) * 1000 / $LOOPS}")
        base_times="$base_times $b"
        # cyclic
        start=$(date +%s.%N)
        for i in $(seq 1 $LOOPS); do ./moptm_cyclic < $IN > /dev/null; done
        end=$(date +%s.%N)
        c=$(awk "BEGIN{printf \"%.3f\", ($end - $start) * 1000 / $LOOPS}")
        cyc_times="$cyc_times $c"
    done
    printf '%-20s' 'moptm_baseline'
    for t in $base_times; do printf '%-8s' $t; done
    bmed=$(echo $base_times | tr ' ' '\n' | sort -n | awk 'NR==8{print}')
    bmin=$(echo $base_times | tr ' ' '\n' | sort -n | head -1)
    bmax=$(echo $base_times | tr ' ' '\n' | sort -n | tail -1)
    printf '%-8s%-8s%-8s\n' $bmed $bmin $bmax
    printf '%-20s' 'moptm_cyclic'
    for t in $cyc_times; do printf '%-8s' $t; done
    cmed=$(echo $cyc_times | tr ' ' '\n' | sort -n | awk 'NR==8{print}')
    cmin=$(echo $cyc_times | tr ' ' '\n' | sort -n | head -1)
    cmax=$(echo $cyc_times | tr ' ' '\n' | sort -n | tail -1)
    printf '%-8s%-8s%-8s\n' $cmed $cmin $cmax
    echo ''
    echo "Speedup: $(awk "BEGIN{printf \"%.2f%%\", ($bmed - $cmed) / $bmed * 100}") (positive = cyclic faster)"
    echo ''
done
echo '=== DONE ==='
