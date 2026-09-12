#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""26 例逐字节对拍: 任意候选 vs 参考二进制。

用法: python3 _bytecheck.py <cand>[,<cand>...] [ref=d25]

注意: 一律写成独立脚本, 不要把 python -c / awk 内联进 run() ——
      远端 shell 会吞掉引号 (历史踩过两次)。
"""
import hashlib
import os
import subprocess
import sys

ROOT = "/home/azzr/divbench"
IN = "/home/azzr/lcp/big_integer/division_of_big_integers/in"
CANDS = sys.argv[1].split(",") if len(sys.argv) > 1 else ["d30"]
REF = sys.argv[2] if len(sys.argv) > 2 else "d25"
CASES = sorted(f[:-3] for f in os.listdir(IN) if f.endswith(".in"))


def h(exe, case):
    with open("%s/%s.in" % (IN, case), "rb") as fi:
        p = subprocess.run([os.path.join(ROOT, "bin", exe)], stdin=fi,
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    return hashlib.sha256(p.stdout).hexdigest(), p.returncode


rc_all = 0
ref = {c: h(REF, c)[0] for c in CASES}
for cand in CANDS:
    bad = []
    for c in CASES:
        hh, rc = h(cand, c)
        if hh != ref[c] or rc != 0:
            bad.append(c + ("" if rc == 0 else "(rc=%d)" % rc))
    print("byte-exact %-6s vs %-5s (%d cases): %s"
          % (cand, REF, len(CASES),
             "ALL MATCH" if not bad else "MISMATCH " + ",".join(bad)))
    if bad:
        rc_all = 1
sys.exit(rc_all)
