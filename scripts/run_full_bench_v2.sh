#!/bin/bash
# 完整 benchmark v2: 8 原版本 + best 三文件, 输出 5 次原始数据 + 中位数
set -e
cd /tmp/bench
mkdir -p logs

LC_FLAGS="-std=gnu++20 -static -DONLINE_JUDGE"
TS=$(date +%Y%m%d_%H%M%S)
OUT_LOG="logs/full_bench_v2_${TS}.log"
RAW_LOG="logs/full_bench_raw_${TS}.log"

echo "=== Full Benchmark v2 (with raw data) $(date) ===" | tee $OUT_LOG
echo "VM: Ubuntu $(lsb_release -rs), g++ $(g++ --version | head -1)" | tee -a $OUT_LOG
echo "CPU: $(grep 'model name' /proc/cpuinfo | head -1 | cut -d: -f2 | xargs)" | tee -a $OUT_LOG
echo "方法: 5 runs + 2 warmup, taskset -c 0, 输出原始数据+中位数" | tee -a $OUT_LOG
echo "" | tee -a $OUT_LOG

# 生成缺失的数据
if [ ! -f add_100k.txt ]; then python3 gen_all_data.py; fi

# === 编译 ===
echo "=== 编译所有版本 ===" | tee -a $OUT_LOG
COMPILED=()

# 通用 -DHINT_OP 开关版本
for name_src_flags in \
    "moptm_fusion_O2|moptm_fusion.cpp|-O2" \
    "moptm_fusion_O3|moptm_fusion.cpp|-O3" \
    "fusion_o2only_O2|fusion_o2only.cpp|-O2" \
    "fusion_o2only_O3|fusion_o2only.cpp|-O3" \
    "fusion_O2|fusion.cpp|-O2" \
    "fusion_O3|fusion.cpp|-O3" \
    "moptm_O2|moptm.cpp|-O2" \
    "moptm_O3|moptm.cpp|-O3"
do
    IFS='|' read -r name src flags <<< "$name_src_flags"
    [ ! -f "$src" ] && { echo "[SKIP] $name: $src 不存在" | tee -a $OUT_LOG; continue; }
    echo -n "[CMP] $name ... " | tee -a $OUT_LOG
    ok=1
    for op in ADD MUL DIV; do
        if ! g++ $flags $LC_FLAGS -DHINT_OP_$op -o ${name}_${op} $src 2>logs/${name}_${op}.err; then
            ok=0; echo -n "[$op FAIL] " | tee -a $OUT_LOG
        fi
    done
    [ $ok -eq 1 ] && { echo "OK" | tee -a $OUT_LOG; COMPILED+=("$name"); } || echo "FAIL" | tee -a $OUT_LOG
done

# best 三文件 (无 -DHINT_OP 开关, 独立 main)
for opt in O2 O3; do
    flags="-O${opt#O}"
    echo -n "[CMP] best_$opt ... " | tee -a $OUT_LOG
    ok=1
    if ! g++ $flags $LC_FLAGS -o best_${opt}_ADD best_add.cpp 2>logs/best_${opt}_ADD.err; then ok=0; echo -n "[ADD FAIL] " | tee -a $OUT_LOG; fi
    if ! g++ $flags $LC_FLAGS -o best_${opt}_MUL best_mul.cpp 2>logs/best_${opt}_MUL.err; then ok=0; echo -n "[MUL FAIL] " | tee -a $OUT_LOG; fi
    if ! g++ $flags $LC_FLAGS -o best_${opt}_DIV best_div.cpp 2>logs/best_${opt}_DIV.err; then ok=0; echo -n "[DIV FAIL] " | tee -a $OUT_LOG; fi
    [ $ok -eq 1 ] && { echo "OK" | tee -a $OUT_LOG; COMPILED+=("best_$opt"); } || echo "FAIL" | tee -a $OUT_LOG
done

echo "" | tee -a $OUT_LOG

