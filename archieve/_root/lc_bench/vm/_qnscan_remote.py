#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""扫描 absDivRem 的 BASIC 分派阈值 DIV_BASIC_QN。
   对每个变体: 先 26 例逐字节对拍 vs 基线, 再配对计时。
   用法: python3 _qnscan_remote.py "0,8,16,24,32,48" [reps]
"""
import hashlib
import os
import statistics
import subprocess
import sys
import time

ROOT = "/home/azzr/divbench"
IN = "/home/azzr/lcp/big_integer/division_of_big_integers/in"
BASE = "d25"

QNS = [int(x) for x in sys.argv[1].split(",")] if len(sys.argv) > 1 else [32]
REPS = int(sys.argv[2]) if len(sys.argv) > 2 else 9

ALL = sorted(f[:-3] for f in os.listdir(IN) if f.endswith(".in"))
# 受 BASIC 分支影响的用例 (含大量小除法组)
HEAVY = [c for c in ALL if c.startswith(
    ("r_nearly_zero", "medium", "burnikel", "small", "large"))]


def sh(cmd):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True)


# ---------- 编译各变体 ----------
bins = []
for qn in QNS:
    b = "d28q%d" % qn
    r = sh("cd %s && g++ -O2 -std=c++23 -march=x86-64-v3 -DDIV_BASIC_QN=%d "
           "-o bin/%s div_D28.cpp 2>&1 | head -5" % (ROOT, qn, b))
    if r.stdout.strip():
        print("BUILD WARN qn=%d: %s" % (qn, r.stdout.strip()[:300]))
    if not os.path.exists(os.path.join(ROOT, "bin", b)):
        print("BUILD FAIL qn=%d" % qn)
        sys.exit(1)
    bins.append((qn, b))
print("built:", ", ".join(b for _, b in bins), flush=True)


def out_hash(exe, case):
    with open("%s/%s.in" % (IN, case), "rb") as fi:
        p = subprocess.run([os.path.join(ROOT, "bin", exe)],
                           stdin=fi, stdout=subprocess.PIPE)
    return hashlib.sha256(p.stdout).hexdigest()


# ---------- 正确性 ----------
print("\n[1] byte-exact vs %s (%d cases)" % (BASE, len(ALL)))
ref = {c: out_hash(BASE, c) for c in ALL}
ok_bins = []
for qn, b in bins:
    bad = [c for c in ALL if out_hash(b, c) != ref[c]]
    print("    qn=%-3d %s" % (qn, "ALL MATCH" if not bad
                              else "MISMATCH: " + ",".join(bad[:5])))
    if not bad:
        ok_bins.append((qn, b))
if not ok_bins:
    sys.exit("no correct variant")


# ---------- 计时 ----------
def run_once(exe, case):
    fi = open("%s/%s.in" % (IN, case), "rb")
    fo = open("/dev/null", "wb")
    t0 = time.perf_counter()
    subprocess.run(["taskset", "-c", "0", os.path.join(ROOT, "bin", exe)],
                   stdin=fi, stdout=fo)
    t1 = time.perf_counter()
    fi.close()
    fo.close()
    return (t1 - t0) * 1000.0


cands = [BASE] + [b for _, b in ok_bins]
print("\n[2] paired timing  reps=%d  baseline=%s" % (REPS, BASE))
for _ in range(2):                       # 预热, 全丢
    for c in HEAVY:
        for x in cands:
            run_once(x, c)

hdr = "%-30s" % "case" + "".join("%11s" % x for x in cands)
print(hdr)
acc = {x: [] for x in cands}
for c in HEAVY:
    ratios = {x: [] for x in cands}
    absms = {x: [] for x in cands}
    for r in range(REPS):
        order = cands[r % len(cands):] + cands[:r % len(cands)]
        tup = {}
        for x in order:
            tup[x] = run_once(x, c)
        for x in cands:
            absms[x].append(tup[x])
            ratios[x].append(tup[x] / tup[BASE])
    row = "%-30s" % c
    for x in cands:
        m = statistics.median(ratios[x])
        acc[x].append(m)
        row += "%11s" % ("%.2f/%.3f" % (statistics.median(absms[x]), m))
    print(row, flush=True)

import math
print("\n%-30s" % "GEOMEAN" + "".join(
    "%11.4f" % math.exp(sum(map(math.log, acc[x])) / len(acc[x]))
    for x in cands))
