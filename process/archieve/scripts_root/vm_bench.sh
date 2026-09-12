#!/bin/bash
# vm_bench.sh - Compile and benchmark on VM (Linux, simulates LC environment)
# Compiles best/cur × add/mul/div with -O2 -march=native (LC flags)
# Runs each test 3 times, takes min

set -e
cd ~/div_bench

echo "============================================================"
echo "VM Benchmark: best VS cur (O2 -march=native, LC flags)"
echo "============================================================"
echo "CPU: $(nproc) cores, $(free -h | awk '/^内存/{print $2}') RAM"
echo "GCC: $(g++ --version | head -1)"
echo

# LC compile flags (from docs/limits.md)
CXXFLAGS="-std=c++20 -O2 -march=native -DEVAL -DONLINE_JUDGE -I."
LDFLAGS="-lpthread"

echo "=== Step 1: Compile 6 executables ==="
echo "  Flags: $CXXFLAGS"
echo

# best (no shim needed on Linux)
echo -n "  best_add... "; g++ $CXXFLAGS best_add.cpp -o best_add $LDFLAGS && echo OK || echo FAIL
echo -n "  best_mul... "; g++ $CXXFLAGS best_mul.cpp -o best_mul $LDFLAGS && echo OK || echo FAIL
echo -n "  best_div... "; g++ $CXXFLAGS best_div.cpp -o best_div $LDFLAGS && echo OK || echo FAIL
# cur
echo -n "  cur_add...  "; g++ $CXXFLAGS cur_add.cpp  -o cur_add  $LDFLAGS && echo OK || echo FAIL
echo -n "  cur_mul...  "; g++ $CXXFLAGS cur_mul.cpp  -o cur_mul  $LDFLAGS && echo OK || echo FAIL
echo -n "  cur_div...  "; g++ $CXXFLAGS cur_div.cpp  -o cur_div  $LDFLAGS && echo OK || echo FAIL
echo

echo "=== Step 2: Run benchmarks (3 runs each, min) ==="
echo

run_bench() {
    local exe=$1
    local input=$2
    local label=$3
    if [ ! -x "$exe" ]; then
        echo "  $label: SKIP (no exe)"
        return
    fi
    # Warm up
    ./"$exe" < "$input" > /dev/null 2>&1 || true
    # 3 timed runs
    local t1 t2 t3
    t1=$( { /usr/bin/time -f "%e" ./"$exe" < "$input" > /dev/null; } 2>&1 )
    t2=$( { /usr/bin/time -f "%e" ./"$exe" < "$input" > /dev/null; } 2>&1 )
    t3=$( { /usr/bin/time -f "%e" ./"$exe" < "$input" > /dev/null; } 2>&1 )
    # min
    local min=$t1
    if (( $(echo "$t2 < $min" | bc -l) )); then min=$t2; fi
    if (( $(echo "$t3 < $min" | bc -l) )); then min=$t3; fi
    # ms
    local ms=$(echo "$min * 1000" | bc -l)
    printf "  %-25s: %8.1f ms  (runs: %.0f / %.0f / %.0f ms)\n" "$label" "$ms" "$(echo "$t1*1000"|bc -l)" "$(echo "$t2*1000"|bc -l)" "$(echo "$t3*1000"|bc -l)"
}

echo "--- ADD ---"
run_bench best_add add_max_0.in "best_add max_0"
run_bench cur_add  add_max_0.in "cur_add  max_0"
echo

echo "--- MUL ---"
run_bench best_mul mul_max_0.in "best_mul max_0"
run_bench cur_mul  mul_max_0.in "cur_mul  max_0"
echo

echo "--- DIV ---"
run_bench best_div div_max_0.in "best_div max_0 (A=B)"
run_bench cur_div  div_max_0.in "cur_div  max_0 (A=B)"
run_bench best_div div_max_2.in "best_div max_2 (A<B)"
run_bench cur_div  div_max_2.in "cur_div  max_2 (A<B)"
run_bench best_div div_medium_0.in "best_div medium_0"
run_bench cur_div  div_medium_0.in "cur_div  medium_0"
run_bench best_div div_large_0.in "best_div large_0"
run_bench cur_div  div_large_0.in "cur_div  large_0"

echo
echo "============================================================"
echo "Done."
