#!/bin/bash
# debug_bench.sh
cd /tmp/bench2

echo "=== Step 1: raw time output ==="
{ /usr/bin/time -f "%U %S" ./moptm_DIV < test_div_1M_500k.txt > /dev/null; } 2>&1
echo "---"

echo ""
echo "=== Step 2: capture in variable ==="
t=$( { /usr/bin/time -f "%U %S" ./moptm_DIV < test_div_1M_500k.txt > /dev/null; } 2>&1 )
echo "t=[$t]"

echo ""
echo "=== Step 3: awk parse ==="
echo "$t" | awk '{print $1 + $2}'

echo ""
echo "=== Step 4: full bench_cpu ==="
bench_cpu() {
    local exe=$1 test_file=$2 runs=${3:-5}
    ./${exe} < ${test_file} > /dev/null 2>&1
    local total=0
    for i in $(seq 1 ${runs}); do
        local t=$( { /usr/bin/time -f "%U %S" ./${exe} < ${test_file} > /dev/null; } 2>&1 )
        echo "  run $i: t=[$t]"
        total=$(echo ${t} | awk -v tot=${total} '{print tot + $1 + $2}')
        echo "  total=$total"
    done
    echo ${total} ${runs} | awk '{printf "%.4f", $1 / $2}'
}

echo "DIV 1M_500k:"
bench_cpu moptm_DIV test_div_1M_500k.txt
