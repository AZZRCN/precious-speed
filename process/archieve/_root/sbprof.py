#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""sbprof.py —— 给 D47 打 -DSBPROF 计数器, 量出 base-1e4 schoolbook 的真实 limb-op 总数

目的: 此前一直用「8.1 指令/limb-op」的模型假设当基准, 从未实测。
     结合已测 callgrind absSubMul1 = 112.9M Ir, 可算出真实 指令/limb-op。

用法: "C:/Program Files/Python311/python.exe" sbprof.py
"""
import io
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "lc_bench", "vm"))
from vmctl import put, run  # noqa: E402

IN = "/home/azzr/lcp/big_integer/division_of_big_integers/in"
CASES = ["r_nearly_zero_01", "burnikel_ziegler_bound_02", "a_max_b_random_02"]

src = io.open(os.path.join(HERE, "best", "div_D47.cpp"), encoding="utf-8", newline="").read()
NL = "\r\n" if "\r\n" in src else "\n"

GLOB = "\n".join([
    "#ifdef SBPROF",
    "#include <cstdio>",
    "#include <cstdlib>",
    "static unsigned long long g_sb_calls = 0, g_sb_ops = 0, g_sb_iters = 0;",
    "static void sbprof_dump() {",
    '    fprintf(stderr, "[sbprof] absDivBasicCore calls=%llu  quotdigits=%llu  limb_ops=%llu\\n",',
    "            g_sb_calls, g_sb_iters, g_sb_ops);",
    "}",
    "struct SbProfInit { SbProfInit(){ atexit(sbprof_dump); } };",
    "static SbProfInit g_sbprof_init;",
    "#endif",
    "",
])

# 必须插在 absDivBasicCore(4641行) 之前 -> 直接放文件最前面
src = GLOB.replace("\n", NL) + src

CNT = "\n".join([
    "#ifdef SBPROF",
    "            g_sb_calls++; g_sb_ops += (unsigned long long)quot_idx * len2;",
    "            g_sb_iters += quot_idx;",
    "#endif",
    "",
])
anchor2 = "            size_t quot_idx = len1 - len2;" + NL
assert src.count(anchor2) == 1, src.count(anchor2)
src = src.replace(anchor2, anchor2 + CNT.replace("\n", NL), 1)

out = os.path.join(HERE, "proto", "d47_sbprof.cpp")
io.open(out, "w", encoding="utf-8", newline="").write(src)
print("patched ->", out)

put(out, "/home/azzr/proto/d47_sbprof.cpp")
rc, o, e = run("cd /home/azzr/proto && g++ -O2 -std=c++23 -march=x86-64-v3 -DSBPROF "
               "d47_sbprof.cpp -o d47_sbprof 2>&1 | head -20; ls -la d47_sbprof", timeout=900)
print(o or e)
if "No such" in (o or ""):
    sys.exit(1)

sh = ""
for c in CASES:
    sh += ("echo '### %s'; ./d47_sbprof < %s/%s.in > /dev/null; " % (c, IN, c))
rc, o, e = run("cd /home/azzr/proto && " + sh, timeout=900)
print(o or e)
