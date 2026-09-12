#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""D15 = D14 + nb 代价模型口径一致化。
   1) 26 例 md5 对拍 (vs d13 黄金基准)
   2) MUSHAPE 形状: 确认 lri_02 nb 6->7, lri_00 nb 2->3
   3) callgrind Ir 对比 d14 vs d15 (零计时评估)
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run  # noqa: E402

PS = r"D:\precious_speed"
HERE = os.path.dirname(os.path.abspath(__file__))
F = "-O2 -std=c++23 -march=x86-64-v3"

put(os.path.join(PS, "best", "div_D16.cpp"), "/home/azzr/divbench/src/d16.cpp")
rc, out, err = run(f"cd ~/divbench/src && "
                   f"g++ {F} -o ../bin/d16 d16.cpp || echo BUILDFAIL_d15; "
                   f"g++ {F} -DDIV_MUSHAPE -o ../bin/d16s d16.cpp || echo BUILDFAIL_d16s; "
                   f"echo BUILT", timeout=3600)
print(out[-3000:]); print(err[-2500:])
if "BUILT" not in out or "BUILDFAIL" in out:
    sys.exit("build failed")

md5 = r'''cd ~/divbench && python3 - <<'EOF'
import subprocess, os, hashlib
IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
cases = sorted(f[:-3] for f in os.listdir(IN) if f.endswith('.in'))
bad = []
for c in cases:
    hs = {}
    for b in ('d13', 'd16'):
        with open(IN + c + '.in', 'rb') as f:
            p = subprocess.run(['./bin/' + b], stdin=f, capture_output=True)
        hs[b] = hashlib.md5(p.stdout).hexdigest()
    if hs['d16'] != hs['d13']: bad.append(c)
print('== d16 vs d13 (26 cases):', bad if bad else 'ALL MATCH')
EOF'''
rc, o1, e = run(md5, timeout=2400)
print(o1)

shape = r'''cd ~/divbench
IN=/home/azzr/lcp/big_integer/division_of_big_integers/in
for c in length_ratio_integer_00 length_ratio_integer_02 length_ratio_integer_03 \
         length_ratio_integer_01 a_max_b_random_02; do
  ./bin/d16s < $IN/$c.in >/dev/null 2>/tmp/s15.txt
  echo "=== $c"
  grep '\[mushape\]' /tmp/s15.txt | sort | uniq -c | sort -rn | head -2
done'''
rc, o2, e = run(shape, timeout=2400)
print(o2)

with open(os.path.join(PS, "lc_bench", "vm", "_d16_check.log"), "w", encoding="utf-8") as f:
    f.write(o1 + "\n" + o2)
