#!/bin/bash
# Full O2/O3 benchmark for moptm / fusion / fusion_o2only / best
# 4 versions x 3 ops x 2 opt levels = 24 binaries
# Each: 1 correctness run + 1 warmup + 5 timed runs, report median
set +e
cd /tmp/bench

WARMUP=1
RUNS=5
LOG=/tmp/bench/results.log
> "$LOG"

# run_and_bench <label> <binary> <input>
# Outputs: label median_ms md5 size raw_times
run_and_bench() {
    local name=$1
    local bin=$2
    local input=$3

    if [ ! -x "./$bin" ]; then
        printf "%-32s MISSING\n" "$name" | tee -a "$LOG"
        return
    fi

    # 1) Correctness run - save output
    timeout 600 ./$bin < "$input" > "/tmp/out_${bin}.txt" 2>/tmp/err_${bin}.txt
    local rc=$?
    if [ $rc -ne 0 ]; then
        printf "%-32s FAILED exit=%d  err=%s\n" "$name" "$rc" "$(head -c 100 /tmp/err_${bin}.txt)" | tee -a "$LOG"
        return
    fi
    local md5=$(md5sum "/tmp/out_${bin}.txt" | awk '{print $1}')
    local size=$(wc -c < "/tmp/out_${bin}.txt")

    # 2) Warmup
    local w
    for ((w=0; w<WARMUP; w++)); do
        ./$bin < "$input" > /dev/null 2>&1
    done

    # 3) Timed runs
    local times=""
    local i
    for ((i=0; i<RUNS; i++)); do
        local start=$(date +%s%N)
        ./$bin < "$input" > /dev/null 2>&1
        local end=$(date +%s%N)
        local ms=$(awk "BEGIN {printf \"%.2f\", ($end - $start) / 1000000}")
        times="$times $ms"
    done

    # Median = 3rd of 5 sorted
    local median=$(echo $times | tr ' ' '\n' | grep -v '^$' | sort -n | sed -n '3p')
    printf "%-32s median=%8s ms  md5=%s  size=%d  [raw:%s]\n" "$name" "$median" "$md5" "$size" "$times" | tee -a "$LOG"
}

# ---- Section header ----
section() {
    echo "" | tee -a "$LOG"
    echo "============================================================" | tee -a "$LOG"
    echo "  $1" | tee -a "$LOG"
    echo "============================================================" | tee -a "$LOG"
}

# ============================================================
# O2 BENCHMARK
# ============================================================
section "O2 BENCHMARK"

echo "" | tee -a "$LOG"
echo "--- ADD: 1M + 1M ---" | tee -a "$LOG"
run_and_bench "moptm_O2_ADD"        moptm_O2_ADD        add_1M.txt
run_and_bench "fusion_O2_ADD"       fusion_O2_ADD       add_1M.txt
run_and_bench "fusion_o2only_O2_ADD" fusion_o2only_O2_ADD add_1M.txt
run_and_bench "best_O2_ADD"         best_O2_ADD         add_1M.txt

echo "" | tee -a "$LOG"
echo "--- MUL: 500k * 500k ---" | tee -a "$LOG"
run_and_bench "moptm_O2_MUL"        moptm_O2_MUL        mul_500k.txt
run_and_bench "fusion_O2_MUL"       fusion_O2_MUL       mul_500k.txt
run_and_bench "fusion_o2only_O2_MUL" fusion_o2only_O2_MUL mul_500k.txt
run_and_bench "best_O2_MUL"         best_O2_MUL         mul_500k.txt

echo "" | tee -a "$LOG"
echo "--- DIV: 1M / 500k ---" | tee -a "$LOG"
run_and_bench "moptm_O2_DIV"        moptm_O2_DIV        div_1M_500k.txt
run_and_bench "fusion_O2_DIV"       fusion_O2_DIV       div_1M_500k.txt
run_and_bench "fusion_o2only_O2_DIV" fusion_o2only_O2_DIV div_1M_500k.txt
run_and_bench "best_O2_DIV"         best_O2_DIV         div_1M_500k.txt

# ============================================================
# O3 BENCHMARK
# ============================================================
section "O3 BENCHMARK"

echo "" | tee -a "$LOG"
echo "--- ADD: 1M + 1M ---" | tee -a "$LOG"
run_and_bench "moptm_O3_ADD"        moptm_O3_ADD        add_1M.txt
run_and_bench "fusion_O3_ADD"       fusion_O3_ADD       add_1M.txt
run_and_bench "fusion_o2only_O3_ADD" fusion_o2only_O3_ADD add_1M.txt
run_and_bench "best_O3_ADD"         best_O3_ADD         add_1M.txt

echo "" | tee -a "$LOG"
echo "--- MUL: 500k * 500k ---" | tee -a "$LOG"
run_and_bench "moptm_O3_MUL"        moptm_O3_MUL        mul_500k.txt
run_and_bench "fusion_O3_MUL"       fusion_O3_MUL       mul_500k.txt
run_and_bench "fusion_o2only_O3_MUL" fusion_o2only_O3_MUL mul_500k.txt
run_and_bench "best_O3_MUL"         best_O3_MUL         mul_500k.txt

echo "" | tee -a "$LOG"
echo "--- DIV: 1M / 500k ---" | tee -a "$LOG"
run_and_bench "moptm_O3_DIV"        moptm_O3_DIV        div_1M_500k.txt
run_and_bench "fusion_O3_DIV"       fusion_O3_DIV       div_1M_500k.txt
run_and_bench "fusion_o2only_O3_DIV" fusion_o2only_O3_DIV div_1M_500k.txt
run_and_bench "best_O3_DIV"         best_O3_DIV         div_1M_500k.txt

# ============================================================
# CORRECTNESS SUMMARY
# ============================================================
section "CORRECTNESS MD5 SUMMARY"

for op in ADD MUL DIV; do
    echo "" | tee -a "$LOG"
    echo "--- $op ---" | tee -a "$LOG"
    for ver in moptm_O2 fusion_O2 fusion_o2only_O2 best_O2 moptm_O3 fusion_O3 fusion_o2only_O3 best_O3; do
        bin=${ver}_${op}
        if [ -f "/tmp/out_${bin}.txt" ]; then
            out_md5=$(md5sum "/tmp/out_${bin}.txt" | awk '{print $1}')
            out_size=$(wc -c < "/tmp/out_${bin}.txt")
            printf "  %-32s md5=%s  size=%d\n" "$bin" "$out_md5" "$out_size" | tee -a "$LOG"
        fi
    done
done

echo "" | tee -a "$LOG"
echo "=== DONE ===" | tee -a "$LOG"
