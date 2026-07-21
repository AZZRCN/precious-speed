#!/bin/bash
# big_bench2.sh
# 精确对比 moptm vs best（都用 clock() CPU 时间）
cd /tmp/bench2

RUNS=10

bench_clock() {
    local exe=$1 test=$2 pattern=$3
    ./$exe < $test > /dev/null 2>&1  # warmup
    local total=0
    for i in $(seq 1 $RUNS); do
        # 捕获 stderr，忽略退出码（best_ADD/MUL 退出后段错误但结果正确）
        t=$(./$exe < $test 2>&1 1>/dev/null | awk "/$pattern/{print \$2}" || true)
        if [ -z "$t" ]; then
            echo "ERROR: no output from $exe on $test" >&2
            t=0
        fi
        total=$(awk -v t=$total -v n=$t 'BEGIN{print t+n}')
    done
    awk -v t=$total -v r=$RUNS 'BEGIN{printf "%.3f", t/r}'
}

echo "============================================"
echo "  Precise CPU Benchmark: moptm vs best (clock())"
echo "  Date: $(date '+%Y-%m-%d %H:%M:%S')"
echo "  $RUNS runs avg"
echo "============================================"

echo ""
echo "=== ADD ==="
for test in big_add_2M_2M.txt test_add_1M_1M.txt test_add_500k_500k.txt test_add_100k_100k.txt; do
    [ -f $test ] || continue
    m=$(bench_clock moptm_ADD_bench $test "CPU:")
    b=$(bench_clock best_ADD $test "BEST_CPU:")
    r=$(awk -v m=$m -v b=$b 'BEGIN{printf "%.2f", m/b}')
    echo "  $test: moptm=${m}ms best=${b}ms ratio=${r}x"
done

echo ""
echo "=== MUL ==="
for test in big_mul_2M_2M.txt test_mul_500k_500k.txt test_mul_300k_300k.txt test_mul_100k_100k.txt; do
    [ -f $test ] || continue
    m=$(bench_clock moptm_MUL_bench $test "CPU:")
    b=$(bench_clock best_MUL $test "BEST_CPU:")
    r=$(awk -v m=$m -v b=$b 'BEGIN{printf "%.2f", m/b}')
    echo "  $test: moptm=${m}ms best=${b}ms ratio=${r}x"
done

echo ""
echo "=== DIV ==="
for test in big_div_2M_1M.txt big_div_2M_500k.txt big_div_2M_2M.txt test_div_1M_500k.txt test_div_1M_100k.txt test_div_1M_900k.txt test_div_1M_999k.txt; do
    [ -f $test ] || continue
    m=$(bench_clock moptm_DIV_bench $test "CPU:")
    b=$(bench_clock best_DIV $test "BEST_CPU:")
    r=$(awk -v m=$m -v b=$b 'BEGIN{printf "%.2f", m/b}')
    echo "  $test: moptm=${m}ms best=${b}ms ratio=${r}x"
done

echo ""
echo "=== Done ==="
