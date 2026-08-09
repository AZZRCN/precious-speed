#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""函数级 cache 分解 —— 验证 "radix-3 的 twiddle 表把 L2 打爆" 假设。

假设: dif3Stage/idit3Stage 每次调用要线性扫过 FFTTable3 的两张表
      (t1 FACTOR=1, t2 FACTOR=2), 每张 m*4 floats。
      Fi=98304 时 m=16384 -> 每表 512KB, 两表 1MB >> Zen3 L2 512KB。
      => dif3Stage 的 D1mr/Ir 比值应显著高于纯 2 幂的 dif<false>。

若成立, 优化路径: t2 可由 t1 平方得到 (w^{2n} = (w^n)^2), 省掉一张表。
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
LOG = "/home/azzr/divbench/logs/cg_cache_fn.log"

if len(sys.argv) > 1 and sys.argv[1] == "poll":
    rc, out, err = run(f"cat {LOG}")
    print(out)
    sys.exit(0)

REMOTE = r'''#!/usr/bin/env python3
import subprocess, re, os
FILES = [('fft3 ', '/tmp/ab_d16_length_ratio_integer_03.out'),
         ('nofft3', '/tmp/ab_d16_nf3_length_ratio_integer_03.out')]
WANT = ['dif3Stage', 'idit3Stage', 'dif<false>', 'idit<false>', 'dif<true>',
        'real_dot_binrev3', 'rdot', 'Float2', 'memset', 'memcpy']

for tag, f in FILES:
    if not os.path.exists(f):
        print('MISSING', f, flush=True); continue
    r = subprocess.run(['callgrind_annotate', '--threshold=99.9',
                        '--show=Ir,D1mr,D1mw,DLmr,DLmw', f],
                       capture_output=True, text=True)
    out = r.stdout
    ev = None
    for line in out.splitlines():
        if line.strip().startswith('Events shown:'):
            ev = line.split(':', 1)[1].split(); break
    if not ev:
        print('NO EVENTS; head of output:', flush=True)
        print('\n'.join(out.splitlines()[:25]), flush=True)
        continue
    iIr = ev.index('Ir'); iD1 = ev.index('D1mr'); iDL = ev.index('DLmr')
    iD1w = ev.index('D1mw') if 'D1mw' in ev else None
    print('=== %s   events=%s' % (tag, ev), flush=True)
    print('  %-24s %13s %11s %9s %8s %8s' % ('fn','Ir','D1mr','DLmr','D1mr/Ir%','est/Ir'), flush=True)
    tot = None
    rows = []
    for line in out.splitlines():
        m = re.match(r'^\s*([\d,]+(?:\s+[\d,]+)*)\s+(\S.*)$', line)
        if not m: continue
        nums = [int(x.replace(',', '')) for x in m.group(1).split()]
        if len(nums) < len(ev): continue
        name = m.group(2).strip()
        rows.append((nums, name))
    agg = {}
    for nums, name in rows:
        fn = name.split(':')[-1].strip()
        for w in WANT:
            if w in fn:
                a = agg.setdefault(w, [0]*len(ev))
                for i, v in enumerate(nums): a[i] += v
                break
    for w in WANT:
        if w not in agg: continue
        a = agg[w]
        Ir = a[iIr] or 1
        d1 = a[iD1] + (a[iD1w] if iD1w is not None else 0)
        dl = a[iDL]
        est = Ir + 5*d1 + 200*dl
        print('  %-24s %13d %11d %9d %7.3f%% %8.3f' % (w, Ir, d1, dl, 100.0*d1/Ir, est/Ir), flush=True)
    print(flush=True)
print('DONE', flush=True)
'''
p = os.path.join(HERE, "_cg_cache_fn.py")
with open(p, "w", encoding="utf-8", newline="\n") as f:
    f.write(REMOTE)
put(p, "/home/azzr/cg_cache_fn.py")
rc, out, err = run(f"cd /home/azzr && python3 cg_cache_fn.py 2>&1 | tee {LOG}", timeout=300)
print(out)
print(err)
