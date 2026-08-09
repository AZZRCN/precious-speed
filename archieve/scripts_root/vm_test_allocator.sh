#!/bin/bash
# vm_test_allocator.sh - Test if cur's huge-page allocator affects medium performance
cd ~/div_bench

echo "=== Experiment: Replace cur's allocator with best's simple allocator ==="
# Create a version with best's simple allocator
python3 -c "
import re
with open('cur_div.cpp') as f:
    code = f.read()

# Replace the allocator's allocate method
old_alloc = '''            size_t bytes = n * sizeof(T);
#ifdef _WIN32
            p = _aligned_malloc(bytes, 32);
            if (!p) throw std::bad_alloc();
#else
            if (bytes >= 2 * 1024 * 1024)
            {
                if (posix_memalign(&p, 2 * 1024 * 1024, bytes) != 0)
                    throw std::bad_alloc();
                madvise(p, bytes, MADV_HUGEPAGE);
            }
            else
            {
                if (posix_memalign(&p, 32, bytes) != 0)
                    throw std::bad_alloc();
            }
#endif'''

new_alloc = '''            if (posix_memalign(&p, 32, n * sizeof(T)) != 0)
                throw std::bad_alloc();'''

if old_alloc in code:
    code = code.replace(old_alloc, new_alloc)
    print('Replaced allocator allocate method')
else:
    print('ERROR: Could not find allocator pattern')
    exit(1)

# Replace the allocator's deallocate method
old_dealloc = '''        void deallocate(T *p, size_t) {
#ifdef _WIN32
            _aligned_free(p);
#else
            free(p);
#endif
        }'''

new_dealloc = '''        void deallocate(T *p, size_t) { free(p); }'''

if old_dealloc in code:
    code = code.replace(old_dealloc, new_dealloc)
    print('Replaced allocator deallocate method')
else:
    print('ERROR: Could not find deallocator pattern')
    exit(1)

with open('cur_div_simple_alloc.cpp', 'w') as f:
    f.write(code)
print('Created cur_div_simple_alloc.cpp')
"

CXXFLAGS="-std=c++20 -O2 -march=native -DEVAL -DONLINE_JUDGE -DHINT_OP_DIV -I."

echo "Compiling cur_div_simple_alloc..."
g++ $CXXFLAGS cur_div_simple_alloc.cpp -o cur_div_simple_alloc -lpthread 2>&1 | tail -3
echo "RC=$?"

echo ""
echo "=== Benchmark medium (5 runs each, min) ==="
echo "--- cur_div (original) ---"
best=999
for i in 1 2 3 4 5; do
    t0=$(date +%s%N)
    ./cur_div < div_medium_0.in > /dev/null 2>/dev/null
    t1=$(date +%s%N)
    ms=$(( (t1-t0)/1000000 ))
    echo "  run $i: $ms ms"
    if [ $ms -lt $best ]; then best=$ms; fi
done
echo "  min: $best ms"

echo "--- cur_div_simple_alloc ---"
best=999
for i in 1 2 3 4 5; do
    t0=$(date +%s%N)
    ./cur_div_simple_alloc < div_medium_0.in > /dev/null 2>/dev/null
    t1=$(date +%s%N)
    ms=$(( (t1-t0)/1000000 ))
    echo "  run $i: $ms ms"
    if [ $ms -lt $best ]; then best=$ms; fi
done
echo "  min: $best ms"

echo ""
echo "=== perf stat comparison on medium ==="
echo "--- cur_div (original) ---"
perf stat -e cycles,instructions,L1-dcache-load-misses,iTLB-load-misses,icacheMisses ./cur_div < div_medium_0.in > /dev/null 2>&1 || true
perf stat -e cycles,instructions,cache-misses,branch-misses ./cur_div < div_medium_0.in > /dev/null 2>perf_cur_orig.txt
cat perf_cur_orig.txt

echo ""
echo "--- cur_div_simple_alloc ---"
perf stat -e cycles,instructions,cache-misses,branch-misses ./cur_div_simple_alloc < div_medium_0.in > /dev/null 2>perf_cur_simple.txt
cat perf_cur_simple.txt

echo ""
echo "=== Binary sizes ==="
ls -la cur_div cur_div_simple_alloc best_div | awk '{print $5, $9}'
