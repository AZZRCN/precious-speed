#!/usr/bin/env python3
import subprocess, time, sys, os
os.chdir('/tmp/bench')

cases = [
    ('div_1M_500k.txt', 'moptm_fusion_DIV', 'moptm_fusion_O2_DIV'),
    ('div_200k_100k.txt', 'moptm_fusion_DIV', 'moptm_fusion_O2_DIV'),
    ('div_1M_100k.txt', 'moptm_fusion_DIV', 'moptm_fusion_O2_DIV'),
]

def bench(binary, infile, runs=15, warmup=3):
    with open(infile, 'rb') as f:
        data = f.read()
    cmd = f'taskset -c 0 ./{binary}'
    # warmup
    for _ in range(warmup):
        subprocess.run(cmd, input=data, capture_output=True, shell=True, cwd='/tmp/bench')
    times = []
    for _ in range(runs):
        t0 = time.perf_counter()
        subprocess.run(cmd, input=data, capture_output=True, shell=True, cwd='/tmp/bench')
        t1 = time.perf_counter()
        times.append((t1 - t0) * 1000)  # ms
    times.sort()
    # drop 2 min and 2 max, return median of remaining 11
    trimmed = times[2:-2]
    return trimmed[len(trimmed)//2], times

print("=== moptm_fusion (new, const-opt) vs moptm_fusion_O2 (old) ===")
for infile, b1, b2 in cases:
    print(f"--- {infile} ---")
    m1, all1 = bench(b1, infile)
    m2, all2 = bench(b2, infile)
    delta = (m1 - m2) / m2 * 100
    print(f"  {b1:20s}: {m1:.2f} ms  (all: {' '.join(f'{x:.2f}' for x in all1)})")
    print(f"  {b2:20s}: {m2:.2f} ms  (all: {' '.join(f'{x:.2f}' for x in all2)})")
    print(f"  delta: {delta:+.1f}%")
