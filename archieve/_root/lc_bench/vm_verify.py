#!/usr/bin/env python3
"""vm_verify.py - Verify CUR vs BEST output equality on VM.
Usage: ssh azzr@10.144.33.157 "cd ~ && python3 vm_verify.py"
"""
import subprocess, os

CUR = "./cur_mul"
BEST = "./best_mul"
CASES_DIR = "./cases/mul"

cases = [
    "example_00", "small_00", "medium_00", "medium_01", "medium_02",
    "large_00", "large_01", "large_02", "large_small_00",
    "max_max_00", "max_max_01", "max_max_02", "max_max_03",
    "max_max_04", "max_max_05", "max_max_06", "max_max_07",
    "fft_killer_00", "fft_killer_01", "zero_00",
]

def run_exe(exe, inp):
    with open(inp, "rb") as f:
        r = subprocess.run([exe], stdin=f, stdout=subprocess.PIPE,
                          stderr=subprocess.DEVNULL, timeout=60)
    return r.stdout

passed = 0; failed = 0
for c in cases:
    inp = os.path.join(CASES_DIR, c + ".in")
    if not os.path.exists(inp):
        inp = os.path.join(CASES_DIR, c + " .in")
    if not os.path.exists(inp):
        print(f"[SKIP] {c}")
        continue
    print(f"[run] {c}...", flush=True)
    a = run_exe(CUR, inp)
    b = run_exe(BEST, inp)
    if a == b:
        print(f"[PASS] {c}")
        passed += 1
    else:
        print(f"[FAIL] {c}")
        failed += 1

print(f"\nTotal: {passed+failed}, Passed: {passed}, Failed: {failed}")
