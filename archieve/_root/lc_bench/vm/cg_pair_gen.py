#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""通用 callgrind 指令计数对比（零计时评估）。

用法:
    python cg_pair_gen.py A B [case1 case2 ...]   # 启动后台对比 bin/A vs bin/B
    python cg_pair_gen.py poll
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
LOG = "/home/azzr/divbench/logs/cg_pair.log"

if len(sys.argv) > 1 and sys.argv[1] == "poll":
    rc, out, err = run(f"cat {LOG}; echo '--- alive:'; pgrep -fa cg_pair_gen.py | head -2")
    print(out)
    sys.exit(0)

A = sys.argv[1] if len(sys.argv) > 1 else "d14"
B = sys.argv[2] if len(sys.argv) > 2 else "d15"
CASES = sys.argv[3:] or ["length_ratio_integer_02", "length_ratio_integer_03",
                         "length_ratio_integer_00", "length_ratio_integer_01",
                         "a_max_b_random_02", "large_01"]

REMOTE = '''#!/usr/bin/env python3
import subprocess, os
IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
A = %r
B = %r
CASES = %r

def ir(b, c):
    outf = '/tmp/cg_%%s_%%s.out' %% (b, c)
    cmd = ('valgrind --tool=callgrind --cache-sim=no --branch-sim=no '
           '--callgrind-out-file=' + outf + ' ./bin/' + b +
           ' < ' + IN + c + '.in > /dev/null 2>/dev/null')
    if subprocess.run(cmd, shell=True, cwd='/home/azzr/divbench').returncode != 0:
        return None
    tot = None
    with open(outf) as f:
        for line in f:
            if line.startswith('summary:') or line.startswith('totals:'):
                tot = int(line.split()[1])
    return tot

print('%%-30s %%14s %%14s %%9s' %% ('case', A + ' Ir', B + ' Ir', 'B/A'), flush=True)
print('-' * 72, flush=True)
rows = []
for c in CASES:
    if not os.path.exists(IN + c + '.in'):
        print(c + ': MISSING', flush=True); continue
    va, vb = ir(A, c), ir(B, c)
    if va and vb:
        rows.append((c, va, vb, vb / va))
        print('%%-30s %%14d %%14d %%9.4f' %% (c, va, vb, vb / va), flush=True)
    else:
        print(c + ': FAIL', flush=True)
print('-' * 72, flush=True)
if rows:
    ma = max(rows, key=lambda r: r[1])
    mb = max(rows, key=lambda r: r[2])
    print('MAX-Ir  %%s: %%s (%%d)' %% (A, ma[0], ma[1]), flush=True)
    print('MAX-Ir  %%s: %%s (%%d)' %% (B, mb[0], mb[2]), flush=True)
    print('bottleneck shift: %%.4f' %% (mb[2] / ma[1]), flush=True)
print('DONE', flush=True)
''' % (A, B, CASES)

p = os.path.join(HERE, "_cg_pair_gen.py")
with open(p, "w", encoding="utf-8", newline="\n") as f:
    f.write(REMOTE)
put(p, "/home/azzr/cg_pair_gen.py")
run("mkdir -p /home/azzr/divbench/logs && rm -f " + LOG)
run(f"cd /home/azzr && nohup python3 cg_pair_gen.py > {LOG} 2>&1 &  echo started", timeout=30)
print(f"started {A} vs {B}; poll with: python cg_pair_gen.py poll")
