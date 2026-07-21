#!/bin/bash
# run_lc_bench.sh - LC 环境下 moptm vs best ADD benchmark
# 10 次运行，输出每次的 CPU 时间

cd /tmp/bench2

echo "=== moptm ADD 2M+2M (10 runs, -O2 LC env) ==="
for i in $(seq 1 10); do
    ./moptm_ADD_bench_lc < big_add_2M_2M.txt > /dev/null
done

echo ""
echo "=== best ADD 2M+2M (10 runs) ==="
for i in $(seq 1 10); do
    ./best_ADD < big_add_2M_2M.txt > /dev/null
done

echo ""
echo "=== moptm ADD 1M+1M (10 runs) ==="
for i in $(seq 1 10); do
    ./moptm_ADD_bench_lc < test_add_1M_1M.txt > /dev/null
done

echo ""
echo "=== best ADD 1M+1M (10 runs) ==="
for i in $(seq 1 10); do
    ./best_ADD < test_add_1M_1M.txt > /dev/null
done
