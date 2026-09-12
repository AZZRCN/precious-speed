#!/bin/bash
# vm_verify_fix.sh - Verify absDiv1 fix: correctness + full benchmark
cd ~/div_bench

echo "=== Recompile cur_div_pure ==="
g++ -std=c++20 -O2 -march=native -DEVAL -DONLINE_JUDGE -DHINT_OP_DIV -DBENCH_DIV_PURE -I. cur_div.cpp -o cur_div_pure -lpthread 2>&1 | tail -2
echo "RC=$?"

echo ""
echo "=== Correctness check ==="
echo "--- small ---"
./cur_div < div_small_0.in > /tmp/cur_out.txt 2>/dev/null
./best_div < div_small_0.in > /tmp/best_out.txt 2>/dev/null
if diff -q /tmp/cur_out.txt /tmp/best_out.txt > /dev/null; then
    echo "  PASS"
else
    echo "  FAIL"
    diff /tmp/cur_out.txt /tmp/best_out.txt | head -3
fi

echo "--- medium ---"
./cur_div < div_medium_0.in > /tmp/cur_out.txt 2>/dev/null
./best_div < div_medium_0.in > /tmp/best_out.txt 2>/dev/null
if diff -q /tmp/cur_out.txt /tmp/best_out.txt > /dev/null; then
    echo "  PASS"
else
    echo "  FAIL"
fi

echo "--- large ---"
./cur_div < div_large_0.in > /tmp/cur_out.txt 2>/dev/null
./best_div < div_large_0.in > /tmp/best_out.txt 2>/dev/null
if diff -q /tmp/cur_out.txt /tmp/best_out.txt > /dev/null; then
    echo "  PASS"
else
    echo "  FAIL"
fi

echo "--- xlarge ---"
./cur_div < div_xlarge_0.in > /tmp/cur_out.txt 2>/dev/null
./best_div < div_xlarge_0.in > /tmp/best_out.txt 2>/dev/null
if diff -q /tmp/cur_out.txt /tmp/best_out.txt > /dev/null; then
    echo "  PASS"
else
    echo "  FAIL"
fi

echo "--- xxlarge ---"
./cur_div < div_xxlarge_0.in > /tmp/cur_out.txt 2>/dev/null
./best_div < div_xxlarge_0.in > /tmp/best_out.txt 2>/dev/null
if diff -q /tmp/cur_out.txt /tmp/best_out.txt > /dev/null; then
    echo "  PASS"
else
    echo "  FAIL"
fi

echo "--- max_0 ---"
./cur_div < div_max_0.in > /tmp/cur_out.txt 2>/dev/null
./best_div < div_max_0.in > /tmp/best_out.txt 2>/dev/null
if diff -q /tmp/cur_out.txt /tmp/best_out.txt > /dev/null; then
    echo "  PASS"
else
    echo "  FAIL"
fi

echo ""
echo "=== Full benchmark (5 runs, min) ==="
printf "%-25s %8s %8s %8s %8s\n" "test" "cur(ms)" "best(ms)" "diff" "cur_vs%"
printf "%-25s %8s %8s %8s %8s\n" "----" "-------" "--------" "----" "-------"

for entry in "div_small_0.in|small" "div_medium_0.in|medium" "div_large_0.in|large" "div_xlarge_0.in|xlarge" "div_xxlarge_0.in|xxlarge" "div_max_0.in|max_eq" "div_max_2.in|max_lt"; do
    IFS='|' read -r infile label <<< "$entry"
    [ -f "$infile" ] || { echo "  SKIP $label (no file)"; continue; }

    best_cur=999
    for i in 1 2 3 4 5; do
        t0=$(date +%s%N)
        ./cur_div < $infile > /dev/null 2>/dev/null
        t1=$(date +%s%N)
        ms=$(( (t1-t0)/1000000 ))
        if [ $ms -lt $best_cur ]; then best_cur=$ms; fi
    done

    best_best=999
    for i in 1 2 3 4 5; do
        t0=$(date +%s%N)
        ./best_div < $infile > /dev/null 2>/dev/null
        t1=$(date +%s%N)
        ms=$(( (t1-t0)/1000000 ))
        if [ $ms -lt $best_best ]; then best_best=$ms; fi
    done

    diff_ms=$((best_cur - best_best))
    pct=$(awk "BEGIN{printf \"%+.1f\", ($best_cur/$best_best - 1) * 100}")
    printf "%-25s %8d %8d %8d %7s%%\n" "$label" "$best_cur" "$best_best" "$diff_ms" "$pct"
done

echo ""
echo "=== perf stat comparison (medium) ==="
echo "--- cur_div (fixed) ---"
perf stat -e cycles,instructions,cache-misses,branch-misses ./cur_div < div_medium_0.in > /dev/null 2>&1 || true
perf stat -e cycles,instructions,cache-misses,branch-misses ./cur_div < div_medium_0.in > /dev/null 2>perf_cur_fixed.txt
cat perf_cur_fixed.txt

echo ""
echo "--- best_div ---"
perf stat -e cycles,instructions,cache-misses,branch-misses ./best_div < div_medium_0.in > /dev/null 2>perf_best_new.txt
cat perf_best_new.txt

echo ""
echo "=== Pure div time comparison ==="
echo "--- cur_div_pure (medium) ---"
./cur_div_pure < div_medium_0.in > /dev/null 2>cur_pure_new.txt
cat cur_pure_new.txt

echo ""
echo "--- best_div_pure (medium) ---"
./best_div_pure < div_medium_0.in > /dev/null 2>best_pure_new.txt
cat best_pure_new.txt
