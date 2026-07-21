#!/bin/bash
# v3 vs v2_baseline 交替 benchmark, taskset -c 0, min/p10 指标
cd /tmp/bench
ROUNDS=30
WARMUP=3

time_bin() {
    local bin=$1
    local data=$2
    python3 -c "
import subprocess, time
t0=time.perf_counter()
subprocess.run(['./$bin'], stdin=open('$data','rb'), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
print((time.perf_counter()-t0)*1000)
"
}

bench_case() {
    local old_bin=$1
    local new_bin=$2
    local data=$3
    local label=$4
    echo ""
    echo "=== $label ($data) ==="
    # warmup
    for i in $(seq 1 $WARMUP); do
        time_bin "$old_bin" "$data" > /dev/null
        time_bin "$new_bin" "$data" > /dev/null
    done
    # alternating
    local old_times=""
    local new_times=""
    for i in $(seq 1 $ROUNDS); do
        if [ $((i % 2)) -eq 0 ]; then
            t1=$(time_bin "$old_bin" "$data")
            t2=$(time_bin "$new_bin" "$data")
        else
            t2=$(time_bin "$new_bin" "$data")
            t1=$(time_bin "$old_bin" "$data")
        fi
        old_times="$old_times $t1"
        new_times="$new_times $t2"
        if [ $((i % 10)) -eq 0 ]; then
            old_min=$(echo $old_times | tr ' ' '\n' | sort -n | head -1)
            new_min=$(echo $new_times | tr ' ' '\n' | sort -n | head -1)
            echo "  round $i/$ROUNDS: old_min=$old_min new_min=$new_min"
        fi
    done
    # compute min and p10
    old_sorted=$(echo $old_times | tr ' ' '\n' | sort -n)
    new_sorted=$(echo $new_times | tr ' ' '\n' | sort -n)
    old_min=$(echo "$old_sorted" | head -1)
    new_min=$(echo "$new_sorted" | head -1)
    old_p10=$(echo "$old_sorted" | sed -n '3p')
    new_p10=$(echo "$new_sorted" | sed -n '3p')
    # delta
    python3 -c "
o=$old_min; n=$new_min; op=$old_p10; np=$new_p10
print(f'  OLD min={o:.2f}ms p10={op:.2f}ms')
print(f'  NEW min={n:.2f}ms p10={np:.2f}ms')
print(f'  delta_min={(n-o)/o*100:+.1f}%  delta_p10={(np-op)/op*100:+.1f}%')
"
}

echo "=== v3 vs v2_baseline benchmark ==="
echo "Date: $(date)"
echo "Rounds: $ROUNDS, Warmup: $WARMUP"

bench_case moptm_fusion_v2_DIV_baseline moptm_fusion_v3_DIV div_1M_500k.txt "DIV 1M/500k"
bench_case moptm_fusion_v2_DIV_baseline moptm_fusion_v3_DIV div_200k_100k.txt "DIV 200k/100k"
bench_case moptm_fusion_v2_DIV_baseline moptm_fusion_v3_DIV div_1M_100k.txt "DIV 1M/100k"
bench_case moptm_fusion_O2_ADD moptm_fusion_v3_ADD add_1M.txt "ADD 1M"
bench_case moptm_fusion_O2_MUL moptm_fusion_v3_MUL mul_500k.txt "MUL 500k"

echo ""
echo "=== Done ==="
