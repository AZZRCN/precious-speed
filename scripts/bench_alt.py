#!/usr/bin/env python3
import subprocess, time, os
os.chdir('/tmp/bench')

cases = [
    'div_1M_500k.txt',
    'div_200k_100k.txt',
    'div_1M_100k.txt',
]
bins = ['moptm_fusion_DIV', 'moptm_fusion_O2_DIV']

def bench_alt(infile, bins, rounds=30, warmup=5):
    """交替测试两个 binary，消除负载波动"""
    with open(infile, 'rb') as f:
        data = f.read()
    # warmup
    for b in bins:
        for _ in range(warmup):
            subprocess.run(f'taskset -c 0 ./{b}', input=data, capture_output=True, shell=True, cwd='/tmp/bench')
    # 交替测试
    times = {b: [] for b in bins}
    for _ in range(rounds):
        for b in bins:
            t0 = time.perf_counter()
            subprocess.run(f'taskset -c 0 ./{b}', input=data, capture_output=True, shell=True, cwd='/tmp/bench')
            t1 = time.perf_counter()
            times[b].append((t1 - t0) * 1000)
    # 返回最小值和 P10（最稳定的指标）
    result = {}
    for b in bins:
        t = sorted(times[b])
        result[b] = (t[0], t[len(t)//10], t)
    return result

print("=== moptm_fusion (new, const-opt) vs moptm_fusion_O2 (old), alternating, 30 rounds ===")
for f in cases:
    print(f"--- {f} ---")
    res = bench_alt(f, bins)
    min_new, p10_new, all_new = res[bins[0]]
    min_old, p10_old, all_old = res[bins[1]]
    delta_min = (min_new - min_old) / min_old * 100
    delta_p10 = (p10_new - p10_old) / p10_old * 100
    print(f"  {bins[0]:25s}: min={min_new:.2f} p10={p10_new:.2f}")
    print(f"  {bins[1]:25s}: min={min_old:.2f} p10={p10_old:.2f}")
    print(f"  delta_min: {delta_min:+.1f}%  delta_p10: {delta_p10:+.1f}%")
