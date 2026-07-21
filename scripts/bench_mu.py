#!/usr/bin/env python3
import subprocess
import time
import sys
import os

os.chdir("/tmp/bench")

bin1 = sys.argv[1] if len(sys.argv) > 1 else "moptm_fusion_O2_DIV"
bin2 = sys.argv[2] if len(sys.argv) > 2 else "moptm_ref"
tests = sys.argv[3].split() if len(sys.argv) > 3 else ["div_1M_500k", "div_200k_100k", "div_1M_100k"]
runs = 7

def bench(binary, test_file):
    times = []
    for _ in range(runs):
        with open(test_file, "rb") as f:
            data = f.read()
        t0 = time.perf_counter()
        r = subprocess.run(["./" + binary], input=data, capture_output=True)
        t1 = time.perf_counter()
        times.append((t1 - t0) * 1000.0)
    times.sort()
    return times

for t in tests:
    tf = t + ".txt"
    t1 = bench(bin1, tf)
    t2 = bench(bin2, tf)
    m1 = t1[len(t1)//2]
    m2 = t2[len(t2)//2]
    print(f"=== {t} ===")
    print(f"  {bin1}: med={m1:.2f}ms  all={[round(x,2) for x in t1]}")
    print(f"  {bin2}: med={m2:.2f}ms  all={[round(x,2) for x in t2]}")
    delta = m1 - m2
    pct = (m1 - m2) / m2 * 100.0 if m2 > 0 else 0
    print(f"  delta: {delta:+.2f}ms ({pct:+.1f}%)")
