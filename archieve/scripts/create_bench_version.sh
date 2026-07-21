#!/bin/bash
# create_bench_version.sh
# 创建带 CPU 时间计时的 moptm_bench.cpp
cd /tmp/bench2

cp moptm.cpp moptm_bench.cpp

# 在每个 int main() { 后插入计时开始
# 在每个 flushOutput(); 后插入计时输出
# 用 clock() 测 CPU 时间（和 LC 一致）

python3 << 'PYEOF'
with open('moptm_bench.cpp', 'r') as f:
    code = f.read()

# 在所有 "int main() {" 后插入计时开始
code = code.replace(
    'int main() {',
    'int main() { clock_t _bench_t0 = clock();'
)

# 在所有 "flushOutput();" 后插入计时输出
code = code.replace(
    '    flushOutput();\n    return 0;',
    '    flushOutput();\n    fprintf(stderr, "CPU: %.3f ms\\n", double(clock() - _bench_t0) * 1000.0 / CLOCKS_PER_SEC);\n    return 0;'
)

with open('moptm_bench.cpp', 'w') as f:
    f.write(code)

print("OK")
PYEOF

echo "=== verify modifications ==="
grep -n "_bench_t0" moptm_bench.cpp
grep -n "CPU:" moptm_bench.cpp

echo ""
echo "=== compile bench versions ==="
LC_FLAGS="-O2 -std=gnu++20 -static -DONLINE_JUDGE"
g++ ${LC_FLAGS} -DHINT_OP_ADD -o moptm_ADD_bench moptm_bench.cpp && echo "  ADD OK"
g++ ${LC_FLAGS} -DHINT_OP_MUL -o moptm_MUL_bench moptm_bench.cpp && echo "  MUL OK"
g++ ${LC_FLAGS} -DHINT_OP_DIV -o moptm_DIV_bench moptm_bench.cpp && echo "  DIV OK"

# Pragma 版本
sed '1i\#pragma GCC target("avx2,fma")\n#pragma GCC optimize("O3,unroll-loops")' moptm_bench.cpp > moptm_bench_o3avx2.cpp
g++ ${LC_FLAGS} -DHINT_OP_ADD -o moptm_ADD_bench_o3avx2 moptm_bench_o3avx2.cpp && echo "  ADD O3AVX2 OK"
g++ ${LC_FLAGS} -DHINT_OP_MUL -o moptm_MUL_bench_o3avx2 moptm_bench_o3avx2.cpp && echo "  MUL O3AVX2 OK"
g++ ${LC_FLAGS} -DHINT_OP_DIV -o moptm_DIV_bench_o3avx2 moptm_bench_o3avx2.cpp && echo "  DIV O3AVX2 OK"

echo ""
echo "=== quick test ==="
echo "1" | ./moptm_DIV_bench 2>&1
printf '1\n1234567890\n9876543210\n' | ./moptm_DIV_bench 2>&1
