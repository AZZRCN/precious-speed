#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""在 VM 上编译候选。

用法: python3 _build.py <tag>=<src.cpp>[:<extra flags>] [...]
例:   python3 _build.py d31=div_D31.cpp d31v=div_D31.cpp:-DDIV_D31_VERIFY
"""
import os
import subprocess
import sys

ROOT = "/home/azzr/divbench"
BASE = "g++ -O2 -std=c++23 -march=x86-64-v3"

for spec in sys.argv[1:]:
    tag, rest = spec.split("=", 1)
    if ":" in rest:
        src, extra = rest.split(":", 1)
    else:
        src, extra = rest, ""
    out = os.path.join(ROOT, "bin", tag)
    cmd = "%s %s src/%s -o %s" % (BASE, extra, src, out)
    p = subprocess.run(cmd, shell=True, cwd=ROOT, capture_output=True, text=True)
    ok = "OK " if p.returncode == 0 else "FAIL"
    print("[%s] %s" % (ok, cmd))
    if p.returncode != 0:
        sys.stdout.write(p.stderr[-4000:])
        sys.exit(1)
print("BUILD DONE")
