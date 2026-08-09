#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""callgrind A/B 配对仲裁 —— 同一份 case 上同时测多个 bin, 输出零方差比值。

为什么: LC 墙钟有 ±25% 抖动 (同一份 D31 两次提交 33ms / 35ms, 单点 spread 2.25x),
        max 指标被单点抖动主导, 不能用来比版本。callgrind 是确定性模拟,
        配置成 Zen3 cache 后即是可靠仲裁。

用法:
    python cg_ab.py d31 d39            # 默认 12 个瓶颈 case
    python cg_ab.py d31 d39 -- case... # 指定 case
    python cg_ab.py poll
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
LOG = "/home/azzr/divbench/logs/cg_ab.log"

if len(sys.argv) > 1 and sys.argv[1] == "poll":
    rc, out, err = run(f"cat {LOG} 2>/dev/null; echo '--- alive:'; pgrep -fa cg_ab.py | head -3")
    print(out)
    sys.exit(0)

argv = sys.argv[1:]
if "--" in argv:
    k = argv.index("--")
    BINS, CASES = argv[:k], argv[k + 1:]
else:
    BINS, CASES = argv, []
if not BINS:
    print(__doc__)
    sys.exit(1)
if not CASES:
    # LC 回执里耗时最高的 12 个点 (两次提交取并集)
    CASES = [
        "burnikel_ziegler_bound_00", "burnikel_ziegler_bound_02",
        "burnikel_ziegler_bound_03", "a_max_b_random_01", "a_max_b_random_02",
        "r_nearly_zero_01", "length_ratio_integer_00", "length_ratio_integer_02",
        "length_ratio_integer_05", "medium_02", "large_01", "medium_01",
    ]

REMOTE = r'''#!/usr/bin/env python3
import subprocess, os, sys
from concurrent.futures import ThreadPoolExecutor
IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
BINS = %r
CASES = %r
CACHE = '--cache-sim=yes --D1=32768,8,64 --I1=32768,8,64 --LL=33554432,16,64'

def measure(args):
    b, c = args
    outf = '/tmp/cgab_%%s_%%s.out' %% (b, c)
    cmd = ('valgrind --tool=callgrind ' + CACHE +
           ' --callgrind-out-file=' + outf + ' ./bin/' + b +
           ' < ' + IN + c + '.in > /dev/null 2>/dev/null')
    if subprocess.run(cmd, shell=True, cwd='/home/azzr/divbench').returncode != 0:
        return (b, c, None)
    ev, tot = None, None
    with open(outf) as f:
        for line in f:
            if line.startswith('events:'):
                ev = line.split()[1:]
            elif line.startswith('summary:') or line.startswith('totals:'):
                tot = [int(x) for x in line.split()[1:]]
    os.remove(outf)
    if not ev or not tot:
        return (b, c, None)
    return (b, c, dict(zip(ev, tot)))

jobs = [(b, c) for c in CASES for b in BINS if os.path.exists(IN + c + '.in')]
res = {}
with ThreadPoolExecutor(max_workers=3) as ex:
    for b, c, d in ex.map(measure, jobs):
        res[(b, c)] = d
        print('  done %%s/%%s' %% (b, c), flush=True)

def est(d):
    if not d: return None
    Ir = d.get('Ir', 0)
    d1 = d.get('D1mr', 0) + d.get('D1mw', 0)
    dl = d.get('DLmr', 0) + d.get('DLmw', 0)
    return Ir + 5 * d1 + 200 * dl, Ir, d1, dl

base = BINS[0]
print('', flush=True)
hdr = '%%-28s' %% 'case'
for b in BINS: hdr += ' %%13s' %% b
for b in BINS[1:]: hdr += ' %%8s' %% ('r:' + b)
print(hdr, flush=True); print('-' * len(hdr), flush=True)
ratios = {b: [] for b in BINS[1:]}
tot = {b: 0 for b in BINS}
for c in CASES:
    if (base, c) not in res: continue
    e0 = est(res[(base, c)])
    if not e0: continue
    line = '%%-28s' %% c
    for b in BINS:
        e = est(res.get((b, c)))
        line += ' %%13d' %% (e[0] if e else -1)
        if e: tot[b] += e[0]
    for b in BINS[1:]:
        e = est(res.get((b, c)))
        if e:
            r = e[0] / e0[0]; ratios[b].append(r)
            line += ' %%8.4f' %% r
        else:
            line += ' %%8s' %% '-'
    print(line, flush=True)
print('-' * len(hdr), flush=True)
line = '%%-28s' %% 'TOTAL'
for b in BINS: line += ' %%13d' %% tot[b]
for b in BINS[1:]: line += ' %%8.4f' %% (tot[b] / tot[base] if tot[base] else 0)
print(line, flush=True)
import statistics
for b in BINS[1:]:
    if ratios[b]:
        rs = sorted(ratios[b])
        print('  %%s: median=%%.4f  min=%%.4f  max=%%.4f  (n=%%d)' %%
              (b, statistics.median(rs), rs[0], rs[-1], len(rs)), flush=True)
# 每个 bin 的瓶颈点 (LC max 指标由它决定)
print('', flush=True)
for b in BINS:
    rows = [(c, est(res[(b, c)])[0]) for c in CASES if res.get((b, c))]
    rows.sort(key=lambda x: -x[1])
    print('  %%s bottleneck: %%s' %% (b, ', '.join('%%s=%%d' %% (c, v) for c, v in rows[:3])), flush=True)
print('DONE', flush=True)
''' % (BINS, CASES)

p = os.path.join(HERE, "_cg_ab.py")
with open(p, "w", encoding="utf-8", newline="\n") as f:
    f.write(REMOTE)
put(p, "/home/azzr/cg_ab.py")
run("mkdir -p /home/azzr/divbench/logs && rm -f " + LOG)
run(f"cd /home/azzr && nohup python3 cg_ab.py > {LOG} 2>&1 & echo started", timeout=30)
print(f"started cg_ab: bins={BINS}  cases={len(CASES)}")
print("poll with: python cg_ab.py poll")
