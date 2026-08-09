#!/bin/bash
# vm_fix_absdiv1.sh - Replace cur's absDiv1 with best's simple 32-bit division
cd ~/div_bench

echo "=== Creating cur_div_fix_absdiv1.cpp ==="
python3 << 'PYEOF'
with open('cur_div.cpp') as f:
    code = f.read()

# Find and replace absDiv1 function
old_func = '''        static Limb absDiv1(View in, Limb x, Span out)
        {
            if (x == 1) {
                if (out.ptr != in.ptr) std::memcpy(out.ptr, in.ptr, in.size * sizeof(Limb));
                return 0;
            }
            // SIMD 搬运 + 64 位 Barrett (ceil 乘数 M = ceil(2^64/x))
            uint64_t M = (uint64_t)((((unsigned __int128)1 << 64) + x - 1) / x);
            Limb rem = 0;
            size_t i = in.size;
            while (i >= 8) {
                i -= 8;
                __m128i in16 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in.ptr + i));
                __m256i in32 = _mm256_cvtepu16_epi32(in16);
                alignas(32) uint32_t tmp_in[8], tmp_out[8];
                _mm256_store_si256(reinterpret_cast<__m256i*>(tmp_in), in32);
                for (int k = 7; k >= 0; --k) {
                    uint64_t prod = uint64_t(tmp_in[k]) + uint64_t(rem) * BASE;
                    uint64_t q = (uint64_t)((unsigned __int128)prod * M >> 64);
                    if ((uint64_t)q * x > prod) q--;
                    tmp_out[k] = (uint32_t)q;
                    rem = Limb(prod - q * x);
                }
                for (int k = 0; k < 8; ++k)
                    out.ptr[i + k] = Limb(tmp_out[k]);
            }
            while (i > 0) {
                --i;
                uint64_t prod = uint64_t(in[i]) + uint64_t(rem) * BASE;
                uint64_t q = (uint64_t)((unsigned __int128)prod * M >> 64);
                if ((uint64_t)q * x > prod) q--;
                out[i] = Limb(q);
                rem = Limb(prod - q * x);
            }
            return rem;
        }'''

new_func = '''        static Limb absDiv1(View in, Limb x, Span out)
        {
            Limb rem = 0;
            size_t i = in.size;
            while (i > 0)
            {
                i--;
                Limb2 prod = Limb2(in[i]) + Limb2(rem) * BASE;
                out[i] = prod / x;
                rem = prod % x;
            }
            return rem;
        }'''

if old_func in code:
    code = code.replace(old_func, new_func)
    print('Replaced absDiv1 with simple 32-bit division')
else:
    print('ERROR: Could not find absDiv1 pattern')
    # Try to find it
    idx = code.find('static Limb absDiv1')
    if idx >= 0:
        print(f'Found at pos {idx}')
        print(code[idx:idx+200])
    exit(1)

with open('cur_div_fix_absdiv1.cpp', 'w') as f:
    f.write(code)
print('Created cur_div_fix_absdiv1.cpp')
PYEOF

CXXFLAGS="-std=c++20 -O2 -march=native -DEVAL -DONLINE_JUDGE -DHINT_OP_DIV -I."

echo "Compiling..."
g++ $CXXFLAGS cur_div_fix_absdiv1.cpp -o cur_div_fix_absdiv1 -lpthread 2>&1 | tail -3
echo "RC=$?"

if [ ! -x cur_div_fix_absdiv1 ]; then
    echo "Compilation failed"
    exit 1
fi

echo ""
echo "=== Correctness check (small) ==="
./cur_div_fix_absdiv1 < div_small_0.in > /tmp/fix_out.txt 2>/dev/null
./best_div < div_small_0.in > /tmp/best_out.txt 2>/dev/null
if diff -q /tmp/fix_out.txt /tmp/best_out.txt > /dev/null; then
    echo "PASS: output matches best_div"
else
    echo "FAIL: output mismatch!"
    diff /tmp/fix_out.txt /tmp/best_out.txt | head -5
fi

echo ""
echo "=== Benchmark medium (5 runs, min) ==="
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

echo "--- cur_div_fix_absdiv1 ---"
best_fix=999
for i in 1 2 3 4 5; do
    t0=$(date +%s%N)
    ./cur_div_fix_absdiv1 < div_medium_0.in > /dev/null 2>/dev/null
    t1=$(date +%s%N)
    ms=$(( (t1-t0)/1000000 ))
    echo "  run $i: $ms ms"
    if [ $ms -lt $best_fix ]; then best_fix=$ms; fi
done
echo "  min: $best_fix ms"

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
echo "=== Benchmark small (5 runs, min) ==="
echo "--- cur_div (original) ---"
best_orig=999
for i in 1 2 3 4 5; do
    t0=$(date +%s%N)
    ./cur_div < div_small_0.in > /dev/null 2>/dev/null
    t1=$(date +%s%N)
    ms=$(( (t1-t0)/1000000 ))
    if [ $ms -lt $best_orig ]; then best_orig=$ms; fi
done
echo "  min: $best_orig ms"

echo "--- cur_div_fix_absdiv1 ---"
best_fix=999
for i in 1 2 3 4 5; do
    t0=$(date +%s%N)
    ./cur_div_fix_absdiv1 < div_small_0.in > /dev/null 2>/dev/null
    t1=$(date +%s%N)
    ms=$(( (t1-t0)/1000000 ))
    if [ $ms -lt $best_fix ]; then best_fix=$ms; fi
done
echo "  min: $best_fix ms"

echo ""
echo "=== Benchmark large/xlarge (3 runs, min) ==="
echo "--- cur_div (original) ---"
for f in div_large_0.in div_xlarge_0.in div_xxlarge_0.in div_max_0.in; do
    [ -f "$f" ] || continue
    best_orig=999
    for i in 1 2 3; do
        t0=$(date +%s%N)
        ./cur_div < $f > /dev/null 2>/dev/null
        t1=$(date +%s%N)
        ms=$(( (t1-t0)/1000000 ))
        if [ $ms -lt $best_orig ]; then best_orig=$ms; fi
    done
    echo "  $f: min $best_orig ms"
done

echo "--- cur_div_fix_absdiv1 ---"
for f in div_large_0.in div_xlarge_0.in div_xxlarge_0.in div_max_0.in; do
    [ -f "$f" ] || continue
    best_fix=999
    for i in 1 2 3; do
        t0=$(date +%s%N)
        ./cur_div_fix_absdiv1 < $f > /dev/null 2>/dev/null
        t1=$(date +%s%N)
        ms=$(( (t1-t0)/1000000 ))
        if [ $ms -lt $best_fix ]; then best_fix=$ms; fi
    done
    echo "  $f: min $best_fix ms"
done

echo ""
echo "=== perf stat comparison (medium) ==="
echo "--- cur_div (original) ---"
perf stat -e cycles,instructions,branch-misses ./cur_div < div_medium_0.in > /dev/null 2>&1 || true
perf stat -e cycles,instructions,branch-misses ./cur_div < div_medium_0.in > /dev/null 2>perf_cur_orig.txt
cat perf_cur_orig.txt

echo ""
echo "--- cur_div_fix_absdiv1 ---"
perf stat -e cycles,instructions,branch-misses ./cur_div_fix_absdiv1 < div_medium_0.in > /dev/null 2>perf_cur_fix.txt
cat perf_cur_fix.txt
