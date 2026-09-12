#!/usr/bin/env python3
"""D14 快检: 精确 unwrap + 放宽 cyclic 闸门。

三个二进制:
  d13   = 基线 (D13, 已 554 全绿)
  d14p  = D14 + -DGATE_POW2 (只有精确 unwrap, 闸门仍是 D12 的 2 幂口径)
          -> 必须与 d13 逐字节一致 (证明 incr 在已启用形状里是 no-op, 无回归)
  d14   = D14 默认 (精确 unwrap + fft3 档位闸门)
          -> 必须与 d13 逐字节一致 (证明放宽闸门后结果仍正确)

再用 -DDIV_MUSHAPE 确认 length_ratio_integer_00 从 cyc=0 变 cyc=1。
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run  # noqa: E402

PS = r"D:\precious_speed"

put(os.path.join(PS, "best", "div_D13.cpp"), "/home/azzr/divbench/src/d13.cpp")
put(os.path.join(PS, "best", "div_D14.cpp"), "/home/azzr/divbench/src/d14.cpp")

build = ("cd ~/divbench/src && "
         "g++ -O2 -std=c++23 -march=x86-64-v3 -o ../bin/d13 d13.cpp || echo BUILDFAIL_d13; "
         "g++ -O2 -std=c++23 -march=x86-64-v3 -DGATE_POW2 -o ../bin/d14p d14.cpp || echo BUILDFAIL_d14p; "
         "g++ -O2 -std=c++23 -march=x86-64-v3 -o ../bin/d14 d14.cpp || echo BUILDFAIL_d14; "
         "g++ -O2 -std=c++23 -march=x86-64-v3 -DDIV_MUSHAPE -o ../bin/d14s d14.cpp || echo BUILDFAIL_d14s; "
         "echo BUILT")
rc, out, err = run(build, timeout=3600)
print(out[-6000:])
print(err[-4000:])
if "BUILT" not in out or "BUILDFAIL" in out:
    sys.exit("build failed")

remote = r'''cd ~/divbench && python3 - <<'EOF'
import subprocess, os, hashlib
IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
cases = sorted(f[:-3] for f in os.listdir(IN) if f.endswith('.in'))
print('== 26-case md5: d14p / d14  vs  d13 ==')
badp, bad = [], []
for c in cases:
    hs = {}
    for b in ('d13', 'd14p', 'd14'):
        with open(IN + c + '.in', 'rb') as f:
            p = subprocess.run(['./bin/' + b], stdin=f, capture_output=True)
        hs[b] = hashlib.md5(p.stdout).hexdigest()
    if hs['d14p'] != hs['d13']:
        badp.append(c)
    if hs['d14'] != hs['d13']:
        bad.append(c)
print('  d14p mismatches:', badp if badp else 'NONE')
print('  d14  mismatches:', bad if bad else 'NONE')
EOF'''
rc, out, err = run(remote, timeout=1800)
print(out)

shape = (r'''cd ~/divbench && for c in length_ratio_integer_00 length_ratio_integer_01 '''
         r'''length_ratio_integer_02 length_ratio_integer_03 a_max_b_random_01 large_01; do '''
         r'''echo "--- $c"; ./bin/d14s < /home/azzr/lcp/big_integer/division_of_big_integers/in/$c.in '''
         r'''>/dev/null 2>/tmp/s.txt; sort -u /tmp/s.txt | sort -t= -k2 -rn | head -3; done''')
rc, out2, err = run(shape, timeout=1800)
print(out2)

with open(os.path.join(PS, "lc_bench", "vm", "_check_d14.log"), "w", encoding="utf-8") as f:
    f.write(out + "\n" + out2)
