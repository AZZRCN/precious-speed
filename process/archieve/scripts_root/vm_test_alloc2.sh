#!/bin/bash
# vm_test_alloc2.sh - Replace cur's allocator with best's simple one using sed
cd ~/div_bench

echo "=== Creating cur_div_simple_alloc.cpp ==="
# Use Python with more flexible matching
python3 << 'PYEOF'
with open('cur_div.cpp') as f:
    lines = f.readlines()

out = []
i = 0
while i < len(lines):
    line = lines[i]
    # Match the allocator allocate block
    if 'size_t bytes = n * sizeof(T);' in line and i + 10 < len(lines):
        # Skip the entire allocate block until 'return static_cast'
        # Find the end of the block
        j = i
        while j < len(lines) and 'return static_cast' not in lines[j]:
            j += 1
        if j < len(lines):
            # Replace with simple allocator
            indent = '            '
            out.append(indent + 'if (posix_memalign(&p, 32, n * sizeof(T)) != 0)\n')
            out.append(indent + '    throw std::bad_alloc();\n')
            out.append(lines[j])  # return static_cast<T *>(p);
            i = j + 1
            continue
    # Match the deallocate block
    if 'void deallocate(T *p, size_t)' in line and i + 5 < len(lines):
        # Find the closing brace
        j = i
        brace_count = 0
        while j < len(lines):
            brace_count += lines[j].count('{') - lines[j].count('}')
            j += 1
            if brace_count == 0 and j > i + 1:
                break
        if j < len(lines):
            indent = '        '
            out.append(indent + 'void deallocate(T *p, size_t) { free(p); }\n')
            i = j
            continue
    out.append(line)
    i += 1

with open('cur_div_simple_alloc.cpp', 'w') as f:
    f.writelines(out)
print(f'Created cur_div_simple_alloc.cpp ({len(out)} lines)')
PYEOF

CXXFLAGS="-std=c++20 -O2 -march=native -DEVAL -DONLINE_JUDGE -DHINT_OP_DIV -I."

echo "Compiling..."
g++ $CXXFLAGS cur_div_simple_alloc.cpp -o cur_div_simple_alloc -lpthread 2>&1 | tail -3
echo "RC=$?"

if [ ! -x cur_div_simple_alloc ]; then
    echo "Compilation failed"
    exit 1
fi

echo ""
echo "=== Benchmark medium (5 runs each, min) ==="
echo "--- cur_div (original) ---"
best_orig=999
for i in 1 2 3 4 5; do
    t0=$(date +%s%N)
    ./cur_div < div_medium_0.in > /dev/null 2>/dev/null
    t1=$(date +%s%N)
    ms=$(( (t1-t0)/1000000 ))
    echo "  run $i: $ms ms"
    if [ $ms -lt $best_orig ]; then best_orig=$ms; fi
done
echo "  min: $best_orig ms"

echo "--- cur_div_simple_alloc ---"
best_simple=999
for i in 1 2 3 4 5; do
    t0=$(date +%s%N)
    ./cur_div_simple_alloc < div_medium_0.in > /dev/null 2>/dev/null
    t1=$(date +%s%N)
    ms=$(( (t1-t0)/1000000 ))
    echo "  run $i: $ms ms"
    if [ $ms -lt $best_simple ]; then best_simple=$ms; fi
done
echo "  min: $best_simple ms"

echo "--- best_div ---"
best_best=999
for i in 1 2 3 4 5; do
    t0=$(date +%s%N)
    ./best_div < div_medium_0.in > /dev/null 2>/dev/null
    t1=$(date +%s%N)
    ms=$(( (t1-t0)/1000000 ))
    echo "  run $i: $ms ms"
    if [ $ms -lt $best_best ]; then best_best=$ms; fi
done
echo "  min: $best_best ms"

echo ""
echo "=== perf stat comparison ==="
echo "--- cur_div (original) ---"
perf stat -e cycles,instructions,cache-misses,branch-misses ./cur_div < div_medium_0.in > /dev/null 2>perf_cur_orig.txt
cat perf_cur_orig.txt

echo ""
echo "--- cur_div_simple_alloc ---"
perf stat -e cycles,instructions,cache-misses,branch-misses ./cur_div_simple_alloc < div_medium_0.in > /dev/null 2>perf_cur_simple.txt
cat perf_cur_simple.txt

echo ""
echo "--- best_div ---"
perf stat -e cycles,instructions,cache-misses,branch-misses ./best_div < div_medium_0.in > /dev/null 2>perf_best.txt
cat perf_best.txt

echo ""
echo "=== Binary sizes ==="
ls -la cur_div cur_div_simple_alloc best_div | awk '{print $5, $9}'
