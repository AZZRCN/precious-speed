#!/usr/bin/env python3
"""quick_ab.py - Quick A/B sanity check (not precise benchmark).
Runs CUR vs BEST on a few representative cases, 3 rounds each.
Purpose: detect gross regressions only (night benchmarking unreliable).
"""
import subprocess, os, time, sys

CUR = r"d:\precious_speed\cur_mul.exe"
BEST = r"d:\precious_speed\best_mul.exe"
CASES_DIR = r"d:\precious_speed\lc_bench\cases\mul"
ROUNDS = 3

# Representative cases (not all 20, just sanity check)
cases = [
    "medium_00", "large_00", "large_small_00",
    "max_max_00", "max_max_03", "fft_killer_00",
]

def run_once(exe, inp_path):
    with open(inp_path, "rb") as f:
        t0 = time.perf_counter()
        r = subprocess.run([exe], stdin=f, stdout=subprocess.DEVNULL,
                          stderr=subprocess.DEVNULL, timeout=30)
        t1 = time.perf_counter()
    return (t1 - t0) * 1000 if r.returncode == 0 else -1

print(f"{'case':<22} {'CUR_med':>8} {'BEST_med':>8} {'diff':>7} {'%':>7}")
print("-" * 57)

gt = 0; gb = 0
for c in cases:
    inp = os.path.join(CASES_DIR, c + ".in")
    if not os.path.exists(inp):
        inp = os.path.join(CASES_DIR, c + " .in")
    if not os.path.exists(inp):
        print(f"[SKIP] {c}")
        continue
    # Warmup
    run_once(CUR, inp); run_once(BEST, inp)
    ct_list = sorted([run_once(CUR, inp) for _ in range(ROUNDS)])
    bt_list = sorted([run_once(BEST, inp) for _ in range(ROUNDS)])
    ct = ct_list[ROUNDS//2]; bt = bt_list[ROUNDS//2]
    gt += ct; gb += bt
    d = ct - bt; pc = d / bt * 100 if bt > 0 else 0
    flag = "*CUR" if ct < bt else "*BEST" if bt < ct else ""
    print(f"{c:<22} {ct:>8.1f} {bt:>8.1f} {d:>+7.1f} {pc:>+7.1f}% {flag}")

print("-" * 57)
td = gt - gb; tp = td / gb * 100 if gb > 0 else 0
print(f"{'TOTAL':<22} {gt:>8.1f} {gb:>8.1f} {td:>+7.1f} {tp:>+7.1f}%")
print("\n(Note: night benchmarking unreliable, sanity check only)")
