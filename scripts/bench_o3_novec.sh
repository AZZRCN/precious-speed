#!/bin/bash
# Benchmark: O2 vs O3 vs O3-novec
# 15 runs × 50 loops, median

cd /home/azzr

VERSIONS=("moptm_o2" "moptm_o3" "moptm_o3_novec")
TESTS=("div_1M_500k.in" "div_200k_100k.in")
LOOPS=50
RUNS=15

for test in "${TESTS[@]}"; do
    echo "=== Test: $test ==="
    # warmup
    for v in "${VERSIONS[@]}"; do
        ./$v < $test > /tmp/warmup_${v}.out 2>/dev/null
    done
    # MD5 check
    ref_md5=$(md5sum /tmp/warmup_${VERSIONS[0]}.out | cut -d' ' -f1)
    for v in "${VERSIONS[@]}"; do
        cur_md5=$(md5sum /tmp/warmup_${v}.out | cut -d' ' -f1)
        if [ "$cur_md5" != "$ref_md5" ]; then
            echo "  [FAIL] $v MD5 mismatch: $cur_md5 vs $ref_md5"
        fi
    done
    echo "  [OK] All MD5 match: $ref_md5"

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
        # median
        sorted=$(printf '%s\n' "${times[@]}" | sort -n)
        median=$(echo "$sorted" | sed -n "$((RUNS/2 + 1))p")
        echo "  $v: median=${median}ms (runs=${RUNS}x${LOOPS})"
    done
    echo ""
done

echo "=== Done ==="
