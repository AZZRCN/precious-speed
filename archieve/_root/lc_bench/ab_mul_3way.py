#!/usr/bin/env python3
"""ab_mul_3way.py - 3-way A/B benchmark: CUR_v0 (baseline) vs CUR_final (optimized) vs BEST."""
import subprocess, os, time

V0    = r"d:\precious_speed\lc_bench\exe\cur_mul_base.exe"
FINAL = r"d:\precious_speed\lc_bench\exe\cur_mul_final.exe"
BEST  = r"d:\precious_speed\lc_bench\exe\best_mul_lc.exe"
CASES_DIR = r"d:\precious_speed\lc_bench\cases\mul"
ROUNDS = 10

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

def find_inp(c):
    inp = os.path.join(CASES_DIR, c + ".in")
    if not os.path.exists(inp):
        inp = os.path.join(CASES_DIR, c + " .in")
    return inp if os.path.exists(inp) else None

# Warmup
print("[warmup] running 1 round per exe per case...")
for c in cases:
    inp = find_inp(c)
    if not inp: continue
    run_once(V0, inp); run_once(FINAL, inp); run_once(BEST, inp)

print(f"{'case':<22} {'V0_med':>8} {'FINAL':>8} {'BEST':>8} {'F-V0':>7} {'F-B':>7} {'V0-B':>7}")
print("-" * 80)

gv0 = 0; gf = 0; gb = 0
for c in cases:
    inp = find_inp(c)
    if not inp: continue
    v0_list = [run_once(V0, inp) for _ in range(ROUNDS)]
    f_list  = [run_once(FINAL, inp) for _ in range(ROUNDS)]
    b_list  = [run_once(BEST, inp) for _ in range(ROUNDS)]
    v0_list.sort(); f_list.sort(); b_list.sort()
    v0 = v0_list[ROUNDS//2]; f = f_list[ROUNDS//2]; b = b_list[ROUNDS//2]
    gv0 += v0; gf += f; gb += b
    fv0 = f - v0; fb = f - b; v0b = v0 - b
    print(f"{c:<22} {v0:>8.2f} {f:>8.2f} {b:>8.2f} {fv0:>+7.2f} {fb:>+7.2f} {v0b:>+7.2f}")

print("-" * 80)
print(f"{'TOTAL':<22} {gv0:>8.2f} {gf:>8.2f} {gb:>8.2f} {gf-gv0:>+7.2f} {gf-gb:>+7.2f} {gv0-gb:>+7.2f}")
print(f"{'% vs BEST':<22} {gv0/gb*100-100:>+7.2f}% {gf/gb*100-100:>+7.2f}% {0:>+7.2f}%")
