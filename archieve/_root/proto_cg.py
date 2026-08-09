#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""proto_cg.py —— 对原型做 callgrind 行级归因 (含内联, 靠 -g 行号定位)

用法: "C:/Program Files/Python311/python.exe" proto_cg.py [dec19_sb] [case]
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "lc_bench", "vm"))
from vmctl import run  # noqa: E402

IN = "/home/azzr/lcp/big_integer/division_of_big_integers/in"
TGT = sys.argv[1] if len(sys.argv) > 1 else "dec19_sb"
CASE = sys.argv[2] if len(sys.argv) > 2 else "r_nearly_zero_01"

cmd = (
    "cd /home/azzr/proto && "
    "g++ -O2 -g -std=c++17 -march=x86-64-v3 %s.cpp -o %s_g 2>&1 | head -20 && "
    "valgrind --tool=callgrind --callgrind-out-file=cg_%s.out "
    "./%s_g %s/%s.in run >/dev/null 2>&1; "
    "callgrind_annotate --auto=yes --threshold=92 cg_%s.out 2>/dev/null | "
    "grep -E '^ *[0-9,]+ ' | head -45"
    % (TGT, TGT, TGT, TGT, IN, CASE, TGT)
)
rc, out, err = run(cmd, timeout=1800)
print(out or err)
