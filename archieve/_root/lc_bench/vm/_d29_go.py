#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""D29 (融合 submul_1) 验证 + 收益量化。

d29  = div_D29.cpp -DDIV_FUSED_SUBMUL=1  (新: 单趟 submul_1)
d29b = div_D29.cpp -DDIV_FUSED_SUBMUL=0  (旧: absMul1+absCompare+absSub, 语义 == d27)

1) 26 例逐字节对拍 d29 / d29b vs d25   —— 融合是恒等变换, 必须全同
2) callgrind Zen3 est 对比 d29 vs d29b —— 夜间用确定性代理
3) 配对比值计时 d29 vs d29b

用法: python3 _d29_go.py [reps]
"""
import hashlib
import os
import statistics
import subprocess
import sys
import time

ROOT = "/home/azzr/divbench"
IN = "/home/azzr/lcp/big_integer/division_of_big_integers/in"
REPS = int(sys.argv[1]) if len(sys.argv) > 1 else 9
CASES = sorted(f[:-3] for f in os.listdir(IN) if f.endswith(".in"))
CACHE = "--cache-sim=yes --D1=32768,8,64 --I1=32768,8,64 --LL=33554432,16,64"


def sh(cmd, **kw):
    return subprocess.run(cmd, shell=True, cwd=ROOT, capture_output=True,
                          text=True, **kw)


# ---------- 0) build ----------
print("=" * 78)
for tag, flag in (("d29", 1), ("d29b", 0)):
    t0 = time.time()
    r = sh("g++ -O2 -std=c++23 -march=x86-64-v3 -DDIV_FUSED_SUBMUL=%d "
           "div_D29.cpp -o bin/%s 2>&1" % (flag, tag), timeout=1800)
    if r.returncode != 0:
        print("BUILD FAIL %s\n%s" % (tag, (r.stdout or "")[-3000:]))
        sys.exit(1)
    print("built %-5s (%.0fs)" % (tag, time.time() - t0), flush=True)


def out_hash(exe, case):
    with open("%s/%s.in" % (IN, case), "rb") as fi:
        p = subprocess.run([os.path.join(ROOT, "bin", exe)],
                           stdin=fi, stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE)
    return hashlib.sha256(p.stdout).hexdigest(), p.returncode


# ---------- 1) 逐字节对拍 ----------
print("=" * 78)
print("[1] byte-exact vs d25 (%d cases)" % len(CASES))
ref = {}
bad = {"d29": [], "d29b": []}
for c in CASES:
    ref[c] = out_hash("d25", c)[0]
    for tag in ("d29", "d29b"):
        h, rc = out_hash(tag, c)
        if h != ref[c] or rc != 0:
            bad[tag].append("%s%s" % (c, "" if rc == 0 else "(rc=%d)" % rc))
for tag in ("d29", "d29b"):
    print("    %-5s %s" % (tag, "ALL MATCH" if not bad[tag]
                           else "MISMATCH: " + ",".join(bad[tag])))
if bad["d29"]:
    print("=> fused submul_1 WRONG, stop")
    sys.exit(1)

# ---------- 2) callgrind Zen3 est ----------
HEAVY = [c for c in CASES if c.startswith(
    ("length_ratio_integer", "a_max_b_random", "r_nearly_zero",
     "burnikel", "medium", "large"))]


def est(exe, case):
    outf = "/tmp/cg29_%s_%s.out" % (exe, case)
    sh("valgrind --tool=callgrind %s --callgrind-out-file=%s ./bin/%s "
       "< %s/%s.in > /dev/null 2>/dev/null" % (CACHE, outf, exe, IN, case),
       timeout=7200)
    ev = tot = None
    try:
        with open(outf) as f:
            for line in f:
                if line.startswith("events:"):
                    ev = line.split()[1:]
                elif line.startswith(("summary:", "totals:")):
                    tot = [int(x) for x in line.split()[1:]]
    except OSError:
        return None
    if not ev or not tot:
        return None
    d = dict(zip(ev, tot))
    Ir = d.get("Ir", 0)
    d1 = d.get("D1mr", 0) + d.get("D1mw", 0)
    dl = d.get("DLmr", 0) + d.get("DLmw", 0)
    return Ir, d1, dl, Ir + 5 * d1 + 200 * dl


print("=" * 78)
print("[2] Zen3 est  (d29 fused vs d29b old)")
print("%-30s %13s %13s %7s  %9s" % ("case", "est_old", "est_new", "ratio", "dIr%"))
for c in HEAVY:
    a, b = est("d29b", c), est("d29", c)
    if not a or not b:
        print("%-30s FAIL" % c, flush=True)
        continue
    print("%-30s %13d %13d %7.4f  %+8.2f%%"
          % (c, a[3], b[3], b[3] / a[3], 100.0 * (b[0] - a[0]) / a[0]),
          flush=True)

# ---------- 3) 配对计时 ----------
print("=" * 78)
print("[3] paired timing  d29 / d29b   (reps=%d, median-of-ratios)" % REPS)


def t1(exe, case):
    with open("%s/%s.in" % (IN, case), "rb") as fi:
        t0 = time.perf_counter()
        subprocess.run([os.path.join(ROOT, "bin", exe)], stdin=fi,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return time.perf_counter() - t0


ratios = []
print("%-30s %9s %9s %8s" % ("case", "old_ms", "new_ms", "ratio"))
for c in HEAVY:
    t1("d29", c); t1("d29b", c)          # 预热, 丢弃
    ra, rb = [], []
    for k in range(REPS):
        if k % 2 == 0:
            ra.append(t1("d29b", c)); rb.append(t1("d29", c))
        else:
            rb.append(t1("d29", c)); ra.append(t1("d29b", c))
    r = statistics.median(y / x for x, y in zip(ra, rb))
    ratios.append(r)
    print("%-30s %9.1f %9.1f %8.4f"
          % (c, 1000 * statistics.median(ra), 1000 * statistics.median(rb), r),
          flush=True)
g = 1.0
for r in ratios:
    g *= r
print("geomean = %.4f" % (g ** (1.0 / len(ratios))))
