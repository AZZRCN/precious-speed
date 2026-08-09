#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""D14 第一轮验证（零计时）:
   1) 编译 d14 / d14p / d14probe
   2) 26 例 md5 对拍 vs d13
   3) 探针自洽性: _err 应恒等于 -_true, 且 gmp_incr 的误报量化
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run  # noqa: E402

PS = r"D:\precious_speed"
IN = "/home/azzr/lcp/big_integer/division_of_big_integers/in/"
F = "-O2 -std=c++23 -march=x86-64-v3"

put(os.path.join(PS, "best", "div_D14.cpp"), "/home/azzr/divbench/src/d14.cpp")

build = (f"cd ~/divbench/src && "
         f"g++ {F} -o ../bin/d14 d14.cpp || echo BUILDFAIL_d14; "
         f"g++ {F} -DGATE_POW2 -o ../bin/d14p d14.cpp || echo BUILDFAIL_d14p; "
         f"g++ {F} -DUNWRAP_PROBE -o ../bin/d14probe d14.cpp || echo BUILDFAIL_probe; "
         f"echo BUILT")
rc, out, err = run(build, timeout=3600)
print(out[-4000:])
print(err[-3000:])
if "BUILT" not in out or "BUILDFAIL" in out:
    sys.exit("build failed")

remote = r'''cd ~/divbench && python3 - <<'EOF'
import subprocess, os, hashlib
IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
cases = sorted(f[:-3] for f in os.listdir(IN) if f.endswith('.in'))
print('== 26-case md5 vs d13 ==')
badp, bad = [], []
for c in cases:
    hs = {}
    for b in ('d13', 'd14p', 'd14'):
        with open(IN + c + '.in', 'rb') as f:
            p = subprocess.run(['./bin/' + b], stdin=f, capture_output=True)
        hs[b] = hashlib.md5(p.stdout).hexdigest()
    if hs['d14p'] != hs['d13']: badp.append(c)
    if hs['d14']  != hs['d13']: bad.append(c)
print('  d14p (exact-unwrap, pow2 gate) mismatches:', badp if badp else 'NONE')
print('  d14  (exact-unwrap, fft3 gate) mismatches:', bad  if bad  else 'NONE')
EOF'''
rc, out1, err = run(remote, timeout=2400)
print(out1)

probe = r'''cd ~/divbench && for c in medium_00 medium_01 medium_02 max_00 min_00 length_ratio_integer_00; do
  f=/home/azzr/lcp/big_integer/division_of_big_integers/in/$c.in
  [ -f $f ] || continue
  ./bin/d14probe < $f >/dev/null 2>/tmp/pb.txt
  n=$(wc -l < /tmp/pb.txt)
  echo "--- $c  probe_lines=$n"
  sort /tmp/pb.txt | uniq -c | sort -rn | head -4
done'''
rc, out2, err = run(probe, timeout=2400)
print(out2)

with open(os.path.join(PS, "lc_bench", "vm", "_d14_verify1.log"), "w", encoding="utf-8") as f:
    f.write(out1 + "\n" + out2)
