#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""D17 codelet 配置扫描: 构建多组 (FFT_FIXED_MAX, AI_SMALL, AI_DISPATCH) 并跑 Zen3 est。"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run  # noqa: E402

PS = r"D:\precious_speed"
HERE = os.path.dirname(os.path.abspath(__file__))
F = "-O2 -std=c++23 -march=x86-64-v3"

# (name, FIXED_MAX, AI_SMALL, AI_DISPATCH)
CFG = [
    ("c16_00", 16, 0, 0),
    ("c32_00", 32, 0, 0),
    ("c64_00", 64, 0, 0),
    ("c16_01", 16, 0, 1),
    ("c32_01", 32, 0, 1),
    ("c64_01", 64, 0, 1),
]
CASES = sys.argv[1] if len(sys.argv) > 1 else 'length_ratio_integer_03'
TAG = sys.argv[2] if len(sys.argv) > 2 else 'sweep'

put(os.path.join(PS, "best", "div_D17.cpp"), "/home/azzr/divbench/src/d17.cpp")
cmds = ["cd ~/divbench/src"]
for n, m, s, d in CFG:
    cmds.append(f"g++ {F} -DFFT_FIXED_MAX={m} -DFFT_AI_SMALL={s} -DFFT_AI_DISPATCH={d} "
                f"-o ../bin/{n} d17.cpp || echo BUILDFAIL_{n}")
cmds.append("echo BUILT")
rc, out, err = run(" ; ".join(cmds), timeout=7200)
print(out[-2500:]); print(err[-2500:])
if "BUILT" not in out or "BUILDFAIL" in out:
    sys.exit("build failed")

# 26 例快速对拍
names = ",".join("'%s'" % c[0] for c in CFG)
md5 = r'''cd ~/divbench && python3 - <<'EOF'
import subprocess, os, hashlib
IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
cases = sorted(f[:-3] for f in os.listdir(IN) if f.endswith('.in'))
gold = {}
for c in cases:
    with open(IN + c + '.in', 'rb') as f:
        p = subprocess.run(['./bin/d13'], stdin=f, capture_output=True)
    gold[c] = hashlib.md5(p.stdout).hexdigest()
for b in [''' + names + r''']:
    bad = [c for c in cases
           if hashlib.md5(subprocess.run(['./bin/'+b], stdin=open(IN+c+'.in','rb'),
                                         capture_output=True).stdout).hexdigest() != gold[c]]
    print('== %-8s : %s' % (b, bad if bad else 'ALL MATCH'))
EOF'''
rc, o0, e = run(md5, timeout=3600)
print(o0)

put(os.path.join(HERE, "_d17_cg_remote.py"), "/home/azzr/divbench/_d17_cg_remote.py")
bins = "d16," + ",".join(c[0] for c in CFG)
rc, o1, e = run("cd ~/divbench && python3 _d17_cg_remote.py '%s' '%s'" % (bins, CASES),
                timeout=14400)
print(o1)
with open(os.path.join(HERE, "_d17_sweep_%s.log" % TAG), "w", encoding="utf-8") as f:
    f.write(o0 + "\n" + o1)
