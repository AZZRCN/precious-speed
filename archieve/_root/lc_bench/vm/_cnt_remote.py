#!/usr/bin/env python3
# 正确解析 callgrind summary 行的全部事件 (按 events: 头部顺序)
# 用法: python3 _cnt_remote.py <bin> <case1,case2,...> [LLMB]
import os, sys, subprocess, re

BIN = sys.argv[1]
CASES = sys.argv[2].split(",")
LLMB = float(sys.argv[3]) if len(sys.argv) > 3 else 32

ROOT = "/home/azzr/divbench"
IN = "/home/azzr/lcp/big_integer/division_of_big_integers/in"
EXE = os.path.join(ROOT, "bin", BIN)

D1 = "32768,8,64"
LL = "%d,%d,64" % (int(LLMB * 1024 * 1024), 8 if LLMB < 2 else 16)

rows = []
for c in CASES:
    out = "/tmp/cnt_%s_%s.out" % (BIN, c)
    cmd = ("valgrind --tool=callgrind --cache-sim=yes --branch-sim=yes "
           "--I1=32768,8,64 --D1=%s --LL=%s --callgrind-out-file=%s "
           "%s < %s/%s.in > /dev/null 2>/tmp/cnt_err.txt"
           % (D1, LL, out, EXE, IN, c))
    subprocess.run(cmd, shell=True)
    ev, sm = None, None
    with open(out) as f:
        for ln in f:
            if ln.startswith("events:"):
                ev = ln.split()[1:]
            elif ln.startswith("summary:"):
                sm = [int(x) for x in ln.split()[1:]]
                break
    d = dict(zip(ev, sm))
    rows.append((c, d))

keys = ["Ir", "Dr", "Dw", "I1mr", "D1mr", "DLmr", "ILmr", "D1mw", "DLmw",
        "Bc", "Bcm", "Bi", "Bim"]
print("%-32s" % "case" + "".join("%12s" % k for k in keys))
for c, d in rows:
    print("%-32s" % c + "".join("%12d" % d.get(k, 0) for k in keys))

print()
print("%-32s %10s %10s %10s %10s %10s" % ("case", "est(M)", "+DLmw", "+Bcm", "+ILmr", "all(M)"))
for c, d in rows:
    est = d["Ir"] + 5 * d["D1mr"] + 200 * d["DLmr"]
    w = 200 * d["DLmw"] + 5 * d["D1mw"]
    b = 15 * (d["Bcm"] + d["Bim"])
    i = 5 * d["I1mr"] + 200 * d["ILmr"]
    print("%-32s %10.1f %10.1f %10.1f %10.1f %10.1f" % (
        c, est / 1e6, w / 1e6, b / 1e6, i / 1e6, (est + w + b + i) / 1e6))