# === 正确性验证 ===
echo "=== 正确性验证 (MD5 对比 moptm_fusion_O2 基准) ===" | tee -a $OUT_LOG
CASES=(
    "ADD_1M|add_1M.txt|ADD"
    "ADD_100k|add_100k.txt|ADD"
    "MUL_500k|mul_500k.txt|MUL"
    "MUL_100k|mul_100k.txt|MUL"
    "DIV_1M_500k|div_1M_500k.txt|DIV"
    "DIV_200k_100k|div_200k_100k.txt|DIV"
    "DIV_1M_100k|div_1M_100k.txt|DIV"
)
for c in "${CASES[@]}"; do
    IFS='|' read -r cname cfile op <<< "$c"
    [ ! -f "$cfile" ] && continue
    ref_bin="moptm_fusion_O2_${op}"
    [ ! -f "$ref_bin" ] && continue
    ref_md5=$(./$ref_bin < $cfile | md5sum | cut -d' ' -f1)
    line="  $cname: ref=$ref_md5"
    for name in "${COMPILED[@]}"; do
        [ "$name" = "moptm_fusion_O2" ] && continue
        bin="${name}_${op}"
        [ ! -f "$bin" ] && continue
        cur_md5=$(./$bin < $cfile | md5sum | cut -d' ' -f1)
        [ "$cur_md5" = "$ref_md5" ] && line="$line $name=OK" || line="$line $name=FAIL"
    done
    echo "$line" | tee -a $OUT_LOG
done
echo "" | tee -a $OUT_LOG

# === Benchmark: 5 次原始数据 + 中位数 ===
echo "=== Benchmark (5 runs + 2 warmup, taskset -c 0) ===" | tee -a $OUT_LOG
echo "" > $RAW_LOG
echo "# Raw benchmark data (5 runs each, ms)" >> $RAW_LOG
echo "# Date: $(date)" >> $RAW_LOG
echo "# VM: Ubuntu $(lsb_release -rs), g++ $(g++ --version | head -1)" >> $RAW_LOG
echo "" >> $RAW_LOG

# 表头
HEADER=$(printf "%-22s" "Binary")
for c in "${CASES[@]}"; do
    IFS='|' read -r cname cfile op <<< "$c"
    HEADER="$HEADER$(printf '%-13s' "$cname")"
done
echo "$HEADER" | tee -a $OUT_LOG

for name in "${COMPILED[@]}"; do
    line=$(printf "%-22s" "$name")
    echo "" >> $RAW_LOG
    echo "## $name" >> $RAW_LOG
    for c in "${CASES[@]}"; do
        IFS='|' read -r cname cfile op <<< "$c"
        bin="${name}_${op}"
        echo "### $cname ($bin)" >> $RAW_LOG
        if [ ! -f "$bin" ] || [ ! -f "$cfile" ]; then
            line="$line$(printf '%-13s' "N/A")"
            echo "N/A" >> $RAW_LOG
            continue
        fi
        # warmup
        ./$bin < $cfile > /dev/null 2>&1 || true
        ./$bin < $cfile > /dev/null 2>&1 || true
        # 5 次计时, 记录原始数据
        raw_times=""
        for i in 1 2 3 4 5; do
            t_ms=$(python3 -c "
import subprocess, time
t0=time.perf_counter()
subprocess.run(['taskset','-c','0','./$bin'], stdin=open('$cfile','rb'), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
print(f'{(time.perf_counter()-t0)*1000:.3f}')
")
            raw_times="$raw_times $t_ms"
            echo "  run $i: $t_ms ms" >> $RAW_LOG
        done
        median=$(python3 -c "
ts = [float(x) for x in '$raw_times'.split()]
ts.sort()
print(f'{ts[2]:.3f}')  # 5个数中位数 = 第3个
")
        line="$line$(printf '%-13s' "${median}ms")"
        echo "  median: $median ms" >> $RAW_LOG
    done
    echo "$line" | tee -a $OUT_LOG
done

echo "" | tee -a $OUT_LOG
echo "=== 完成 $(date) ===" | tee -a $OUT_LOG
echo "" | tee -a $OUT_LOG
echo "原始数据日志: $RAW_LOG" | tee -a $OUT_LOG
echo "汇总日志: $OUT_LOG" | tee -a $OUT_LOG
echo ""
echo "###### 汇总表 ######"
cat $OUT_LOG
echo ""
echo "###### 原始数据 (前 100 行) ######"
head -100 $RAW_LOG
