#!/usr/bin/env python3
import subprocess, time, os
os.chdir('/tmp/bench')

# (infile, new_bin, old_bin)
cases = [
    ('add_1M.txt', 'moptm_fusion_ADD', 'moptm_O2_ADD'),
    ('mul_500k.txt', 'moptm_fusion_MUL', 'moptm_O2_MUL'),
]

def bench_alt(infile, bins, rounds=30, warmup=5):
    with open(infile, 'rb') as f:
        data = f.read()
    for b in bins:
        for _ in range(warmup):
            subprocess.run(f'taskset -c 0 ./{b}', input=data, capture_output=True, shell=True, cwd='/tmp/bench')
    times = {b: [] for b in bins}
    for _ in range(rounds):
        for b in bins:
            t0 = time.perf_counter()
            subprocess.run(f'taskset -c 0 ./{b}', input=data, capture_output=True, shell=True, cwd='/tmp/bench')
            t1 = time.perf_counter()
            times[b].append((t1 - t0) * 1000)
    result = {}
    for b in bins:
        t = sorted(times[b])
        result[b] = (t[0], t[len(t)//10], t)
    return result

# 正确性检查
print("=== Correctness ===")
for f, b1, b2 in cases:
    r1 = subprocess.run(f'taskset -c 0 ./{b1}', input=open(f,'rb').read(), capture_output=True, shell=True, cwd='/tmp/bench')
    r2 = subprocess.run(f'taskset -c 0 ./{b2}', input=open(f,'rb').read(), capture_output=True, shell=True, cwd='/tmp/bench')
    ok = r1.stdout == r2.stdout
    print(f"  {f}: {b1} vs {b2} -> {'PASS' if ok else 'FAIL'}")

print("\n=== ADD/MUL: new (const-opt) vs old, alternating, 30 rounds ===")
for f, b1, b2 in cases:
    print(f"--- {f} ---")
    res = bench_alt(f, [b1, b2])
    min1, p10_1, _ = res[b1]
    min2, p10_2, _ = res[b2]
    delta_min = (min1 - min2) / min2 * 100
    print(f"  {b1:25s}: min={min1:.2f} p10={p10_1:.2f}")
    print(f"  {b2:25s}: min={min2:.2f} p10={p10_2:.2f}")
    print(f"  delta_min: {delta_min:+.1f}%")
