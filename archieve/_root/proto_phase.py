#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""proto_phase.py —— v2 原型分阶段 perf 归因

阶段: stat(读+切token) -> p(+str2bin) -> pd(+bin_divrem) -> run(+bin2str)
用法: "C:/Program Files/Python311/python.exe" proto_phase.py
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "lc_bench", "vm"))
from vmctl import run  # noqa: E402

IN = "/home/azzr/lcp/big_integer/division_of_big_integers/in"
CASES = ["r_nearly_zero_01", "burnikel_ziegler_bound_02"]
MODES = ["stat", "p", "pd", "run"]
TGT = sys.argv[1] if len(sys.argv) > 1 else "bin_sb2"

sh = ""
for c in CASES:
    for m in MODES:
        sh += ("for i in 1 2 3 4 5 6 7; do perf stat -x, -e instructions,cycles "
               "/home/azzr/proto/%s %s/%s.in %s 2>&1 >/dev/null | "
               "grep -E ',instructions|,cycles' | cut -d, -f1 | tr '\\n' ' ' ; "
               "echo '<- %s|%s'; done; " % (TGT, IN, c, m, c, m))
rc, out, err = run("cd /home/azzr/proto && " + sh, timeout=900)
txt = out or err

agg = {}
for line in txt.splitlines():
    mm = re.match(r"\s*(\d+)\s+(\d+)\s*<-\s*(\S+)\|(\S+)", line)
    if not mm:
        continue
    ins, cyc, cc, mo = int(mm.group(1)), int(mm.group(2)), mm.group(3), mm.group(4)
    k = (cc, mo)
    if k not in agg or ins < agg[k][0]:
        agg[k] = (ins, cyc)

LBL = [("str2bin", "stat", "p"), ("bin_divrem", "p", "pd"), ("bin2str", "pd", "run")]
for c in CASES:
    if (c, "run") not in agg:
        continue
    tot_i = agg[(c, "run")][0] - agg[(c, "stat")][0]
    tot_c = agg[(c, "run")][1] - agg[(c, "stat")][1]
    print("\n===== %s =====  净 %s instr / %s cyc" % (c, "{:,}".format(tot_i), "{:,}".format(tot_c)))
    print("%-12s %14s %7s %14s %7s" % ("phase", "instr", "%", "cycles", "%"))
    for nm, a, b in LBL:
        if (c, a) not in agg or (c, b) not in agg:
            continue
        di = agg[(c, b)][0] - agg[(c, a)][0]
        dc = agg[(c, b)][1] - agg[(c, a)][1]
        print("%-12s %14s %6.1f%% %14s %6.1f%%" % (
            nm, "{:,}".format(di), 100.0 * di / max(tot_i, 1),
            "{:,}".format(dc), 100.0 * dc / max(tot_c, 1)))
print("\n对照 D47 十进制: rnz_01 schoolbook 内环模型 88.4M Ir; absSubMul1 聚合 112.9M Ir / 66.4M cyc")
