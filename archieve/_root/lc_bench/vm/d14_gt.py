#!/usr/bin/env python3
"""ground-truth 探针: cx 判据 vs 真正需要的 incr。"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run  # noqa: E402

PS = r"D:\precious_speed"
IN = "/home/azzr/lcp/big_integer/division_of_big_integers/in/"

put(os.path.join(PS, "best", "div_D14.cpp"), "/home/azzr/divbench/src/d14.cpp")

G = "g++ -O2 -std=c++23 -march=x86-64-v3"
build = (
    "cd ~/divbench/src && "
    f"{G} -DUNWRAP_PROBE -DUNWRAP_MODE=0 -o ../bin/gt0 d14.cpp 2>&1 | head -20; "
    "echo BUILT"
)
rc, out, err = run(build, timeout=5400)
print(out[-3000:])
if "BUILT" not in out:
    sys.exit("build failed")

# 小/中规模用例跑 ground truth
cases = ["medium_02", "medium_00", "medium_01", "small_00", "example_00",
         "burnikel_ziegler_bound_00", "burnikel_ziegler_bound_01",
         "burnikel_ziegler_bound_02", "burnikel_ziegler_bound_03",
         "r_nearly_zero_00"]
cmd = "cd ~/divbench && " + "; ".join(
    f"echo '=== {c}'; ./bin/gt0 < {IN}{c}.in >/dev/null 2>/tmp/p.err; "
    f"sort /tmp/p.err | uniq -c | sort -rn | head -8"
    for c in cases
)
rc, out2, err = run(cmd, timeout=3600)
print(out2)
print(err[-1500:])
with open(os.path.join(PS, "lc_bench", "vm", "_gt_d14.log"), "w", encoding="utf-8") as f:
    f.write(out2)
