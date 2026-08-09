#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""proto_base.py —— 对比 base-1e16 / base-1e19 两种大基数 schoolbook

base-1e16 = (10^4)^4, 与 D47 的 base-1e4 精确整除对齐 => 打包/解包纯线性, 易集成。
base-1e19 理论最优但无法与 base-1e4 互转。

用法: "C:/Program Files/Python311/python.exe" proto_base.py
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "lc_bench", "vm"))
from vmctl import put, run  # noqa: E402

IN = "/home/azzr/lcp/big_integer/division_of_big_integers/in"
CASES = ["r_nearly_zero_01", "burnikel_ziegler_bound_02"]
BASES = [16, 19]
MODES = ["stat", "p", "pd", "run"]

put(os.path.join(HERE, "proto", "decN_sb.cpp"), "/home/azzr/proto/decN_sb.cpp")

build = ""
for b in BASES:
    build += ("g++ -O2 -std=c++17 -march=x86-64-v3 -DDBASE=%d decN_sb.cpp -o decN%d 2>&1 | head -12; " % (b, b))
rc, o, e = run("cd /home/azzr/proto && " + build + "ls -la decN16 decN19", timeout=600)
print(o or e)
if "decN16" not in (o or "") or "decN19" not in (o or ""):
    sys.exit(1)

print("\n===== 正确性 =====")
sh = ""
for b in BASES:
    for c in CASES:
        sh += "echo -n 'DBASE=%d %s : '; ./decN%d %s/%s.in verify 2>&1 >/dev/null | tail -1; " % (b, c, b, IN, c)
rc, o, e = run("cd /home/azzr/proto && " + sh, timeout=1200)
print(o or e)

print("\n===== limb-ops =====")
sh = ""
for b in BASES:
    sh += "./decN%d %s/%s.in stat; " % (b, IN, CASES[0])
rc, o, e = run("cd /home/azzr/proto && " + sh, timeout=300)
print(o or e)

print("\n===== perf 分阶段 (7 轮取 min) =====")
sh = ""
for b in BASES:
    for c in CASES:
        for m in MODES:
            sh += ("for i in 1 2 3 4 5 6 7; do perf stat -x, -e instructions,cycles "
                   "./decN%d %s/%s.in %s 2>&1 >/dev/null | grep -E ',instructions|,cycles' | "
                   "cut -d, -f1 | tr '\\n' ' '; echo '<- %d|%s|%s'; done; " % (b, IN, c, m, b, c, m))
rc, o, e = run("cd /home/azzr/proto && " + sh, timeout=1800)
txt = o or e

agg = {}
for line in txt.splitlines():
    mm = re.match(r"\s*(\d+)\s+(\d+)\s*<-\s*(\d+)\|(\S+)\|(\S+)", line)
    if not mm:
        continue
    ins, cyc, bb, cc, mo = int(mm.group(1)), int(mm.group(2)), int(mm.group(3)), mm.group(4), mm.group(5)
    k = (bb, cc, mo)
    if k not in agg or ins < agg[k][0]:
        agg[k] = (ins, cyc)

LBL = [("str->limb", "stat", "p"), ("divrem", "p", "pd"), ("limb->str", "pd", "run")]
for c in CASES:
    print("\n--- %s ---" % c)
    print("%-10s %-11s %13s %13s" % ("base", "phase", "instr", "cycles"))
    for b in BASES:
        if (b, c, "run") not in agg:
            continue
        for nm, x, y in LBL:
            di = agg[(b, c, y)][0] - agg[(b, c, x)][0]
            dc = agg[(b, c, y)][1] - agg[(b, c, x)][1]
            print("%-10s %-11s %13s %13s" % ("1e%d" % b, nm, "{:,}".format(di), "{:,}".format(dc)))
        ti = agg[(b, c, "run")][0] - agg[(b, c, "stat")][0]
        tc = agg[(b, c, "run")][1] - agg[(b, c, "stat")][1]
        print("%-10s %-11s %13s %13s" % ("1e%d" % b, "TOTAL", "{:,}".format(ti), "{:,}".format(tc)))
print("\n对照 D47 base-1e4 AVX2 (同一批 case, 9.07 instr / 5.34 cyc 每 limb-op 实测):")
print("  rnz_01 : 10,917,305 limb-ops -> 99.0M instr / 58.3M cyc")
print("  bz_02  :    603,437 limb-ops ->  5.47M instr /  3.22M cyc")
