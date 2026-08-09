#!/usr/bin/env python3
"""ab_mul_multi.py - Multi-round A/B benchmark with LC compile params."""
import subprocess, os, time

CUR = r"d:\precious_speed\lc_bench\exe\cur_mul_lc.exe"
BEST = r"d:\precious_speed\lc_bench\exe\best_mul_lc.exe"
CASES_DIR = r"d:\precious_speed\lc_bench\cases\mul"
ROUNDS = 5  # 5 rounds, take median

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
                          stderr=subprocess.DEVNULL, timeout=60)
        t1 = time.perf_counter()
    return (t1 - t0) * 1000 if r.returncode == 0 else 0

# Warmup
for c in cases:
    inp = os.path.join(CASES_DIR, c + ".in")
    if not os.path.exists(inp):
        inp = os.path.join(CASES_DIR, c + " .in")
    if not os.path.exists(inp):
        continue
    run_once(CUR, inp)
    run_once(BEST, inp)

print(f"{'case':<22} {'CUR_med':>8} {'BEST_med':>8} {'diff':>7} {'%':>7} {'wins':>6}")
print("-" * 65)

grand_cur = 0
grand_best = 0
cur_wins_total = 0
best_wins_total = 0

for c in cases:
    inp = os.path.join(CASES_DIR, c + ".in")
    if not os.path.exists(inp):
        inp = os.path.join(CASES_DIR, c + " .in")
    if not os.path.exists(inp):
        print(f"{c:<22} NOT FOUND")
        continue
    
    # 5 rounds alternating CUR, BEST
    cur_times = []
    best_times = []
    for _ in range(ROUNDS):
        t = run_once(CUR, inp)
        if t: cur_times.append(t)
        t = run_once(BEST, inp)
        if t: best_times.append(t)
    
    # Take median
    cur_times.sort()
    best_times.sort()
    cur_med = cur_times[len(cur_times)//2]
    best_med = best_times[len(best_times)//2]
    
    grand_cur += cur_med
    grand_best += best_med
    d = cur_med - best_med
    pc = d / best_med * 100 if best_med > 0 else 0
    
    cw = sum(1 for a, b in zip(cur_times, best_times) if a < b)
    cur_wins_total += cw
    best_wins_total += ROUNDS - cw
    
    flag = "*CUR" if cur_med < best_med else "*BEST" if best_med < cur_med else ""
    print(f"{c:<22} {cur_med:>8.1f} {best_med:>8.1f} {d:>+7.1f} {pc:>+7.1f}% {cw}/{ROUNDS} {flag}")

print("-" * 65)
total_diff = grand_cur - grand_best
total_pct = total_diff / grand_best * 100 if grand_best > 0 else 0
print(f"{'TOTAL':<22} {grand_cur:>8.1f} {grand_best:>8.1f} {total_diff:>+7.1f} {total_pct:>+7.1f}%")
print(f"CUR wins {cur_wins_total}/{cur_wins_total + best_wins_total} rounds")
