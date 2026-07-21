#!/bin/bash
# 完整 benchmark: O2/O3 下所有版本, 5 题中位数, 原始日志输出
set -e
cd /tmp/bench
mkdir -p logs

LC_FLAGS="-std=gnu++20 -static -DONLINE_JUDGE"
TS=$(date +%Y%m%d_%H%M%S)
OUT_LOG="logs/full_bench_${TS}.log"

echo "=== Full Benchmark $(date) ===" | tee $OUT_LOG
echo "VM: Ubuntu $(lsb_release -rs), g++ $(g++ --version | head -1)" | tee -a $OUT_LOG
echo "CPU: $(grep 'model name' /proc/cpuinfo | head -1 | cut -d: -f2 | xargs)" | tee -a $OUT_LOG
echo "" | tee -a $OUT_LOG

# 生成缺失的 100k 数据
if [ ! -f add_100k.txt ] || [ ! -f mul_100k.txt ]; then
    echo "生成 100k 数据..." | tee -a $OUT_LOG
    python3 gen_small.py
fi

# 定义版本: name|src|extra_flags
VERSIONS=(
    "moptm_fusion_O2|moptm_fusion.cpp|-O2"
    "moptm_fusion_O3|moptm_fusion.cpp|-O3"
    "fusion_o2only_O2|fusion_o2only.cpp|-O2"
    "fusion_o2only_O3|fusion_o2only.cpp|-O3"
    "fusion_O2|fusion.cpp|-O2"
    "fusion_O3|fusion.cpp|-O3"
    "moptm_O2|moptm.cpp|-O2"
    "moptm_O3|moptm.cpp|-O3"
)

# 测试用例: name|input_file|op
CASES=(
    "ADD_1M|add_1M.txt|ADD"
    "ADD_100k|add_100k.txt|ADD"
    "MUL_500k|mul_500k.txt|MUL"
    "MUL_100k|mul_100k.txt|MUL"
    "DIV_1M_500k|div_1M_500k.txt|DIV"
    "DIV_200k_100k|div_200k_100k.txt|DIV"
    "DIV_1M_100k|div_1M_100k.txt|DIV"
)

echo "=== 编译所有版本 ===" | tee -a $OUT_LOG
COMPILED=()
for v in "${VERSIONS[@]}"; do
    IFS='|' read -r name src flags <<< "$v"
    if [ ! -f "$src" ]; then
        echo "[SKIP] $name: 源文件 $src 不存在" | tee -a $OUT_LOG
        continue
    fi
    echo -n "[CMP] $name ($src $flags) ... " | tee -a $OUT_LOG
    ok=1
    for op in ADD MUL DIV; do
        if ! g++ $flags $LC_FLAGS -DHINT_OP_$op -o ${name}_${op} $src 2>logs/${name}_${op}.err; then
            ok=0
            echo -n "[$op FAIL] " | tee -a $OUT_LOG
        fi
    done
    if [ $ok -eq 1 ]; then
        echo "OK" | tee -a $OUT_LOG
        COMPILED+=("$name")
    else
        echo "FAIL (见 logs/${name}_*.err)" | tee -a $OUT_LOG
    fi
done
echo "" | tee -a $OUT_LOG

# 正确性验证
echo "=== 正确性验证 (MD5 对比 moptm_fusion_O2 基准) ===" | tee -a $OUT_LOG
for c in "${CASES[@]}"; do
    IFS='|' read -r cname cfile op <<< "$c"
    if [ ! -f "$cfile" ]; then
        echo "[SKIP] $cname: $cfile 不存在" | tee -a $OUT_LOG
        continue
    fi
    ref_bin="moptm_fusion_O2_${op}"
    if [ ! -f "$ref_bin" ]; then
        echo "[SKIP] $cname: 基准 $ref_bin 不存在" | tee -a $OUT_LOG
        continue
    fi
    ref_md5=$(./$ref_bin < $cfile | md5sum | cut -d' ' -f1)
    line="  $cname: ref=$ref_md5"
    for name in "${COMPILED[@]}"; do
        [ "$name" = "moptm_fusion_O2" ] && continue
        bin="${name}_${op}"
        [ ! -f "$bin" ] && continue
        cur_md5=$(./$bin < $cfile | md5sum | cut -d' ' -f1)
        if [ "$cur_md5" = "$ref_md5" ]; then
            line="$line $name=OK"
        else
            line="$line $name=FAIL"
        fi
    done
    echo "$line" | tee -a $OUT_LOG
done
echo "" | tee -a $OUT_LOG

# Benchmark
echo "=== Benchmark (5 runs median + 2 warmup, taskset -c 0) ===" | tee -a $OUT_LOG
HEADER=$(printf "%-22s" "Binary")
for c in "${CASES[@]}"; do
    IFS='|' read -r cname cfile op <<< "$c"
    HEADER="$HEADER$(printf '%-13s' "$cname")"
done
echo "$HEADER" | tee -a $OUT_LOG

for name in "${COMPILED[@]}"; do
    line=$(printf "%-22s" "$name")
    for c in "${CASES[@]}"; do
        IFS='|' read -r cname cfile op <<< "$c"
        bin="${name}_${op}"
        if [ ! -f "$bin" ] || [ ! -f "$cfile" ]; then
            line="$line$(printf '%-13s' "N/A")"
            continue
        fi
        # warmup
        ./$bin < $cfile > /dev/null 2>&1 || true
        ./$bin < $cfile > /dev/null 2>&1 || true
        # 5 次计时
        times=$(for i in 1 2 3 4 5; do
            python3 -c "
import subprocess, time
t0=time.perf_counter()
subprocess.run(['taskset','-c','0','./$bin'], stdin=open('$cfile','rb'), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
print(f'{(time.perf_counter()-t0)*1000:.3f}')
"
        done)
        median=$(echo "$times" | sort -n | sed -n '3p')
        line="$line$(printf '%-13s' "${median}ms")"
    done
    echo "$line" | tee -a $OUT_LOG
done

echo "" | tee -a $OUT_LOG
echo "=== 完成 $(date) ===" | tee -a $OUT_LOG
echo ""
echo "###### 日志路径: $OUT_LOG ######"
echo "###### 日志内容如下 ######"
cat $OUT_LOG
