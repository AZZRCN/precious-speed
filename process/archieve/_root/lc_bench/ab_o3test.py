#!/usr/bin/env python3
"""ab_o3test.py - Test O3 pragma vs BEST O2."""
import subprocess, os, time

CUR = r"d:\precious_speed\lc_bench\exe\cur_mul_o3prg.exe"
BEST = r"d:\precious_speed\lc_bench\exe\best_mul_lc.exe"
CASES_DIR = r"d:\precious_speed\lc_bench\cases\mul"

cases = [
    "small_00", "medium_00", "medium_01", "medium_02",
    "large_00", "large_01", "large_02", "large_small_00",
    "max_max_00", "max_max_01", "max_max_06",
    "fft_killer_00", "fft_killer_01", "zero_00",
]

def run_once(exe, inp_path):
    with open(inp_path, "rb") as f:
        t0 = time.perf_counter()
        r = subprocess.run([exe], stdin=f, stdout=subprocess.DEVNULL,
                          stderr=subprocess.DEVNULL, timeout=60)
        t1 = time.perf_counter()
    return (t1 - t0) * 1000 if r.returncode == 0 else 0

print(f"{'case':<22} {'CUR_O3':>8} {'BEST_O2':>8} {'diff':>7} {'%':>7}")
print("-" * 58)
gt = 0; gb = 0
for c in cases:
    inp = os.path.join(CASES_DIR, c + ".in")
    if not os.path.exists(inp):
        inp = os.path.join(CASES_DIR, c + " .in")
    if not os.path.exists(inp):
        continue
    run_once(CUR, inp); run_once(BEST, inp)
    ct = min(run_once(CUR, inp) for _ in range(3))
    bt = min(run_once(BEST, inp) for _ in range(3))
    gt += ct; gb += bt
    d = ct - bt
    pc = d / bt * 100 if bt > 0 else 0
    w = "*CUR" if ct < bt else "*BEST" if bt < ct else ""
    print(f"{c:<22} {ct:>8.1f} {bt:>8.1f} {d:>+7.1f} {pc:>+7.1f}% {w}")
print("-" * 58)
print(f"{'TOTAL':<22} {gt:>8.1f} {gb:>8.1f} {gt-gb:>+7.1f} {(gt-gb)/gb*100:>+7.1f}%")
