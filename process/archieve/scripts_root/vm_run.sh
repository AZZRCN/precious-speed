#!/bin/bash
# vm_run.sh - Complete VM benchmark pipeline
# 1. Compile all exes (best/cur × add/mul/div) with LC O2 flags
# 2. Compile vm_bench driver
# 3. Run vm_bench
# 4. Run perf stat on key cases (if perf available)

set -e
cd ~/div_bench

echo "============================================================"
echo "VM Benchmark Pipeline (LC: O2 -march=native)"
echo "============================================================"
echo "CPU: $(nproc) cores, $(free -h | awk '/^内存/{print $2}') RAM"
echo "GCC: $(g++ --version | head -1)"
echo

# LC compile flags
CXXFLAGS="-std=c++20 -O2 -march=native -DEVAL -DONLINE_JUDGE -I."
LDFLAGS="-lpthread"

echo "=== Step 1: Compile 6 executables ==="
echo -n "  best_add... "; g++ $CXXFLAGS best_add.cpp -o best_add $LDFLAGS && echo OK || echo FAIL
echo -n "  best_mul... "; g++ $CXXFLAGS best_mul.cpp -o best_mul $LDFLAGS && echo OK || echo FAIL
echo -n "  best_div... "; g++ $CXXFLAGS best_div.cpp -o best_div $LDFLAGS && echo OK || echo FAIL
echo -n "  cur_add...  "; g++ $CXXFLAGS cur_add.cpp  -o cur_add  $LDFLAGS && echo OK || echo FAIL
echo -n "  cur_mul...  "; g++ $CXXFLAGS cur_mul.cpp  -o cur_mul  $LDFLAGS && echo OK || echo FAIL
echo -n "  cur_div...  "; g++ $CXXFLAGS cur_div.cpp  -o cur_div  $LDFLAGS && echo OK || echo FAIL
echo

echo "=== Step 2: Compile vm_bench driver ==="
echo -n "  vm_bench... "; g++ -std=c++20 -O2 vm_bench.cpp -o vm_bench && echo OK || echo FAIL
echo

echo "=== Step 3: Run benchmark (fork+exec+clock_gettime) ==="
./vm_bench 5
echo

echo "=== Step 4: perf stat (if available) ==="
if command -v perf &>/dev/null; then
    echo "  perf available, running on div_max_0..."
    echo "  --- best_div div_max_0 ---"
    perf stat -e cycles,instructions,cache-misses,branch-misses ./best_div < div_max_0.in > /dev/null 2>&1 || true
    perf stat -e cycles,instructions,cache-misses,branch-misses ./best_div < div_max_0.in > /dev/null
    echo
    echo "  --- cur_div div_max_0 ---"
    perf stat -e cycles,instructions,cache-misses,branch-misses ./cur_div < div_max_0.in > /dev/null 2>&1 || true
    perf stat -e cycles,instructions,cache-misses,branch-misses ./cur_div < div_max_0.in > /dev/null
else
    echo "  perf not available, skipping"
fi

echo
echo "============================================================"
echo "Done."
