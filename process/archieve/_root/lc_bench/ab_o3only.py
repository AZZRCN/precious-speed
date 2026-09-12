#!/usr/bin/env python3
"""ab_o3only.py - Test O3-only pragma vs BEST O2, 5 rounds median."""
import subprocess, os, time

CUR = r"d:\precious_speed\lc_bench\exe\cur_mul_o3only.exe"
BEST = r"d:\precious_speed\lc_bench\exe\best_mul_lc.exe"
CASES_DIR = r"d:\precious_speed\lc_bench\cases\mul"
ROUNDS = 5

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
    run_once(CUR, inp); run_once(BEST, inp)

print(f"{'case':<22} {'CUR_O3':>8} {'BEST_O2':>8} {'diff':>7} {'%':>7} {'wins':>6}")
print("-" * 65)

gt = 0; gb = 0; cw_total = 0; bw_total = 0
for c in cases:
    inp = os.path.join(CASES_DIR, c + ".in")
    if not os.path.exists(inp):
        inp = os.path.join(CASES_DIR, c + " .in")
    if not os.path.exists(inp):
        continue
    ct_list = [run_once(CUR, inp) for _ in range(ROUNDS)]
    bt_list = [run_once(BEST, inp) for _ in range(ROUNDS)]
    ct_list.sort(); bt_list.sort()
    ct = ct_list[ROUNDS//2]; bt = bt_list[ROUNDS//2]
    gt += ct; gb += bt
    d = ct - bt; pc = d / bt * 100 if bt > 0 else 0
    cw = sum(1 for a, b in zip(ct_list, bt_list) if a < b)
    cw_total += cw; bw_total += ROUNDS - cw
    flag = "*CUR" if ct < bt else "*BEST" if bt < ct else ""
    print(f"{c:<22} {ct:>8.1f} {bt:>8.1f} {d:>+7.1f} {pc:>+7.1f}% {cw}/{ROUNDS} {flag}")

print("-" * 65)
td = gt - gb; tp = td / gb * 100 if gb > 0 else 0
print(f"{'TOTAL':<22} {gt:>8.1f} {gb:>8.1f} {td:>+7.1f} {tp:>+7.1f}%")
print(f"CUR wins {cw_total}/{cw_total + bw_total} rounds")
