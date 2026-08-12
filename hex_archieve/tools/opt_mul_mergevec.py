import vm, os

LOCAL = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "submit_ready", "mul.cpp"))
src = open(LOCAL).read()

old = (
    "static void merge_b2(u64* f, const double* g, size_t n, int k) {\n"
    "    size_t i = 0, j = 0;\n"
    "    int w = 0;\n"
    "    u128 tmp = 0;\n"
    "    while (i < n) {\n"
    "        while (w < 64) { tmp += (u128)(u64)(int64_t)(g[j++] + 0.5) << w; w += k; }\n"
    "        f[i++] = (u64)tmp;\n"
    "        tmp >>= 64, w -= 64;\n"
    "    }\n"
    "}\n"
)
new = (
    "static void merge_b2(u64* f, const double* g, size_t n, int k) {\n"
    "    size_t i = 0, j = 0;\n"
    "    u128 tmp = 0;\n"
    "    int w = 0;\n"
    "    const size_t ndig = (n * 64u + (size_t)k - 1) / (size_t)k;\n"
    "    u64 buf[4]; size_t bc = 0, bi = 0;\n"
    "    auto refill = [&]() {\n"
    "        size_t take = ndig - j; if (take > 4) take = 4;\n"
    "        if (take == 4) {\n"
    "            __m256d v = _mm256_loadu_pd(g + j);\n"
    "            __m128i i32 = _mm256_cvttpd_epi32(_mm256_add_pd(v, _mm256_set1_pd(0.5)));\n"
    "            buf[0] = (u64)(u32)_mm_cvtsi128_si64(i32);\n"
    "            buf[1] = (u64)(u32)_mm_extract_epi32(i32, 1);\n"
    "            buf[2] = (u64)(u32)_mm_extract_epi32(i32, 2);\n"
    "            buf[3] = (u64)(u32)_mm_extract_epi32(i32, 3);\n"
    "        } else { for (size_t t = 0; t < take; ++t) buf[t] = (u64)(g[j + t] + 0.5); }\n"
    "        j += take; bc = take; bi = 0;\n"
    "    };\n"
    "    while (i < n) {\n"
    "        while (w < 64) { if (bc == 0) refill(); tmp += (u128)buf[bi++] << w; w += k; --bc; }\n"
    "        f[i++] = (u64)tmp;\n"
    "        tmp >>= 64; w -= 64;\n"
    "    }\n"
    "}\n"
)
assert old in src, "merge_b2 pattern not found"
patched = src.replace(old, new)
open("/tmp/mul_mergevec.cpp", "w").write(patched)
vm.put("/tmp/mul_mergevec.cpp", "/tmp/mul_mergevec.cpp")

cmd = r'''
cd /tmp
CK=~/hexbench/oracle/hexcheck.py
echo "=== build mergevec ==="
g++ -O2 -march=x86-64-v3 -fno-inline-small-functions -fno-inline -g -o mul_mergevec mul_mergevec.cpp 2>&1 | tail -2
echo "=== 22/22 verify mergevec ==="
ok=0; bad=0
for f in ~/hexbench/data/mul/*.in; do e="${f%.in}.exp"; r=$(python3 "$CK" run ./mul_mergevec "$f" "$e" 2>&1 | tail -1); if echo "$r" | grep -q OK; then ok=$((ok+1)); else bad=$((bad+1)); echo "FAIL $(basename $f): $r"; fi; done
echo "MERGEVEC_OK=$ok MERGEVEC_BAD=$bad"
echo "=== perf stat max_max_00 ==="
perf stat -e instructions:u -- ./mul_mergevec < ~/hexbench/data/mul/max_max_00.in > /dev/null 2>sm.txt; grep instructions sm.txt
echo "=== perf stat small_00 ==="
perf stat -e instructions:u -- ./mul_mergevec < ~/hexbench/data/mul/small_00.in > /dev/null 2>ss.txt; grep instructions ss.txt
echo "=== perf report max_max_00 ==="
perf record -e instructions:u -F 2000 -o /tmp/mul_mv.data -- bash -c 'for i in $(seq 20); do ./mul_mergevec < ~/hexbench/data/mul/max_max_00.in > /dev/null; done' 2>&1 | tail -1
perf report -i /tmp/mul_mv.data --stdio --no-children --percent-limit=0.3 2>/dev/null | head -16
'''
rc, out, err = vm.run(cmd, timeout=300)
print("RC", rc)
print(out)
if err.strip():
    print("ERR", err[:800])
