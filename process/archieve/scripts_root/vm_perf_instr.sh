#!/bin/bash
# vm_perf_instr.sh - Compare instruction-level hotspots between cur_div and best_div
cd ~/div_bench
OUTDIR=perf_results
mkdir -p $OUTDIR

echo "=== perf record with instructions event (medium) ==="

echo "--- cur_div: instruction samples ---"
perf record -e instructions:u -F 999 --call-graph dwarf -o $OUTDIR/cur_instr_medium.perf ./cur_div < div_medium_0.in > /dev/null 2>&1
perf report -i $OUTDIR/cur_instr_medium.perf --stdio --no-children -g none --percent-limit 0.1 2>/dev/null | head -60

echo ""
echo "--- best_div: instruction samples ---"
perf record -e instructions:u -F 999 --call-graph dwarf -o $OUTDIR/best_instr_medium.perf ./best_div < div_medium_0.in > /dev/null 2>&1
perf report -i $OUTDIR/best_instr_medium.perf --stdio --no-children -g none --percent-limit 0.1 2>/dev/null | head -60

echo ""
echo "=== perf diff (cur vs best) ==="
echo "Comparing top functions..."
perf diff $OUTDIR/best_instr_medium.perf $OUTDIR/cur_instr_medium.perf --stdio --percent-limit 0.5 2>/dev/null | head -40
