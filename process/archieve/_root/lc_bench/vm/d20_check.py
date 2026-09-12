#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""D20 = D19 + carryNormalize (magic-2^52 位提取代替 vcvttsd2si + 分支)。
   编译 -> 26 例对拍 (vs d13) -> Zen3 est (d19 vs d20)。全程零 wall-clock。
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run  # noqa: E402

PS = r"D:\precious_speed"
HERE = os.path.dirname(os.path.abspath(__file__))
F = "-O2 -std=c++23 -march=x86-64-v3"
CASES = sys.argv[1] if len(sys.argv) > 1 else \
    'length_ratio_integer_03,r_nearly_zero_01'
TAG = sys.argv[2] if len(sys.argv) > 2 else 'q'

put(os.path.join(PS, "best", "div_D20.cpp"), "/home/azzr/divbench/src/d20.cpp")
rc, out, err = run(f"cd ~/divbench/src && g++ {F} -o ../bin/d20 d20.cpp "
                   f"&& echo BUILT", timeout=5400)
print(out[-2000:])
print(err[-8000:])
if "BUILT" not in out:
    sys.exit("build failed")

md5 = r'''cd ~/divbench && python3 - <<'EOF'
import subprocess, os, hashlib
IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
cases = sorted(f[:-3] for f in os.listdir(IN) if f.endswith('.in'))
bad = []
for c in cases:
    hs = {}
    for b in ('d13', 'd20'):
        with open(IN + c + '.in', 'rb') as f:
            p = subprocess.run(['./bin/' + b], stdin=f, capture_output=True)
        hs[b] = hashlib.md5(p.stdout).hexdigest()
    if hs['d20'] != hs['d13']: bad.append(c)
print('== d20 vs d13 (26 cases):', bad if bad else 'ALL MATCH')
EOF'''
rc, o0, e = run(md5, timeout=3600)
print(o0)

put(os.path.join(HERE, "_d17_cg_remote.py"), "/home/azzr/divbench/_d17_cg_remote.py")
rc, o1, e = run("cd ~/divbench && python3 _d17_cg_remote.py 'd19,d20' '%s'" % CASES,
                timeout=14400)
print(o1)
with open(os.path.join(HERE, "_d20_%s.log" % TAG), "w", encoding="utf-8") as f:
    f.write(o0 + "\n" + o1)
