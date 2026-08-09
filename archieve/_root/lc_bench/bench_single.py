#!/usr/bin/env python3
"""bench_single.py - Benchmark single case with proper I/O and timeout.
Usage: python bench_single.py <exe> <input_file> [rounds]
"""
import subprocess, sys, time, os

if len(sys.argv) < 3:
    print("Usage: bench_single.py <exe> <input_file> [rounds]")
    sys.exit(1)

exe = sys.argv[1]
inp = sys.argv[2]
rounds = int(sys.argv[3]) if len(sys.argv) > 3 else 3

if not os.path.exists(exe):
    print(f"[ERR] exe not found: {exe}")
    sys.exit(1)
if not os.path.exists(inp):
    print(f"[ERR] input not found: {inp}")
    sys.exit(1)

# Warmup
with open(inp, "rb") as f:
    subprocess.run([exe], stdin=f, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL, timeout=30)

times = []
for i in range(rounds):
    with open(inp, "rb") as f:
        t0 = time.perf_counter()
        r = subprocess.run([exe], stdin=f, stdout=subprocess.DEVNULL,
                          stderr=subprocess.DEVNULL, timeout=30)
        t1 = time.perf_counter()
    if r.returncode != 0:
        print(f"[round {i}] FAILED rc={r.returncode}")
        sys.exit(1)
    ms = (t1 - t0) * 1000
    times.append(ms)
    print(f"[round {i}] {ms:.1f}ms")

times.sort()
print(f"\nmin={times[0]:.1f}ms median={times[len(times)//2]:.1f}ms max={times[-1]:.1f}ms")
