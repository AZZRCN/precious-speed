#!/usr/bin/env python3
import subprocess, os
BASE = "/home/azzr/mulbench"
src = BASE + "/src/mul_gold.cpp"
flag_sets = [
    ("plain", "-O2 -std=c++23"),
    ("native", "-O2 -std=c++23 -march=native"),
    ("O0", "-O2 -std=c++23 -O0"),
]
for tag, fl in flag_sets:
    out = BASE + "/bin/gold_" + tag
    r = subprocess.run(["g++", *fl.split(), "-o", out, src], capture_output=True, text=True)
    if r.returncode != 0:
        print(f"[{tag}] BUILD_FAIL {r.stderr[:300]}")
        continue
    p = subprocess.run([out], input="1\n12 34\n", capture_output=True, text=True)
    print(f"[{tag}] trivial: out=[{p.stdout.strip()}] rc={p.returncode}")
    p2 = subprocess.run([out], stdin=open(BASE + "/cases/max_max_00.in", "rb"), capture_output=True)
    print(f"[{tag}] max_max: outbytes={len(p2.stdout)} rc={p2.returncode}")
