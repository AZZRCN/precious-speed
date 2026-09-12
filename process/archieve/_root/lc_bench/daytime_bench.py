#!/usr/bin/env python3
"""daytime_bench.py - Full 20-case 10-round A/B benchmark for daytime use.
Usage: python daytime_bench.py [rounds]
       Default rounds=10. Run during daytime for reliable results.
"""
import subprocess, os, time, sys

CUR = os.environ.get("BENCH_CUR", r"d:\precious_speed\cur_mul.exe")
BEST = os.environ.get("BENCH_BEST", r"d:\precious_speed\best_mul.exe")
CASES_DIR = r"d:\precious_speed\lc_bench\cases\mul"
ROUNDS = int(sys.argv[1]) if len(sys.argv) > 1 else 10

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
print("Warmup...")
for c in cases:
    inp = os.path.join(CASES_DIR, c + ".in")
    if not os.path.exists(inp):
        inp = os.path.join(CASES_DIR, c + " .in")
    if not os.path.exists(inp):
        continue
    run_once(CUR, inp); run_once(BEST, inp)

print(f"\n{'case':<22} {'CUR_med':>8} {'BEST_med':>8} {'diff':>7} {'%':>7} {'wins':>7}")
print("-" * 67)

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

print("-" * 67)
td = gt - gb; tp = td / gb * 100 if gb > 0 else 0
print(f"{'TOTAL':<22} {gt:>8.1f} {gb:>8.1f} {td:>+7.1f} {tp:>+7.1f}%")
print(f"CUR wins {cw_total}/{cw_total + bw_total} rounds")

# EXE size comparison
cur_sz = os.path.getsize(CUR)
best_sz = os.path.getsize(BEST)
print(f"\nCUR EXE: {cur_sz/1024:.1f} KB, BEST EXE: {best_sz/1024:.1f} KB (CUR {cur_sz-best_sz:+.0f} bytes)")
