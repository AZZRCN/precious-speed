#!/usr/bin/env python3
"""Windows-side uploader: push sources + cases to VM /home/azzr/mulbench/."""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put

PS = r"D:\precious_speed"

SRC = [
    ("mul_gold.cpp",   os.path.join(PS, "best", "mul_385663.cpp")),
    ("mul_best.cpp",   os.path.join(PS, "best", "mul.cpp")),
    ("mul_387374.cpp", os.path.join(PS, "best", "mul_387374.cpp")),
    ("mul_r4.cpp",     os.path.join(PS, "lc_bench", "exe", "mul_r4.cpp")),
]
CASES = os.path.join(PS, "lc_bench", "cases", "mul")

for remote, local in SRC:
    put(local, f"/home/azzr/mulbench/src/{remote}")

cnt = 0
for fn in sorted(os.listdir(CASES)):
    if fn.endswith(".in"):
        clean = fn.replace(" .in", ".in")   # strip the space quirk
        put(os.path.join(CASES, fn), f"/home/azzr/mulbench/cases/{clean}")
        cnt += 1
print(f"UPLOAD DONE: {len(SRC)} sources + {cnt} cases")
