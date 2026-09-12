#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""nb 扫描: 用 Zen3 cache 模拟直接实测「块数」的真实最优, 检验代价模型 A=6.8 的标定。

为什么:
  muBlockCost 里 A=6.8 (Newton 倒数 / 顶层一次变换) 是 INVPROF 逐层估的。
  A 偏大 -> 模型过度惩罚大 in -> 切太多块; A 偏小 -> 切太少。
  与其信标定, 不如把 nb 直接钉死后逐个量。DIV_MU_NB_DELTA 正好会
  **关掉模型**并令 nb = ceil(qn/len2) + DELTA, 是现成的钉死开关。

nb_min = ceil(qn_mu/len2):
  length_ratio_integer_02: qn=333333 len2=83334  -> 4  (模型选 7 => delta 3)
  length_ratio_integer_03: qn=409091 len2=45455  -> 9  (模型选 9 => delta 0)
  length_ratio_integer_01: qn=250001 len2=125000 -> 3  (模型选 4 => delta 1)
  a_max_b_random_02:       qn=293976 len2=206025 -> 2  (模型选 3 => delta 1)

全程零 wall-clock: 只用 callgrind Ir + Zen3 cache 模拟。

用法:
    python nb_sweep.py            # 启动
    python nb_sweep.py poll
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run  # noqa: E402

PS = r"D:\precious_speed"
HERE = os.path.dirname(os.path.abspath(__file__))
LOG = "/home/azzr/divbench/logs/nb_sweep.log"

if len(sys.argv) > 1 and sys.argv[1] == "poll":
    rc, out, err = run(f"cat {LOG}; echo '--- alive:'; pgrep -fa nb_sweep | head -2")
    print(out)
    sys.exit(0)

DELTAS = [0, 1, 2, 3, 4, 5]
CASES = ["length_ratio_integer_02", "length_ratio_integer_03",
         "length_ratio_integer_01", "a_max_b_random_02"]

REMOTE = '''#!/usr/bin/env python3
import subprocess, os
IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
SRC = '/home/azzr/divbench/src/d16.cpp'
F = '-O2 -std=c++23 -march=x86-64-v3'
CACHE = '--cache-sim=yes --D1=32768,8,64 --I1=32768,8,64 --LL=33554432,16,64'
DELTAS = %r
CASES = %r

def build(d):
    b = '/home/azzr/divbench/bin/nbs%%d' %% d
    cmd = 'g++ %%s -DDIV_MU_NB_DELTA=%%d -o %%s %%s' %% (F, d, b, SRC)
    r = subprocess.run(cmd, shell=True, capture_output=True)
    return b if r.returncode == 0 else None

def measure(b, c):
    outf = '/tmp/nbs.out'
    cmd = ('valgrind --tool=callgrind ' + CACHE + ' --callgrind-out-file=' + outf +
           ' ' + b + ' < ' + IN + c + '.in > /dev/null 2>/dev/null')
    if subprocess.run(cmd, shell=True).returncode != 0:
        return None
    ev = tot = None
    with open(outf) as f:
        for line in f:
            if line.startswith('events:'):
                ev = line.split()[1:]
            elif line.startswith('summary:') or line.startswith('totals:'):
                tot = [int(x) for x in line.split()[1:]]
    if not ev or not tot:
        return None
    d = dict(zip(ev, tot))
    Ir = d.get('Ir', 0)
    d1 = d.get('D1mr', 0) + d.get('D1mw', 0)
    dl = d.get('DLmr', 0) + d.get('DLmw', 0)
    return Ir, d1, dl, Ir + 5 * d1 + 200 * dl

bins = {}
for d in DELTAS:
    b = build(d)
    print('build delta=%%d -> %%s' %% (d, 'ok' if b else 'FAIL'), flush=True)
    if b:
        bins[d] = b

for c in CASES:
    if not os.path.exists(IN + c + '.in'):
        continue
    print('', flush=True)
    print('=== %%s' %% c, flush=True)
    print('%%6s %%13s %%10s %%13s %%8s' %% ('delta', 'Ir', 'DLm', 'est_cycles', 'rel'),
          flush=True)
    rows = []
    for d in DELTAS:
        if d not in bins:
            continue
        r = measure(bins[d], c)
        if not r:
            print('%%6d FAIL' %% d, flush=True); continue
        rows.append((d,) + r)
    if not rows:
        continue
    base = min(r[4] for r in rows)
    for d, Ir, d1, dl, est in rows:
        print('%%6d %%13d %%10d %%13d %%8.4f' %% (d, Ir, dl, est, est / base), flush=True)
    bd = min(rows, key=lambda r: r[4])[0]
    print('  --> best delta = %%d (nb = nb_min+%%d)' %% (bd, bd), flush=True)
print('', flush=True)
print('DONE', flush=True)
''' % (DELTAS, CASES)

p = os.path.join(HERE, "_nb_sweep.py")
with open(p, "w", encoding="utf-8", newline="\n") as f:
    f.write(REMOTE)
put(p, "/home/azzr/nb_sweep.py")
run("mkdir -p /home/azzr/divbench/logs && rm -f " + LOG)
run(f"cd /home/azzr && nohup python3 nb_sweep.py > {LOG} 2>&1 &  echo started", timeout=30)
print("started nb_sweep; poll with: python nb_sweep.py poll")
