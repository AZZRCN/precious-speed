#!/bin/bash
cd /home/azzr
VERSIONS='moptm_div_o2 moptm_div_o2_avx2 moptm_div_o3 moptm_div_o3_avx2'
TESTS='div_1M_500k.in div_200k_100k.in'
LOOPS=50
RUNS=10
echo '=== 4-version benchmark (10 runs x 50 loops, ms/loop) ==='
echo 'Note: GCC parallel build may affect VM performance; use relative values'
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
    printf '%-20s' 'version'
    for run in $(seq 1 $RUNS); do printf 'run%-4d' $run; done
    printf '%-8s%-8s%-8s\n' 'median' 'min' 'max'
    for V in $VERSIONS; do
        printf '%-20s' $V
        times=''
        for run in $(seq 1 $RUNS); do
            start=$(date +%s.%N)
            for i in $(seq 1 $LOOPS); do
                ./$V < $IN > /dev/null
            done
            end=$(date +%s.%N)
            elapsed=$(awk "BEGIN{printf \"%.3f\", ($end - $start) * 1000 / $LOOPS}")
            printf '%-8s' $elapsed
            times="$times $elapsed"
        done
        median=$(echo $times | tr ' ' '\n' | sort -n | awk 'NR==6{print}')
        minv=$(echo $times | tr ' ' '\n' | sort -n | head -1)
        maxv=$(echo $times | tr ' ' '\n' | sort -n | tail -1)
        printf '%-8s%-8s%-8s\n' $median $minv $maxv
    done
    echo ''
done
echo '=== DONE ==='
