#!/usr/bin/env python3
"""ab_mul_lc.py - A/B benchmark with LC compile params (no -march=native)."""
import subprocess, os, time

CUR = r"d:\precious_speed\lc_bench\exe\cur_mul_lc.exe"
BEST = r"d:\precious_speed\lc_bench\exe\best_mul_lc.exe"
CASES_DIR = r"d:\precious_speed\lc_bench\cases\mul"

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

print(f"{'case':<22} {'CUR':>7} {'BEST':>7} {'diff':>7} {'%':>7}")
print("-" * 55)

cur_wins = 0
best_wins = 0
cur_total = 0
best_total = 0
for c in cases:
    inp = os.path.join(CASES_DIR, c + ".in")
    if not os.path.exists(inp):
        inp = os.path.join(CASES_DIR, c + " .in")
    if not os.path.exists(inp):
        print(f"{c:<22} NOT FOUND")
        continue
    run_once(CUR, inp)
    run_once(BEST, inp)
    ct = min(run_once(CUR, inp) for _ in range(3))
    bt = min(run_once(BEST, inp) for _ in range(3))
    cur_total += ct
    best_total += bt
    d = ct - bt
    pc = d / bt * 100 if bt > 0 else 0
    if ct < bt:
        flag = "*CUR"
        cur_wins += 1
    elif bt < ct:
        flag = "*BEST"
        best_wins += 1
    else:
        flag = ""
    print(f"{c:<22} {ct:>7.1f} {bt:>7.1f} {d:>+7.1f} {pc:>+7.1f}% {flag}")

print("-" * 55)
print(f"{'TOTAL':<22} {cur_total:>7.1f} {best_total:>7.1f} {cur_total-best_total:>+7.1f} {(cur_total-best_total)/best_total*100:>+7.1f}%")
print(f"CUR wins: {cur_wins}, BEST wins: {best_wins}")
