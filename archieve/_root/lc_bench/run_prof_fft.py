#!/usr/bin/env python3
"""run_prof_fft.py - Measure FFT vs CARRY distribution in fftMul."""
import subprocess
import os

EXE = r"d:\precious_speed\lc_bench\exe\mul_prof_fft.exe"
CASES_DIR = r"d:\precious_speed\lc_bench\cases\mul"

cases = ["max_max_00", "large_00", "fft_killer_00", "medium_00", "small_00"]

for c in cases:
    inp = os.path.join(CASES_DIR, c + ".in")
    if not os.path.exists(inp):
        inp = os.path.join(CASES_DIR, c + " .in")
    if not os.path.exists(inp):
        print(f"--- {c}: NOT FOUND ---")
        continue
    print(f"--- {c} ---")
    with open(inp, "rb") as f:
        r = subprocess.run([EXE], stdin=f, stdout=subprocess.DEVNULL,
                           stderr=subprocess.PIPE, timeout=30)
    # 打印stderr的每一行
    for line in r.stderr.decode(errors="replace").strip().split("\n"):
        print(f"  {line}")
    print()
