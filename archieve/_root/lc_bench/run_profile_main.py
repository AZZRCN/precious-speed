#!/usr/bin/env python3
"""run_profile_main.py - Run mul_profile.exe on all cases, show phase timings."""
import subprocess
import os

import sys
EXE = sys.argv[1] if len(sys.argv) > 1 else r"d:\precious_speed\lc_bench\exe\mul_prof_main.exe"
CASES_DIR = r"d:\precious_speed\lc_bench\cases\mul"

cases = [
    "max_max_00", "max_max_01", "large_00", "large_01",
    "fft_killer_00", "fft_killer_01",
    "medium_00", "medium_01", "medium_02",
    "small_00", "zero_00", "large_small_00",
    "example_00",
]

print(f"{'case':<20} {'PARSE':>8} {'MUL':>8} {'WRITE':>8} {'FLUSH':>8} {'TOTAL':>8}  {'P%':>5} {'M%':>5} {'W%':>5}")
print("-" * 85)

for c in cases:
    inp = os.path.join(CASES_DIR, c + ".in")
    if not os.path.exists(inp):
        inp2 = os.path.join(CASES_DIR, c + " .in")
        if os.path.exists(inp2):
            inp = inp2
        else:
            print(f"{c:<20} NOT FOUND")
            continue
    with open(inp, "rb") as f:
        r = subprocess.run([EXE], stdin=f, stdout=subprocess.DEVNULL,
                           stderr=subprocess.PIPE, timeout=30)
    line = r.stderr.decode(errors="replace").strip()
    parts = line.split()
    vals = {}
    for i, p in enumerate(parts):
        if p.endswith(":") and i + 1 < len(parts):
            try:
                vals[p[:-1]] = float(parts[i + 1])
            except:
                pass
    parse = vals.get("PARSE", -1)
    mul = vals.get("MUL", -1)
    write = vals.get("WRITE", -1)
    flush = vals.get("FLUSH", -1)
    total = vals.get("TOTAL", -1)
    if total > 0:
        pp = parse / total * 100
        mp = mul / total * 100
        wp = write / total * 100
        print(f"{c:<20} {parse:>8.2f} {mul:>8.2f} {write:>8.2f} {flush:>8.2f} {total:>8.2f}  {pp:>5.1f} {mp:>5.1f} {wp:>5.1f}")
    else:
        print(f"{c:<20} {line}")
