#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""proto_run.py —— 在 VM 上编译 + 校验 + perf 计量 proto/bin_sb*.cpp

用法(须 py311, 有 paramiko):
    "C:/Program Files/Python311/python.exe" proto_run.py [all|build|verify|perf] [bin_sb2]

对照基准 (D47 十进制路径, 已实测 rnz_01):
    总 232.4M instr / 126.9M cycles;  absSubMul1 聚合 = 112.9M Ir / 66.4M cyc
    模型算 schoolbook 段 88.4M Ir

净值口径:
    v1 (bin_sb) : stat = 读文件+切token+str2dec  =>  net = dec2bin + divrem + bin2dec
    v2 (bin_sb2): stat = 读文件+切token          =>  net = str2bin + divrem + bin2str (真集成形态)
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "lc_bench", "vm"))
from vmctl import put, run  # noqa: E402

IN = "/home/azzr/lcp/big_integer/division_of_big_integers/in"
CASES = ["r_nearly_zero_01", "burnikel_ziegler_bound_02"]
cmd = sys.argv[1] if len(sys.argv) > 1 else "all"
TGT = sys.argv[2] if len(sys.argv) > 2 else "bin_sb2"

if cmd in ("all", "build"):
    put(os.path.join(HERE, "proto", TGT + ".cpp"), "/home/azzr/proto/%s.cpp" % TGT)
    rc, out, err = run("mkdir -p /home/azzr/proto && cd /home/azzr/proto && "
                       "g++ -O2 -std=c++17 -march=x86-64-v3 %s.cpp -o %s 2>&1 | head -40; "
                       "ls -la %s" % (TGT, TGT, TGT), timeout=300)
    print(out or err)
    if ("rw" not in (out or "")) and ("rwx" not in (out or "")):
        print("BUILD FAILED")
        sys.exit(1)

if cmd in ("all", "verify"):
    print("\n===== 规模统计 + 正确性校验 =====", flush=True)
    for c in CASES:
        rc, out, err = run("cd /home/azzr/proto && ./%s %s/%s.in stat && "
                           "./%s %s/%s.in verify" % (TGT, IN, c, TGT, IN, c), timeout=900)
        print("[%s]\n%s" % (c, out), flush=True)


def perf_block(tgt, cases):
    sh = ""
    for c in cases:
        for m in ("stat", "run"):
            sh += ("for i in 1 2 3 4 5; do perf stat -x, -e instructions,cycles "
                   "/home/azzr/proto/%s %s/%s.in %s 2>&1 >/dev/null | "
                   "grep -E ',instructions|,cycles' | cut -d, -f1 | tr '\\n' ' ' ; "
                   "echo '<- %s|%s|%s'; done; " % (tgt, IN, c, m, tgt, c, m))
    rc, out, err = run("cd /home/azzr/proto && " + sh, timeout=900)
    return out or err


if cmd in ("all", "perf"):
    print("\n===== perf 计量 (5 轮取 min) =====", flush=True)
    tgts = [TGT] if TGT != "both" else ["bin_sb", "bin_sb2"]
    agg = {}
    raw = ""
    for t in tgts:
        o = perf_block(t, CASES)
        raw += o
        for line in o.splitlines():
            mm = re.match(r"\s*(\d+)\s+(\d+)\s*<-\s*(\S+)\|(\S+)\|(\S+)", line)
            if not mm:
                continue
            ins, cyc, tt, cc, mo = int(mm.group(1)), int(mm.group(2)), mm.group(3), mm.group(4), mm.group(5)
            key = (tt, cc, mo)
            if key not in agg or ins < agg[key][0]:
                agg[key] = (ins, cyc)
    print(raw)
    print("\n===== 净成本 (run - stat, min-of-5) =====")
    print("%-10s %-30s %14s %14s" % ("target", "case", "net instr", "net cycles"))
    for t in tgts:
        for c in CASES:
            if (t, c, "run") in agg and (t, c, "stat") in agg:
                ni = agg[(t, c, "run")][0] - agg[(t, c, "stat")][0]
                nc = agg[(t, c, "run")][1] - agg[(t, c, "stat")][1]
                print("%-10s %-30s %14s %14s" % (t, c, "{:,}".format(ni), "{:,}".format(nc)))
    print("\n对照: D47 rnz_01 absSubMul1 = 112,900,000 Ir / 66,400,000 cyc")
