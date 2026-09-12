#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""D27 (大页 arena) 验证一条龙:
   1) 全用例逐字节对拍 vs d25 (D27 只换分配器, 输出必须完全相同)
   2) 缺页 / peak RSS 对比
   3) 配对比值计时 (交替运行 + median-of-ratios, 纪律同 coldratio.py)

用法: python3 _d27_go.py [reps]
"""
import hashlib
import os
import re
import statistics
import subprocess
import sys
import time

ROOT = "/home/azzr/divbench"
IN = "/home/azzr/lcp/big_integer/division_of_big_integers/in"
A, B = "d25", "d27"
REPS = int(sys.argv[1]) if len(sys.argv) > 1 else 13

CASES = sorted(f[:-3] for f in os.listdir(IN) if f.endswith(".in"))


def out_hash(exe, case):
    with open("%s/%s.in" % (IN, case), "rb") as fi:
        p = subprocess.run([os.path.join(ROOT, "bin", exe)],
                           stdin=fi, stdout=subprocess.PIPE)
    return hashlib.sha256(p.stdout).hexdigest(), len(p.stdout)


# ---------- 1) 逐字节对拍 ----------
print("=" * 74)
print("[1] byte-exact diff  %s vs %s   (%d cases)" % (A, B, len(CASES)))
bad = 0
for c in CASES:
    ha, la = out_hash(A, c)
    hb, lb = out_hash(B, c)
    if ha != hb:
        bad += 1
        print("    MISMATCH %-34s lenA=%d lenB=%d" % (c, la, lb))
print("    => %s" % ("ALL MATCH" if bad == 0 else "%d MISMATCH" % bad))
if bad:
    sys.exit(1)

# ---------- 2) 缺页 / RSS ----------
HEAVY = [c for c in CASES if c.startswith(("length_ratio_integer",
                                           "a_max_b_random", "r_nearly_zero",
                                           "burnikel", "large", "medium"))]


def pf(exe, case):
    p = subprocess.run(
        "/usr/bin/time -f '%%R %%M' " + os.path.join(ROOT, "bin", exe) +
        " < " + IN + "/" + case + ".in > /dev/null",
        shell=True, capture_output=True, text=True)
    m = re.search(r"(\d+)\s+(\d+)\s*$", p.stderr.strip())
    return (int(m.group(1)), int(m.group(2))) if m else (0, 0)


print()
print("=" * 74)
print("[2] page faults / peak RSS")
print("%-32s %9s %9s %11s %11s" % ("case", "PF_" + A, "PF_" + B,
                                   "RSS_" + A + "K", "RSS_" + B + "K"))
for c in HEAVY:
    pf(A, c)  # warm page cache
    pf(B, c)
    fa, ra = pf(A, c)
    fb, rb = pf(B, c)
    print("%-32s %9d %9d %11d %11d" % (c, fa, fb, ra, rb))

# ---------- 3) 配对比值计时 ----------
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


print()
print("=" * 74)
print("[3] paired timing  (reps=%d, median-of-ratios, %s=1.000)" % (REPS, A))
for _ in range(2):                      # 全局预热, 样本全丢
    for c in HEAVY:
        run_once(A, c)
        run_once(B, c)

print("%-32s %9s %9s %9s" % ("case", A + "_ms", B + "_ms", "B/A"))
allr = []
for c in HEAVY:
    ta, tb, rs = [], [], []
    for r in range(REPS):
        if r % 2 == 0:                  # 顺序轮转消位置偏置
            x = run_once(A, c)
            y = run_once(B, c)
        else:
            y = run_once(B, c)
            x = run_once(A, c)
        ta.append(x)
        tb.append(y)
        rs.append(y / x)
    m = statistics.median(rs)
    allr.append(m)
    print("%-32s %9.2f %9.2f %9.4f" % (
        c, statistics.median(ta), statistics.median(tb), m), flush=True)

print()
print("geomean B/A = %.4f   (best %.4f / worst %.4f)" % (
    __import__("math").exp(sum(map(__import__("math").log, allr)) / len(allr)),
    min(allr), max(allr)))
