#!/usr/bin/env python3
"""D13: dump absDivMu 的形状参数 (len1/len2/in/nblk/Fi/Fl/Fc/cyc), 对比 nb+DELTA。

目的: 弄清 a_max_b_random_02 在 nb+1 下为何 -21.6% —— 哪个档位掉了级。
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run  # noqa: E402

PS = r"D:\precious_speed"
CASES = sys.argv[1:] or ["a_max_b_random_02", "length_ratio_integer_01",
                         "length_ratio_integer_02", "length_ratio_integer_03",
                         "length_ratio_integer_04", "a_max_b_random_01"]
DELTAS = [0, 1, 2]

put(os.path.join(PS, "best", "div_D13.cpp"), "/home/azzr/divbench/src/div_D13.cpp")

build = "cd ~/divbench/src && "
for d in DELTAS:
    flag = "" if d == 0 else f"-DDIV_MU_NB_DELTA={d} "
    build += (f"g++ -O2 -std=c++23 -march=x86-64-v3 -DDIV_MUSHAPE {flag}"
              f"-o ../bin/sh{d} div_D13.cpp || echo BUILDFAIL_{d}; ")
build += "echo BUILT"
rc, out, err = run(build, timeout=3600)
print(out[-2500:])
if "BUILT" not in out:
    sys.exit("build failed")

REMOTE = r'''
import subprocess, sys
from collections import Counter
from pathlib import Path
IN = Path('/home/azzr/lcp/big_integer/division_of_big_integers/in')
DELTAS = [0, 1, 2]
for CASE in sys.argv[1:]:
    print('\n########## %s ##########' % CASE)
    for d in DELTAS:
        with open(IN / (CASE + '.in'), 'rb') as f:
            p = subprocess.run(['/home/azzr/divbench/bin/sh%d' % d], stdin=f,
                               capture_output=True, timeout=900)
        lines = [l for l in p.stderr.decode('utf-8', 'replace').splitlines()
                 if '[mushape]' in l]
        cnt = Counter(lines)
        tot = 0.0
        for l in lines:
            for tok in l.split():
                if tok.startswith('blkW='):
                    tot += float(tok[5:])
        print('  --- delta=%d   calls=%d   sum(blkW)=%.4g' % (d, len(lines), tot))
        for l, n in cnt.most_common(6):
            print('    x%-4d %s' % (n, l.replace('[mushape] ', '')))
'''
tmp = os.path.join(PS, "lc_bench", "vm", "_shape_remote.py")
with open(tmp, "w", encoding="utf-8") as f:
    f.write(REMOTE)
put(tmp, "/tmp/shape_remote.py")
rc, out, err = run("python3 /tmp/shape_remote.py " + " ".join(CASES), timeout=2400)
print(out)
with open(os.path.join(PS, "lc_bench", "vm", "_shape_d13.log"), "w", encoding="utf-8") as f:
    f.write(out)
