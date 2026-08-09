# -*- coding: utf-8 -*-
"""用 --separate-callers=3 穿透 PLT, 定位 memset/memcpy 的真实调用者。
用法: python _memsrc2.py <bin> <case> [depth]
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import run

BIN = sys.argv[1] if len(sys.argv) > 1 else 'd39'
CASE = sys.argv[2] if len(sys.argv) > 2 else 'length_ratio_integer_00'
DEPTH = sys.argv[3] if len(sys.argv) > 3 else '3'
IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in'
OUT = '/tmp/cgS_%s_%s.out' % (BIN, CASE)

cmd = ('cd /home/azzr/divbench && valgrind --tool=callgrind --separate-callers=%s '
       '--callgrind-out-file=%s --cache-sim=no ./bin/%s < %s/%s.in > /dev/null 2>/tmp/cgS.log; '
       'echo RC=$?; ls -la %s' % (DEPTH, OUT, BIN, IN, CASE, OUT))
rc, o, e = run(cmd, timeout=2400, verbose=False)
print('[run]', o.strip()[-300:], e[-300:])

ANA = r'''
import re, sys, collections
path = "%s"
names = {}
cur = None
tot = collections.Counter()
# callgrind format: fn=(id) name ; cost lines follow
fnid = {}
last_fn = None
with open(path, "r", errors="replace") as f:
    for line in f:
        line = line.rstrip("\n")
        m = re.match(r"^fn=\((\d+)\)(?: (.*))?$", line)
        if m:
            i = m.group(1)
            if m.group(2):
                fnid[i] = m.group(2)
            last_fn = fnid.get(i, "?")
            continue
        if line and (line[0].isdigit() or line[0] in "+-*"):
            parts = line.split()
            if len(parts) >= 2 and last_fn:
                try:
                    tot[last_fn] += int(parts[1])
                except ValueError:
                    pass
rows = [(v, k) for k, v in tot.items() if ("memset" in k or "memcpy" in k or "memmove" in k)]
rows.sort(reverse=True)
print("==== self Ir of mem* (with caller chain) ====")
for v, k in rows[:40]:
    print("%%14d  %%s" %% (v, k[:190]))
''' % OUT

open('/tmp/_ana.py', 'w')  # placeholder (local no-op)
import base64
b64 = base64.b64encode(ANA.encode()).decode()
cmd2 = ("cd /tmp && echo %s | base64 -d > _ana_mem.py && python3 _ana_mem.py" % b64)
rc, o, e = run(cmd2, timeout=600, verbose=False)
print(o)
print('ERR', e[-400:])
