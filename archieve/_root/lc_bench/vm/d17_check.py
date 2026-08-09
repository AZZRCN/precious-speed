#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""D17 = D16 + FFTW 式 codelet (split-radix 子树编译期展开, 递归内 expand 移除)。
   1) 编译 (含 FFT_FIXED_MAX 扫描)
   2) 26 例官方 md5 对拍 (vs d13 黄金基准)
   3) 递归调用次数对比 (callgrind calls=)
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run  # noqa: E402

PS = r"D:\precious_speed"
HERE = os.path.dirname(os.path.abspath(__file__))
F = "-O2 -std=c++23 -march=x86-64-v3"

put(os.path.join(PS, "best", "div_D17.cpp"), "/home/azzr/divbench/src/d17.cpp")

cmds = [f"cd ~/divbench/src"]
for m in (8, 16, 32, 64, 128):
    cmds.append(f"g++ {F} -DFFT_FIXED_MAX={m} -o ../bin/d17_{m} d17.cpp "
                f"|| echo BUILDFAIL_{m}")
cmds.append("echo BUILT")
rc, out, err = run(" ; ".join(cmds), timeout=5400)
print(out[-4000:])
print(err[-4000:])
if "BUILT" not in out or "BUILDFAIL" in out:
    sys.exit("build failed")

md5 = r'''cd ~/divbench && python3 - <<'EOF'
import subprocess, os, hashlib
IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
cases = sorted(f[:-3] for f in os.listdir(IN) if f.endswith('.in'))
gold = {}
for c in cases:
    with open(IN + c + '.in', 'rb') as f:
        p = subprocess.run(['./bin/d13'], stdin=f, capture_output=True)
    gold[c] = hashlib.md5(p.stdout).hexdigest()
for b in ('d17_8','d17_16','d17_32','d17_64','d17_128'):
    bad = []
    for c in cases:
        with open(IN + c + '.in', 'rb') as f:
            p = subprocess.run(['./bin/' + b], stdin=f, capture_output=True)
        if hashlib.md5(p.stdout).hexdigest() != gold[c]:
            bad.append(c)
    print('== %-8s vs d13 (26 cases): %s' % (b, bad if bad else 'ALL MATCH'))
EOF'''
rc, o1, e = run(md5, timeout=3600)
print(o1)
print(e[-2000:])

with open(os.path.join(HERE, "_d17_check.log"), "w", encoding="utf-8") as f:
    f.write(o1)
