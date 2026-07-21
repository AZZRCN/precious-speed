#!/bin/bash
# verify_and_bench.sh
# 验证正确性 + 性能基准 (双肢打包 absAdd 优化后)
cd /tmp/bench2

echo "============================================"
echo "  Verify + Benchmark (dual-limb absAdd)"
echo "  Date: $(date '+%Y-%m-%d %H:%M:%S')"
echo "============================================"

echo ""
echo "=== Correctness ==="
echo "--- ADD ---"
for t in test_add_100k_100k.txt test_add_500k_500k.txt test_add_1M_1M.txt big_add_2M_2M.txt; do
    [ -f "$t" ] || continue
    ./moptm_ADD < "$t" > out_m.txt 2>/dev/null
    ./best_ADD < "$t" > out_b.txt 2>/dev/null
    if diff -q out_m.txt out_b.txt > /dev/null 2>&1; then
        echo "  $t: PASS"
    else
        echo "  $t: FAIL"
        diff out_m.txt out_b.txt | head -3
    fi
done

echo "--- MUL ---"
for t in test_mul_100k_100k.txt test_mul_300k_300k.txt test_mul_500k_500k.txt big_mul_2M_2M.txt; do
    [ -f "$t" ] || continue
    ./moptm_MUL < "$t" > out_m.txt 2>/dev/null
    ./best_MUL < "$t" > out_b.txt 2>/dev/null
    if diff -q out_m.txt out_b.txt > /dev/null 2>&1; then
        echo "  $t: PASS"
    else
        echo "  $t: FAIL"
        diff out_m.txt out_b.txt | head -3
    fi
done

echo "--- DIV ---"
for t in test_div_1M_100k.txt test_div_1M_500k.txt test_div_1M_900k.txt test_div_1M_999k.txt big_div_2M_1M.txt big_div_2M_500k.txt big_div_2M_2M.txt; do
    [ -f "$t" ] || continue
    ./moptm_DIV < "$t" > out_m.txt 2>/dev/null
    ./best_DIV < "$t" > out_b.txt 2>/dev/null
    if diff -q out_m.txt out_b.txt > /dev/null 2>&1; then
        echo "  $t: PASS"
    else
        echo "  $t: FAIL"
        diff out_m.txt out_b.txt | head -3
    fi
done

echo ""
echo "=== Performance (clock() CPU time, 10 runs avg) ==="
RUNS=10

bench_clock() {
    local exe=$1 test=$2
    ./$exe < "$test" > /dev/null 2>&1  # warmup
    local total=0
    for i in $(seq 1 $RUNS); do
        # 用 date 纳秒计时 (wall clock) 作为备用
        local ns_start=$(date +%s%N)
        ./$exe < "$test" > /dev/null 2>&1
        local ns_end=$(date +%s%N)
        local elapsed=$((ns_end - ns_start))
        total=$((total + elapsed))
    done
    # 转换为毫秒
    awk -v t=$total -v r=$RUNS 'BEGIN{printf "%.3f", t/r/1000000}'
}

echo "--- ADD ---"
for t in big_add_2M_2M.txt test_add_1M_1M.txt test_add_500k_500k.txt test_add_100k_100k.txt; do
    [ -f "$t" ] || continue
    m=$(bench_clock moptm_ADD "$t")
    b=$(bench_clock best_ADD "$t")
    r=$(awk -v m=$m -v b=$b 'BEGIN{printf "%.2f", m/b}')
    echo "  $t: moptm=${m}ms best=${b}ms ratio=${r}x"
done

echo "--- MUL ---"
for t in big_mul_2M_2M.txt test_mul_500k_500k.txt test_mul_300k_300k.txt test_mul_100k_100k.txt; do
    [ -f "$t" ] || continue
    m=$(bench_clock moptm_MUL "$t")
    b=$(bench_clock best_MUL "$t")
    r=$(awk -v m=$m -v b=$b 'BEGIN{printf "%.2f", m/b}')
    echo "  $t: moptm=${m}ms best=${b}ms ratio=${r}x"
done

echo "--- DIV ---"
for t in big_div_2M_1M.txt big_div_2M_500k.txt big_div_2M_2M.txt test_div_1M_500k.txt test_div_1M_100k.txt test_div_1M_900k.txt test_div_1M_999k.txt; do
    [ -f "$t" ] || continue
    m=$(bench_clock moptm_DIV "$t")
    b=$(bench_clock best_DIV "$t")
    r=$(awk -v m=$m -v b=$b 'BEGIN{printf "%.2f", m/b}')
    echo "  $t: moptm=${m}ms best=${b}ms ratio=${r}x"
done

echo ""
echo "=== Done ==="
