#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""callgrind 函数级分解 —— 找出 lri_03 相对 lri_02 的异常热点。

背景: D12 锚定发现 k=est/ms 对 lri_00/01/02/a_max_02 一致 (10.13~10.47),
      唯独 lri_03 = 9.31 (低 11%)。即 lri_03 实际 cycles 比 est 模型多 11%,
      存在 IPC < 1 的隐藏开销 (长依赖链 / 分支误预测 / port 压力)。

做法: 复用 cg_zen3 留在 /tmp 的 callgrind out 文件, 用 callgrind_annotate
      按函数拆 Ir + D1mr + DLmr, 归一化成占比后对比两个用例。
用法: python cg_func.py [case ...]
      python cg_func.py poll
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
LOG = "/home/azzr/divbench/logs/cg_func.log"

if len(sys.argv) > 1 and sys.argv[1] == "poll":
    rc, out, err = run(f"cat {LOG}")
    print(out)
    sys.exit(0)

CASES = sys.argv[1:] or ["length_ratio_integer_03", "length_ratio_integer_02",
                         "length_ratio_integer_00"]

REMOTE = '''#!/usr/bin/env python3
import subprocess, re, os
CASES = %r

def parse(case):
    f = '/tmp/cgz_%%s.out' %% case
    if not os.path.exists(f):
        print('MISSING', f, flush=True); return None
    r = subprocess.run(['callgrind_annotate', '--threshold=99.5', f],
                       capture_output=True, text=True)
    out = r.stdout
    # 找到 events 行顺序
    ev = None
    for line in out.splitlines():
        if line.strip().startswith('Events shown:'):
            ev = line.split(':', 1)[1].split()
            break
    rows = []
    started = False
    for line in out.splitlines():
        if re.match(r'^-+$', line.strip()):
            continue
        if line.strip().startswith('Ir ') or (ev and line.strip().startswith(ev[0])):
            started = True; continue
        if not started:
            continue
        m = re.match(r'^\\s*([\\d,]+(?:\\s+[\\d,]+)*)\\s+(\\S.*)$', line)
        if not m:
            continue
        nums = [int(x.replace(',', '')) for x in m.group(1).split()]
        name = m.group(2).strip()
        if name.startswith('<') or 'command' in name.lower():
            continue
        rows.append((nums, name))
    return ev, rows

res = {}
for c in CASES:
    p = parse(c)
    if not p: continue
    ev, rows = p
    tot = sum(r[0][0] for r in rows) or 1
    agg = {}
    for nums, name in rows:
        # name 形如 "12,345  path:func"  取 func 部分
        fn = name.split(':')[-1].strip()
        fn = re.sub(r'\\(.*', '', fn)[:58]
        a = agg.setdefault(fn, [0]*len(nums))
        for i, v in enumerate(nums):
            a[i] += v
    res[c] = (ev, tot, agg)
    print('=== %%s   events=%%s  totalIr=%%d' %% (c, ev, tot), flush=True)
    for fn, a in sorted(agg.items(), key=lambda kv: -kv[1][0])[:18]:
        print('  %%7.3f%%%%  Ir=%%13d  %%s' %% (100.0*a[0]/tot, a[0], fn), flush=True)
    print(flush=True)

if len(CASES) >= 2 and CASES[0] in res and CASES[1] in res:
    a_ev, a_tot, a_agg = res[CASES[0]]
    b_ev, b_tot, b_agg = res[CASES[1]]
    print('=== share diff: %%s  MINUS  %%s (百分点, 正=前者占比更高) ===' %% (CASES[0], CASES[1]), flush=True)
    keys = set(a_agg) | set(b_agg)
    diffs = []
    for k in keys:
        sa = 100.0*a_agg.get(k, [0])[0]/a_tot
        sb = 100.0*b_agg.get(k, [0])[0]/b_tot
        diffs.append((sa-sb, sa, sb, k))
    diffs.sort(key=lambda t: -abs(t[0]))
    for d, sa, sb, k in diffs[:20]:
        print('  %%+7.3f pt   %%6.3f%%%% vs %%6.3f%%%%   %%s' %% (d, sa, sb, k), flush=True)
print('DONE', flush=True)
''' % (CASES,)

p = os.path.join(HERE, "_cg_func.py")
with open(p, "w", encoding="utf-8", newline="\n") as f:
    f.write(REMOTE)
put(p, "/home/azzr/cg_func.py")
run("mkdir -p /home/azzr/divbench/logs && rm -f " + LOG)
rc, out, err = run(f"cd /home/azzr && python3 cg_func.py > {LOG} 2>&1; tail -80 {LOG}", timeout=300)
print(out)
print(err)
