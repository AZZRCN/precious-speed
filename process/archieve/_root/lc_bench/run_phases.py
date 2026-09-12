#!/usr/bin/env python3
"""run_phases.py - Run mul_bench.exe on cases and show phase timings."""
import subprocess
import os
import sys

EXE = r"d:\precious_speed\lc_bench\exe\mul_bench.exe"
CASES_DIR = r"d:\precious_speed\lc_bench\cases\mul"

cases = sys.argv[1:] if len(sys.argv) > 1 else [
    "max_max_00", "large_00", "fft_killer_00", "medium_00", "small_00"
]

for c in cases:
    inp = os.path.join(CASES_DIR, c + ".in")
    if not os.path.exists(inp):
        print(f"--- {c}: NOT FOUND ---")
        continue
    print(f"--- {c} ---")
    with open(inp, "rb") as f:
        r = subprocess.run([EXE], stdin=f, stdout=subprocess.DEVNULL,
                           stderr=subprocess.PIPE, timeout=30)
    print(r.stderr.decode(errors="replace").strip())
    print()
