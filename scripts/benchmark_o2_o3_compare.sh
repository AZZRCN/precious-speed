#!/bin/bash
# benchmark_o2_o3_compare.sh
# 对比 fusion_o2only / fusion(v2,pragma O3) / moptm / best 在 -O2 和 -O3 下的性能
# 三题：ADD 1M+1M, MUL 500k*500k, DIV 1M/500k
# 每个组合跑 5 次取中位数

set -e
cd /tmp/o2compare
mkdir -p /tmp/o2compare

run_bench() {
    local exe=$1
    local infile=$2
    local runs=5
    local times=()
    for i in $(seq 1 $runs); do
        local t=$( { /usr/bin/time -f "%e" $exe < $infile > /tmp/o2compare/out.txt 2>&1; } 2>&1 | tail -1 )
        # 验证输出非空
        if [ ! -s /tmp/o2compare/out.txt ]; then
            echo "FAIL: empty output"
            return 1
        fi
        times+=($t)
    done
    # 取中位数
    printf '%s\n' "${times[@]}" | sort -n | awk -v runs=$runs 'NR==int((runs+1)/2){print}'
}

echo "=== O2 vs O3 Comparison Benchmark ==="
echo "Date: $(date)"
echo "VM: $(hostname), g++ $(g++ --version | head -1)"
echo ""

# 准备测试数据
echo "Preparing test data..."
python3 /tmp/fusion/gen_data.py 2>/dev/null || true
ls -la /tmp/fusion/*.in 2>/dev/null | head -10

# 测试矩阵
for src in fusion_o2only fusion moptm; do
    for opt in O2 O3; do
        for op in ADD MUL DIV; do
            srcfile=/tmp/o2compare/${src}.cpp
            if [ ! -f $srcfile ]; then
                echo "SKIP: $srcfile not found"
                continue
            fi
            exe=/tmp/o2compare/${src}_${opt}_${op}
            echo -n "Compiling $src $opt $op... "
            g++ -${opt} -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_${op} $srcfile -o $exe 2>/tmp/o2compare/err.log || {
                echo "COMPILE FAIL"
                cat /tmp/o2compare/err.log
                continue
            }
            echo "OK"
        done
    done
done

# best 三个文件单独编译
for op in ADD MUL DIV; do
    srcfile=/tmp/o2compare/best_${op,,}.cpp
    if [ ! -f $srcfile ]; then
        echo "SKIP: $srcfile not found"; continue
    fi
    for opt in O2 O3; do
        exe=/tmp/o2compare/best_${opt}_${op}
        echo -n "Compiling best $opt $op... "
        g++ -${opt} -std=gnu++20 -static -DONLINE_JUDGE $srcfile -o $exe 2>/tmp/o2compare/err.log || {
            echo "COMPILE FAIL"
            continue
        }
        echo "OK"
    done
done

echo ""
echo "=== Benchmark Results (median of 5 runs, seconds) ==="
printf "%-25s %-8s %-8s %-8s\n" "Binary" "ADD_1M" "MUL_500k" "DIV_1M_500k"
echo "-----------------------------------------------------------------"

for src in fusion_o2only fusion moptm; do
    for opt in O2 O3; do
        add_t=$(run_bench /tmp/o2compare/${src}_${opt}_ADD /tmp/fusion/add_1M.in 2>/dev/null || echo "N/A")
        mul_t=$(run_bench /tmp/o2compare/${src}_${opt}_MUL /tmp/fusion/mul_500k.in 2>/dev/null || echo "N/A")
        div_t=$(run_bench /tmp/o2compare/${src}_${opt}_DIV /tmp/fusion/div_1M_500k.in 2>/dev/null || echo "N/A")
        printf "%-25s %-8s %-8s %-8s\n" "${src}_${opt}" "$add_t" "$mul_t" "$div_t"
    done
done

for opt in O2 O3; do
    add_t=$(run_bench /tmp/o2compare/best_${opt}_ADD /tmp/fusion/add_1M.in 2>/dev/null || echo "N/A")
    mul_t=$(run_bench /tmp/o2compare/best_${opt}_MUL /tmp/fusion/mul_500k.in 2>/dev/null || echo "N/A")
    div_t=$(run_bench /tmp/o2compare/best_${opt}_DIV /tmp/fusion/div_1M_500k.in 2>/dev/null || echo "N/A")
    printf "%-25s %-8s %-8s %-8s\n" "best_${opt}" "$add_t" "$mul_t" "$div_t"
done

echo ""
echo "=== Done ==="
