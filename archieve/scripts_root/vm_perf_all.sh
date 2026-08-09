#!/bin/bash
# vm_perf_all.sh - Full-scale PERF analysis for cur_div vs best_div
# Runs perf stat + perf record on all scales, outputs structured report
set -u
cd ~/div_bench

OUTDIR=perf_results
mkdir -p $OUTDIR

# Event list for perf stat
EVENTS="cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses,L1-dcache-loads,L1-dcache-load-misses,LLC-loads,LLC-load-misses"

# Scales: filename label
SCALES=(
    "div_small_0.in|small"
    "div_medium_0.in|medium"
    "div_large_0.in|large"
    "div_xlarge_0.in|xlarge"
    "div_xxlarge_0.in|xxlarge"
    "div_max_0.in|max_eq"
)

echo "============================================================"
echo "PERF Full-Scale Analysis"
echo "Date: $(date '+%Y-%m-%d %H:%M:%S')"
echo "CPU: $(nproc) cores"
echo "Kernel: $(uname -r)"
echo "Perf: $(perf --version 2>&1)"
echo "Paranoid: $(cat /proc/sys/kernel/perf_event_paranoid)"
echo "============================================================"
echo ""

# Step 1: Generate missing test data
echo "=== Step 1: Generate missing test data ==="
if [ ! -f div_small_0.in ] || [ ! -f div_xlarge_0.in ] || [ ! -f div_xxlarge_0.in ]; then
    echo "  Generating small/xlarge/xxlarge data..."
    python3 vm_gen_data.py
else
    echo "  All data files exist."
fi
echo ""

# Step 2: Verify executables
echo "=== Step 2: Verify executables ==="
for exe in cur_div best_div; do
    if [ -x "$exe" ]; then
        echo "  $exe: OK ($(stat -c %s $exe) bytes)"
    else
        echo "  $exe: MISSING"
    fi
done
echo ""

# Step 3: perf stat for each scale × each exe
echo "=== Step 3: perf stat (3 runs, best) ==="
printf "%-12s %-10s %12s %12s %12s %12s %12s %12s\n" "scale" "exe" "cycles" "instructions" "cache-miss" "branch-miss" "IPC" "time_ms"
printf "%-12s %-10s %12s %12s %12s %12s %12s %12s\n" "-----" "---" "------" "------------" "----------" "-----------" "---" "-------"

for entry in "${SCALES[@]}"; do
    IFS='|' read -r infile label <<< "$entry"
    [ -f "$infile" ] || { echo "  SKIP $label (no $infile)"; continue; }

    for exe in cur_div best_div; do
        [ -x "$exe" ] || continue
        # 3 runs, take best (min cycles)
        best_cycles=999999999999
        best_ins=0
        best_cm=0
        best_bm=0
        best_time=0
        for run in 1 2 3; do
            tmp=$OUTDIR/stat_${label}_${exe}_run${run}.txt
            perf stat -e $EVENTS ./$exe < $infile > /dev/null 2> $tmp
            # Parse perf stat output
            cyc=$(grep -E '^\s*[0-9,]+\s+cycles' $tmp | awk '{print $1}' | tr -d ',')
            ins=$(grep -E '^\s*[0-9,]+\s+instructions' $tmp | awk '{print $1}' | tr -d ',')
            cm=$(grep -E '^\s*[0-9,]+\s+cache-misses' $tmp | awk '{print $1}' | tr -d ',')
            bm=$(grep -E '^\s*[0-9,]+\s+branch-misses' $tmp | awk '{print $1}' | tr -d ',')
            tm=$(grep -E 'seconds time elapsed' $tmp | awk '{print $1}' | head -1)
            [ -z "$cyc" ] && continue
            if [ "$cyc" -lt "$best_cycles" ]; then
                best_cycles=$cyc
                best_ins=$ins
                best_cm=${cm:-0}
                best_bm=${bm:-0}
                best_time=$tm
            fi
        done
        # Compute IPC
        ipc=$(awk "BEGIN{if($best_ins>0) printf \"%.2f\", $best_ins/$best_cycles; else print \"N/A\"}")
        # time in ms
        ms=$(awk "BEGIN{printf \"%.2f\", $best_time*1000}")
        printf "%-12s %-10s %12s %12s %12s %12s %12s %12s\n" "$label" "$exe" "$best_cycles" "$best_ins" "$best_cm" "$best_bm" "$ipc" "$ms"
    done
done
echo ""

# Step 4: perf record for cur_div on key scales (medium, xlarge)
echo "=== Step 4: perf record (cur_div hotspots) ==="
for label_target in "medium:div_medium_0.in" "xlarge:div_xlarge_0.in" "large:div_large_0.in"; do
    IFS=':' read -r label infile <<< "$label_target"
    [ -f "$infile" ] || { echo "  SKIP $label (no $infile)"; continue; }
    echo "  --- cur_div $label ---"
    perf record -F 999 -g --call-graph dwarf -o $OUTDIR/cur_div_${label}.perf ./cur_div < $infile > /dev/null 2> $OUTDIR/record_${label}.txt
    if [ -f $OUTDIR/cur_div_${label}.perf ]; then
        echo "  Recorded. Top 20 hotspots:"
        perf report -i $OUTDIR/cur_div_${label}.perf --stdio --no-children -g none --percent-limit 0.5 2>/dev/null | head -50
    else
        echo "  Record failed. stderr:"
        cat $OUTDIR/record_${label}.txt
    fi
    echo ""
done

echo "============================================================"
echo "PERF analysis complete. Results in $OUTDIR/"
echo "============================================================"
