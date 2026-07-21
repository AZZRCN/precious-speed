#!/bin/bash
# Benchmark: 不同 INV_NEWTON_BASE_THRESHOLD
cd /home/azzr

VERSIONS=("moptm_o2" "moptm_o2_thr128" "moptm_o2_thr256")
TESTS=("div_1M_500k.in" "div_200k_100k.in")
LOOPS=50
RUNS=15

for test in "${TESTS[@]}"; do
    echo "=== Test: $test ==="
    # warmup + MD5
    for v in "${VERSIONS[@]}"; do
        ./$v < $test > /tmp/warmup_${v}.out 2>/dev/null
    done
    ref_md5=$(md5sum /tmp/warmup_${VERSIONS[0]}.out | cut -d' ' -f1)
    for v in "${VERSIONS[@]}"; do
        cur_md5=$(md5sum /tmp/warmup_${v}.out | cut -d' ' -f1)
        if [ "$cur_md5" != "$ref_md5" ]; then
            echo "  [FAIL] $v MD5 mismatch: $cur_md5 vs $ref_md5"
        fi
    done
    echo "  [OK] MD5: $ref_md5"

    for v in "${VERSIONS[@]}"; do
        times=()
        for run in $(seq 1 $RUNS); do
            t0=$(date +%s.%N)
            for loop in $(seq 1 $LOOPS); do
                ./$v < $test > /dev/null
            done
            t1=$(date +%s.%N)
            elapsed=$(echo "$t1 - $t0" | bc -l)
            per_loop=$(echo "scale=4; $elapsed / $LOOPS * 1000" | bc -l)
            times+=($per_loop)
        done
        sorted=$(printf '%s\n' "${times[@]}" | sort -n)
        median=$(echo "$sorted" | sed -n "$((RUNS/2 + 1))p")
        echo "  $v: median=${median}ms"
    done
    echo ""
done
echo "=== Done ==="
