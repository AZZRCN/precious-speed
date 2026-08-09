#!/usr/bin/env python3
"""ab_mul_fast.py - A/B alternating benchmark CUR vs BEST on all MUL cases."""
import subprocess
import os
import time
import sys

CUR = r"d:\precious_speed\cur_mul.exe"
BEST = r"d:\precious_speed\lc_bench\exe\best_mul_o2.exe"
CASES_DIR = r"d:\precious_speed\lc_bench\cases\mul"
ROUNDS = 3

cases = [
    "example_00", "small_00", "medium_00", "medium_01", "medium_02",
    "large_00", "large_01", "large_02", "large_small_00",
    "max_max_00", "max_max_01", "max_max_02", "max_max_03",
    "max_max_04", "max_max_05", "max_max_06", "max_max_07",
    "fft_killer_00", "fft_killer_01", "zero_00",
]

def run_once(exe, inp_path):
    with open(inp_path, "rb") as f:
        t0 = time.perf_counter()
        r = subprocess.run([exe], stdin=f, stdout=subprocess.DEVNULL,
                          stderr=subprocess.DEVNULL, timeout=30)
        t1 = time.perf_counter()
    if r.returncode == 0:
        return (t1 - t0) * 1000
    return 0

print(f"{'case':<22} {'CUR':>8} {'BEST':>8} {'diff':>8} {'diff%':>7}")
print("-" * 58)

cur_wins = 0
best_wins = 0
for c in cases:
    inp = os.path.join(CASES_DIR, c + ".in")
    if not os.path.exists(inp):
        inp = os.path.join(CASES_DIR, c + " .in")
    if not os.path.exists(inp):
        print(f"{c:<22} NOT FOUND")
        continue

    # Warmup
    run_once(CUR, inp)
    run_once(BEST, inp)

    # Alternating: CUR, BEST, CUR, BEST, CUR, BEST
    cur_times = []
    best_times = []
    for _ in range(ROUNDS):
        t = run_once(CUR, inp)
        if t: cur_times.append(t)
        t = run_once(BEST, inp)
        if t: best_times.append(t)

    cur_min = min(cur_times) if cur_times else 0
    best_min = min(best_times) if best_times else 0
    diff = cur_min - best_min
    pct = (diff / best_min * 100) if best_min > 0 else 0
    flag = ""
    if cur_min < best_min:
        flag = " *CUR"
        cur_wins += 1
    elif best_min < cur_min:
        flag = " *BEST"
        best_wins += 1
    print(f"{c:<22} {cur_min:>8.1f} {best_min:>8.1f} {diff:>+8.1f} {pct:>+7.1f}%{flag}")

print("-" * 58)
print(f"CUR wins: {cur_wins}, BEST wins: {best_wins}")
