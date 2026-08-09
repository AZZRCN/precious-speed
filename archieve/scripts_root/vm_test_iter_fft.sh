#!/bin/bash
# vm_test_iter_fft.sh - Test if HINT_USE_ITER_FFT affects medium performance
cd ~/div_bench

echo "=== Experiment: Remove HINT_USE_ITER_FFT from cur_div ==="
# Create a version without HINT_USE_ITER_FFT
sed 's/#define HINT_USE_ITER_FFT/#undef HINT_USE_ITER_FFT/' cur_div.cpp > cur_div_no_iter.cpp

# Compile both versions
CXXFLAGS="-std=c++20 -O2 -march=native -DEVAL -DONLINE_JUDGE -DHINT_OP_DIV -I."

echo "Compiling cur_div_no_iter..."
g++ $CXXFLAGS cur_div_no_iter.cpp -o cur_div_no_iter -lpthread 2>&1 | tail -3
echo "RC=$?"

echo ""
echo "=== Benchmark medium (3 runs each) ==="
echo "--- cur_div (original) ---"
for i in 1 2 3; do
    t0=$(date +%s%N)
    ./cur_div < div_medium_0.in > /dev/null 2>/dev/null
    t1=$(date +%s%N)
    echo "  run $i: $(( (t1-t0)/1000000 )) ms"
done

echo "--- cur_div_no_iter ---"
for i in 1 2 3; do
    t0=$(date +%s%N)
    ./cur_div_no_iter < div_medium_0.in > /dev/null 2>/dev/null
    t1=$(date +%s%N)
    echo "  run $i: $(( (t1-t0)/1000000 )) ms"
done

echo ""
echo "=== Benchmark small (3 runs each) ==="
echo "--- cur_div (original) ---"
for i in 1 2 3; do
    t0=$(date +%s%N)
    ./cur_div < div_small_0.in > /dev/null 2>/dev/null
    t1=$(date +%s%N)
    echo "  run $i: $(( (t1-t0)/1000000 )) ms"
done

echo "--- cur_div_no_iter ---"
for i in 1 2 3; do
    t0=$(date +%s%N)
    ./cur_div_no_iter < div_small_0.in > /dev/null 2>/dev/null
    t1=$(date +%s%N)
    echo "  run $i: $(( (t1-t0)/1000000 )) ms"
done

echo ""
echo "=== Benchmark xlarge (3 runs each) ==="
echo "--- cur_div (original) ---"
for i in 1 2 3; do
    t0=$(date +%s%N)
    ./cur_div < div_xlarge_0.in > /dev/null 2>/dev/null
    t1=$(date +%s%N)
    echo "  run $i: $(( (t1-t0)/1000000 )) ms"
done

echo "--- cur_div_no_iter ---"
for i in 1 2 3; do
    t0=$(date +%s%N)
    ./cur_div_no_iter < div_xlarge_0.in > /dev/null 2>/dev/null
    t1=$(date +%s%N)
    echo "  run $i: $(( (t1-t0)/1000000 )) ms"
done

echo ""
echo "=== Binary sizes ==="
ls -la cur_div cur_div_no_iter | awk '{print $5, $9}'
