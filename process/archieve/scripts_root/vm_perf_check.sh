#!/bin/bash
# vm_perf_check.sh - Check VM test data files
cd ~/div_bench

echo "=== Input files ==="
ls -la *.in 2>/dev/null

echo ""
echo "=== First line of each input ==="
for f in *.in; do
    [ -f "$f" ] || continue
    printf "%-25s: " "$f"
    head -1 "$f"
done

echo ""
echo "=== Existing executables ==="
ls -la cur_div best_div cur_div_pure best_div_pure 2>/dev/null

echo ""
echo "=== Perf version ==="
perf --version 2>/dev/null

echo ""
echo "=== Kernel paranoid level ==="
cat /proc/sys/kernel/perf_event_paranoid 2>/dev/null
