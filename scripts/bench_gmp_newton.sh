#!/bin/bash
# Benchmark absInvNewtonGMP cyclic vs baseline
cd /tmp/bench
LOOPS=50

run_bench() {
    local bin=$1
    local infile=$2
    # 5 runs, output total user CPU seconds for each run
    for r in 1 2 3 4 5; do
        bash -c '
            TIMEFORMAT="%U"
            time {
                for i in {1..'"$LOOPS"'}; do
                    ./'"$bin"' < "'"$infile"'" > /dev/null
                done
            }
        ' 2>&1
    done
}

echo "=== DIV 1M/500k baseline ==="
run_bench moptm_div_base div_1M_500k.in
echo "=== DIV 1M/500k GMP_cyclic ==="
run_bench moptm_div_gmp div_1M_500k.in
echo "=== DIV 200k/100k baseline ==="
run_bench moptm_div_base div_200k_100k.in
echo "=== DIV 200k/100k GMP_cyclic ==="
run_bench moptm_div_gmp div_200k_100k.in
