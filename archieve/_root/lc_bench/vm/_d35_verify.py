# -*- coding: utf-8 -*-
"""D35 验证: push -> 编译 -> 26例对拍 -> callgrind Ir 对比 (D35 vs D31).
自动探测 burnikel 用例名 (避免硬编码错误). 结果写 _d35_out.txt.
"""
import sys, os, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run

LOCAL = r'D:\precious_speed\best\div_D35.cpp'
REMOTE = '/home/azzr/divbench/div_D35.cpp'
OUT = r'D:\precious_speed\lc_bench\vm\_d35_out.txt'
INDIR = '/home/azzr/lcp/big_integer/division_of_big_integers/in'

lines = []
def log(s):
    lines.append(str(s)); print(s)

def runc(cmd, timeout=3600):
    rc, o, e = run(cmd, timeout=timeout, verbose=False)
    return rc, o, e

def cg(tag, case, timeout=3600):
    rc, o, e = runc('cd /home/azzr/divbench && python3 _top_remote.py %s %s 16' % (tag, case), timeout=timeout)
    m = re.search(r'TOTALS Ir=(\d+)', o)
    ir = m.group(1) if m else 'NA'
    ms = re.search(r'(\d+)\s+[\d.]+%\s+\d+\s+\d+\s+(__memset|absCompare)', o)
    msline = (ms.group(1) + ' Ir ' + ms.group(2)) if ms else 'no-memset/absCompare'
    return ir, msline, e[-500:]

put(LOCAL, REMOTE)
log('[1] PUSH ok')

rc, o, e = runc('cd /home/azzr/divbench && '
                'g++ -O2 -std=c++23 -march=x86-64-v3 -o bin/d35 div_D35.cpp 2>&1 | tail -25 '
                '&& echo BUILD_DONE', timeout=900)
log('[2] COMPILE rc=%d' % rc); log(o[-1800:])
if 'BUILD_DONE' not in o:
    log('BUILD FAILED:\n' + e[-1200:]); open(OUT,'w').write('\n'.join(lines)); sys.exit(1)

rc, o, e = runc('cd /home/azzr/divbench && python3 _bytecheck.py d35 2>&1 | tail -20', timeout=1800)
log('[3] BYTECHECK rc=%d' % rc); log(o[-1800:])
if 'ALL MATCH' not in o and 'bad=0' not in o and 'PASS' not in o:
    log('BYTECHECK suspicious, stopping. ERR=%s' % e[-800:]); open(OUT,'w').write('\n'.join(lines)); sys.exit(2)

# 探测 burnikel 用例名
rc, o, e = runc('ls %s | grep -i burnikel' % INDIR, timeout=120)
bk = [x for x in o.split() if x.endswith('.in')]
log('[4] burnikel cases: %s' % bk)
bk_case = bk[0].rsplit('.in', 1)[0] if bk else None

pairs = [('length_ratio_integer_02', 'lri_02'), ('r_nearly_zero_01', 'rnz_01')]
if bk_case:
    pairs.append((bk_case, 'bz'))

for case, name in pairs:
    ir35, ms35, err35 = cg('d35', case)
    log('[5a] D35 %s Ir=%s  %s' % (name, ir35, ms35))
    if err35.strip(): log('     d35 %s cg-err: %s' % (name, err35))
    ir31, ms31, _ = cg('d31', case)
    log('[5b] D31 %s Ir=%s  %s' % (name, ir31, ms31))
    try:
        a=int(ir35); b=int(ir31)
        log('[5c] %s D35/D31 = %.4f (%.2f%%)' % (name, a/b, (1-a/b)*100))
    except Exception as ex:
        log('[5c] %s ratio n/a: %s' % (name, ex))

open(OUT,'w').write('\n'.join(lines))
log('[done] wrote '+OUT)
