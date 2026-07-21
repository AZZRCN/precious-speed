#!/bin/bash
# big_bench.sh
# 大数据对比 moptm vs best
cd /tmp/bench2

LC_FLAGS="-O2 -std=gnu++20 -static -DONLINE_JUDGE"

# moptm 用 clock() 精确计时
bench_moptm() {
    local exe=$1 test=$2 runs=${3:-10}
    ./$exe < $test > /dev/null 2>&1  # warmup
    local total=0
    for i in $(seq 1 $runs); do
        t=$(./$exe < $test 2>&1 1>/dev/null | awk '/CPU:/{print $2}')
        total=$(awk -v t=$total -v n=$t 'BEGIN{print t+n}')
    done
    awk -v t=$total -v r=$runs 'BEGIN{printf "%.3f", t/r}'
}

# best 用 /usr/bin/time (大数据 > 50ms, 精度够)
bench_best() {
    local exe=$1 test=$2 runs=${3:-10}
    ./$exe < $test > /dev/null 2>&1  # warmup
    local total=0
    for i in $(seq 1 $runs); do
        t=$( { /usr/bin/time -f "%U %S" ./$exe < $test > /dev/null; } 2>&1 )
        total=$(echo $t | awk -v tot=$total '{print tot + $1 + $2}')
    done
    echo $total $runs | awk '{printf "%.3f", $1 / $2 * 1000}'  # 转为毫秒
}

echo "============================================"
echo "  Big Data Benchmark: moptm vs best (LC -O2)"
echo "  Date: $(date '+%Y-%m-%d %H:%M:%S')"
echo "============================================"

echo ""
echo "=== ADD ==="
for test in big_add_2M_2M.txt test_add_1M_1M.txt test_add_500k_500k.txt; do
    [ -f $test ] || continue
    m=$(bench_moptm moptm_ADD_bench $test)
    b=$(bench_best best_ADD $test)
    r=$(awk -v m=$m -v b=$b 'BEGIN{printf "%.2f", m/b}')
    echo "  $test: moptm=${m}ms best=${b}ms ratio=${r}x"
done

echo ""
echo "=== MUL ==="
for test in big_mul_2M_2M.txt test_mul_500k_500k.txt test_mul_300k_300k.txt; do
    [ -f $test ] || continue
    m=$(bench_moptm moptm_MUL_bench $test)
    b=$(bench_best best_MUL $test)
    r=$(awk -v m=$m -v b=$b 'BEGIN{printf "%.2f", m/b}')
    echo "  $test: moptm=${m}ms best=${b}ms ratio=${r}x"
done

echo ""
echo "=== DIV ==="
for test in big_div_2M_1M.txt big_div_2M_500k.txt big_div_2M_2M.txt test_div_1M_500k.txt test_div_1M_100k.txt; do
    [ -f $test ] || continue
    m=$(bench_moptm moptm_DIV_bench $test)
    b=$(bench_best best_DIV $test)
    r=$(awk -v m=$m -v b=$b 'BEGIN{printf "%.2f", m/b}')
    echo "  $test: moptm=${m}ms best=${b}ms ratio=${r}x"
done

echo ""
echo "=== Correctness check (big data) ==="
./moptm_ADD_bench < big_add_2M_2M.txt 2>/dev/null > out_m.txt
./best_ADD < big_add_2M_2M.txt > out_b.txt 2>/dev/null
if diff -q out_m.txt out_b.txt > /dev/null; then echo "  ADD 2M: PASS"; else echo "  ADD 2M: FAIL"; fi

./moptm_MUL_bench < big_mul_2M_2M.txt 2>/dev/null > out_m.txt
./best_MUL < big_mul_2M_2M.txt > out_b.txt 2>/dev/null
if diff -q out_m.txt out_b.txt > /dev/null; then echo "  MUL 2M: PASS"; else echo "  MUL 2M: FAIL"; fi

./moptm_DIV_bench < big_div_2M_1M.txt 2>/dev/null > out_m.txt
./best_DIV < big_div_2M_1M.txt > out_b.txt 2>/dev/null
if diff -q out_m.txt out_b.txt > /dev/null; then echo "  DIV 2M/1M: PASS"; else echo "  DIV 2M/1M: FAIL"; fi

./moptm_DIV_bench < big_div_2M_500k.txt 2>/dev/null > out_m.txt
./best_DIV < big_div_2M_500k.txt > out_b.txt 2>/dev/null
if diff -q out_m.txt out_b.txt > /dev/null; then echo "  DIV 2M/500k: PASS"; else echo "  DIV 2M/500k: FAIL"; fi

echo ""
echo "=== Done ==="
