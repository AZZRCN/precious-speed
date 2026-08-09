#!/bin/bash
# vm_prof_stages.sh - Stage-level profiling for cur_div vs best_div on medium
cd ~/div_bench

echo "=== Stage 1: Pure div time on medium ==="
echo "--- cur_div_pure (medium) ---"
./cur_div_pure < div_medium_0.in > /dev/null 2>cur_pure_medium.txt
cat cur_pure_medium.txt

echo ""
echo "--- best_div_pure (medium) ---"
if [ -x best_div_pure ]; then
    ./best_div_pure < div_medium_0.in > /dev/null 2>best_pure_medium.txt
    cat best_pure_medium.txt
else
    echo "best_div_pure not found, trying to compile..."
    g++ -std=c++20 -O2 -march=native -DEVAL -DONLINE_JUDGE -DBENCH_DIV_PURE -I. best_div.cpp -o best_div_pure -lpthread 2>&1 | tail -3
    if [ -x best_div_pure ]; then
        ./best_div_pure < div_medium_0.in > /dev/null 2>best_pure_medium.txt
        cat best_pure_medium.txt
    else
        echo "compile failed"
    fi
fi

echo ""
echo "=== Stage 2: Pure div time on small ==="
echo "--- cur_div_pure (small) ---"
./cur_div_pure < div_small_0.in > /dev/null 2>cur_pure_small.txt
cat cur_pure_small.txt

echo ""
echo "--- best_div_pure (small) ---"
if [ -x best_div_pure ]; then
    ./best_div_pure < div_small_0.in > /dev/null 2>best_pure_small.txt
    cat best_pure_small.txt
fi

echo ""
echo "=== Stage 3: End-to-end time comparison ==="
echo "--- cur_div (medium, 3 runs) ---"
for i in 1 2 3; do
    t0=$(date +%s%N)
    ./cur_div < div_medium_0.in > /dev/null 2>/dev/null
    t1=$(date +%s%N)
    echo "  run $i: $(( (t1-t0)/1000000 )) ms"
done

echo ""
echo "--- best_div (medium, 3 runs) ---"
for i in 1 2 3; do
    t0=$(date +%s%N)
    ./best_div < div_medium_0.in > /dev/null 2>/dev/null
    t1=$(date +%s%N)
    echo "  run $i: $(( (t1-t0)/1000000 )) ms"
done

echo ""
echo "=== Stage 4: Pure div time on large/xlarge ==="
echo "--- cur_div_pure (large) ---"
./cur_div_pure < div_large_0.in > /dev/null 2>cur_pure_large.txt
cat cur_pure_large.txt

echo ""
echo "--- cur_div_pure (xlarge) ---"
./cur_div_pure < div_xlarge_0.in > /dev/null 2>cur_pure_xlarge.txt
cat cur_pure_xlarge.txt

echo ""
echo "--- best_div_pure (large) ---"
if [ -x best_div_pure ]; then
    ./best_div_pure < div_large_0.in > /dev/null 2>best_pure_large.txt
    cat best_pure_large.txt
fi
