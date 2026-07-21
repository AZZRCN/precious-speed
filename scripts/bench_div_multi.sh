#!/bin/bash
# Multiple benchmark runs for DIV 1M/500k
cd /tmp/o2compare
DIV_IN=/tmp/fusion/div_1M_500k.in
RUNS=7
WARMUP=2

bench() {
    local bin=$1
    local times=()
    for i in $(seq 1 $WARMUP); do
        $bin < $DIV_IN > /dev/null 2>&1
    done
    for i in $(seq 1 $RUNS); do
        local s=$(date +%s.%N)
        $bin < $DIV_IN > /dev/null 2>&1
        local e=$(date +%s.%N)
        times+=("$(awk "BEGIN{printf \"%.3f\", ($e - $s)*1000}")")
    done
    printf '%s\n' "${times[@]}" | sort -n | awk 'NR==4{printf "%s", $0}'
}

echo "DIV 1M/500k benchmark ($RUNS runs + $WARMUP warmup, median)"
for trial in 1 2 3; do
    old=$(bench ./mf_DIV_base28)
    new=$(bench ./mf_DIV)
    awk -v o="$old" -v n="$new" 'BEGIN{printf "  trial %d: OLD=%s ms  NEW=%s ms  delta=%.2f%%\n", '$trial', o, n, (n-o)/o*100}'
done
