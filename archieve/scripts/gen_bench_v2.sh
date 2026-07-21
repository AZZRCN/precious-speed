#!/bin/bash
# gen_bench_v2.sh
# 生成带 clock() 计时的 bench 版本并运行精确对比
cd /tmp/bench2

echo "=== Generating bench versions with clock() ==="

# 给 moptm.cpp 添加 clock() 计时
python3 -c "
import re
with open('moptm.cpp', 'r') as f:
    code = f.read()

# 在每个 main 函数开头插入 clock_t _bench_t0 = clock();
# main 函数特征: int main() 或 int main(int argc, ...)
code = re.sub(r'int main\(\)\s*\{', 'int main() { clock_t _bench_t0 = clock();', code)

# 在 flushOutput() 后插入计时输出
# 查找 flushOutput(); 后面可能跟 return 0; 或 #ifdef
# 用更通用的方式: 在 return 0; 前插入
# 但 ADD main 的 flushOutput 后跟 #ifdef PROFILE_DIV
# 所以用两种模式匹配

# 模式1: flushOutput();\n    return 0;
code = re.sub(
    r'flushOutput\(\);\s*\n(\s*)return 0;',
    r'flushOutput();\n\1fprintf(stderr, \"CPU: %.3f ms\\n\", double(clock() - _bench_t0) * 1000.0 / CLOCKS_PER_SEC);\n\1return 0;',
    code
)

# 模式2: flushOutput();\n#ifdef PROFILE_DIV (ADD main)
code = re.sub(
    r'flushOutput\(\);\s*\n#ifdef PROFILE_DIV',
    r'flushOutput();\n    fprintf(stderr, \"CPU: %.3f ms\\n\", double(clock() - _bench_t0) * 1000.0 / CLOCKS_PER_SEC);\n#ifdef PROFILE_DIV',
    code
)

with open('moptm_bench.cpp', 'w') as f:
    f.write(code)
print('moptm_bench.cpp generated')
"

# 编译 bench 版本
FLAGS="-O2 -std=gnu++20 -static -DONLINE_JUDGE"
g++ $FLAGS -DHINT_OP_ADD -o moptm_ADD_bench moptm_bench.cpp && echo "moptm_ADD_bench OK"
g++ $FLAGS -DHINT_OP_MUL -o moptm_MUL_bench moptm_bench.cpp && echo "moptm_MUL_bench OK"
g++ $FLAGS -DHINT_OP_DIV -o moptm_DIV_bench moptm_bench.cpp && echo "moptm_DIV_bench OK"

echo ""
echo "=== Precise CPU Benchmark (clock(), 10 runs avg) ==="
RUNS=10

bench_cpu() {
    local exe=$1 test=$2
    ./$exe < "$test" > /dev/null 2>&1  # warmup
    local total=0
    for i in $(seq 1 $RUNS); do
        local t=$(./$exe < "$test" 2>&1 1>/dev/null | awk '/CPU:/{print $2}')
        if [ -z "$t" ]; then
            t=0
        fi
        total=$(awk -v tot=$total -v n=$t 'BEGIN{print tot+n}')
    done
    awk -v t=$total -v r=$RUNS 'BEGIN{printf "%.3f", t/r}'
}

bench_best() {
    local exe=$1 test=$2
    ./$exe < "$test" > /dev/null 2>&1  # warmup, ignore segfault
    local total=0
    for i in $(seq 1 $RUNS); do
        local t=$(./$exe < "$test" 2>&1 1>/dev/null | awk '/BEST_CPU:/{print $2}' || true)
        if [ -z "$t" ]; then
            t=0
        fi
        total=$(awk -v tot=$total -v n=$t 'BEGIN{print tot+n}')
    done
    awk -v t=$total -v r=$RUNS 'BEGIN{printf "%.3f", t/r}'
}

echo "--- ADD ---"
for t in big_add_2M_2M.txt test_add_1M_1M.txt test_add_500k_500k.txt test_add_100k_100k.txt; do
    [ -f "$t" ] || continue
    m=$(bench_cpu moptm_ADD_bench "$t")
    b=$(bench_best best_ADD "$t")
    r=$(awk -v m=$m -v b=$b 'BEGIN{printf "%.2f", m/b}')
    echo "  $t: moptm=${m}ms best=${b}ms ratio=${r}x"
done

echo "--- MUL ---"
for t in big_mul_2M_2M.txt test_mul_500k_500k.txt test_mul_300k_300k.txt test_mul_100k_100k.txt; do
    [ -f "$t" ] || continue
    m=$(bench_cpu moptm_MUL_bench "$t")
    b=$(bench_best best_MUL "$t")
    r=$(awk -v m=$m -v b=$b 'BEGIN{printf "%.2f", m/b}')
    echo "  $t: moptm=${m}ms best=${b}ms ratio=${r}x"
done

echo "--- DIV ---"
for t in big_div_2M_1M.txt big_div_2M_500k.txt big_div_2M_2M.txt test_div_1M_500k.txt test_div_1M_100k.txt test_div_1M_900k.txt test_div_1M_999k.txt; do
    [ -f "$t" ] || continue
    m=$(bench_cpu moptm_DIV_bench "$t")
    b=$(bench_best best_DIV "$t")
    r=$(awk -v m=$m -v b=$b 'BEGIN{printf "%.2f", m/b}')
    echo "  $t: moptm=${m}ms best=${b}ms ratio=${r}x"
done

echo ""
echo "=== Done ==="
