#!/bin/bash
# vm_perf_best.sh - PERF record for best_div on medium scale
cd ~/div_bench
OUTDIR=perf_results
mkdir -p $OUTDIR

echo "=== perf record: best_div medium ==="
perf record -F 999 -g --call-graph dwarf -o $OUTDIR/best_div_medium.perf ./best_div < div_medium_0.in > /dev/null 2> $OUTDIR/record_best_medium.txt
if [ -f $OUTDIR/best_div_medium.perf ]; then
    echo "Recorded. Top 25 hotspots:"
    perf report -i $OUTDIR/best_div_medium.perf --stdio --no-children -g none --percent-limit 0.5 2>/dev/null | head -60
else
    echo "Record failed:"
    cat $OUTDIR/record_best_medium.txt
fi

echo ""
echo "=== perf record: best_div small ==="
perf record -F 999 -g --call-graph dwarf -o $OUTDIR/best_div_small.perf ./best_div < div_small_0.in > /dev/null 2> $OUTDIR/record_best_small.txt
if [ -f $OUTDIR/best_div_small.perf ]; then
    echo "Recorded. Top 25 hotspots:"
    perf report -i $OUTDIR/best_div_small.perf --stdio --no-children -g none --percent-limit 0.5 2>/dev/null | head -60
else
    echo "Record failed:"
    cat $OUTDIR/record_best_small.txt
fi

echo ""
echo "=== perf record: cur_div small ==="
perf record -F 999 -g --call-graph dwarf -o $OUTDIR/cur_div_small.perf ./cur_div < div_small_0.in > /dev/null 2> $OUTDIR/record_cur_small.txt
if [ -f $OUTDIR/cur_div_small.perf ]; then
    echo "Recorded. Top 25 hotspots:"
    perf report -i $OUTDIR/cur_div_small.perf --stdio --no-children -g none --percent-limit 0.5 2>/dev/null | head -60
else
    echo "Record failed:"
    cat $OUTDIR/record_cur_small.txt
fi
