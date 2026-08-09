#!/usr/bin/env python3
"""算法效率审计: 把 DIV 的总 FFT 工作量拆成 倒数/商/余数 三段, 并量 cyclic 启用率。

用法: python3 _audit_remote.py <src_stem> <case> [<case2> ...]
  src_stem 例: div_D25   (对应 src/div_D25.cpp)
"""
import os, subprocess, sys, math

SRC = sys.argv[1] if len(sys.argv) > 1 else "div_D25"
CASES = (sys.argv[2] if len(sys.argv) > 2 else "length_ratio_integer_00").split(",")

ROOT = "/home/azzr/divbench"
IN = "/home/azzr/lcp/big_integer/division_of_big_integers/in"
CXX = "g++ -O2 -std=c++23 -march=x86-64-v3 -w"


def sh(cmd, timeout=1800):
    p = subprocess.run(cmd, shell=True, cwd=ROOT, capture_output=True,
                       text=True, timeout=timeout)
    return p.returncode, p.stdout, p.stderr


def build(tag, flags):
    exe = "%s/bin/%s_%s" % (ROOT, SRC, tag)
    if os.path.exists(exe):
        return exe
    rc, o, e = sh("%s %s %s/src/%s.cpp -o %s" % (CXX, flags, ROOT, SRC, exe))
    if rc != 0:
        print("BUILD FAIL(%s): %s" % (tag, e[:2000]))
        return None
    return exe


def main():
    inv = build("invprof", "-DINVPROF")
    prof = build("profdiv", "-DPROFILE_DIV")
    for c in CASES:
        fin = "%s/%s.in" % (IN, c)
        if not os.path.exists(fin):
            print("MISSING %s" % fin)
            continue
        print("\n" + "#" * 78)
        print("##### %s / %s" % (SRC, c))
        print("#" * 78)
        if inv:
            rc, o, e = sh("%s < %s > /dev/null" % (inv, fin), timeout=900)
            lines = [l for l in e.splitlines() if l.startswith("[invprof]")]
            print("\n--- INVPROF (Newton 逐层) ---")
            print("\n".join(lines[:30]))
            # cyclic 启用率统计
            tot = 0
            cyc = 0
            for l in lines:
                p = l.split()
                if len(p) >= 8 and p[1].isdigit():
                    n = int(p[7])
                    tot += n
                    if p[5] == "1":
                        cyc += n
            if tot:
                print("[audit] cyclic 启用层数 %d / %d = %.1f%%" % (cyc, tot, cyc * 100.0 / tot))
        if prof:
            rc, o, e = sh("%s < %s > /dev/null" % (prof, fin), timeout=900)
            lines = [l for l in e.splitlines() if "[prof]" in l or "[top]" in l]
            print("\n--- PROFILE_DIV (阶段分解) ---")
            print("\n".join(lines[:45]))


main()
