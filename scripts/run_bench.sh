#!/bin/bash
cd /home/azzr
LOOPS=50
RUNS=15
for IN in div_1M_500k.in div_200k_100k.in; do
    echo "=== $IN (15 runs x $LOOPS loops, ms/loop) ==="
    printf '%-20s' 'version'
    for run in $(seq 1 $RUNS); do printf 'run%-6d' $run; done
    printf '%-10s%-10s\n' 'median' 'min'
    for V in moptm_baseline moptm_cyclic; do
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
