# -*- coding: utf-8 -*-
"""D37 验证: push -> 编译 -> 26例对拍 -> callgrind Ir 对比 (d34 vs d37) 三个 headline case.
D37 = D36(解耦阈值) + basicMul 内层 addmul_1 SWAR/AVX2 向量化 (absAddMul1).
结果写本地 _d37_out.txt.
"""
import sys, os, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run

LOCAL = r'D:\precious_speed\best\div_D37.cpp'
REMOTE = '/home/azzr/divbench/div_D37.cpp'
OUT = r'D:\precious_speed\lc_bench\vm\_d37_out.txt'
CASES = ['burnikel_ziegler_bound_02', 'r_nearly_zero_01', 'length_ratio_integer_02']

lines = []
def log(s):
    lines.append(str(s))
    print(s)
    try:
        open(OUT, 'w', encoding='utf-8').write('\n'.join(lines))
    except Exception:
        pass

def runc(cmd, timeout=3600):
    rc, o, e = run(cmd, timeout=timeout, verbose=False)
    return rc, o, e

def cg(tag, case, timeout=3600):
    rc, o, e = runc('cd /home/azzr/divbench && python3 _top_remote.py %s %s 14' % (tag, case), timeout=timeout)
    m = re.search(r'TOTALS Ir=(\d+)', o)
    ir = m.group(1) if m else 'NA'
    picks = {}
    for key in ('basicMul', 'absAddMul1', 'absMul1', 'absSub'):
        mm = re.search(r'^\s*(\d+)\s+([\d.]+)%.*?::' + key + r'\(', o, re.M)
        if mm:
            picks[key] = (int(mm.group(1)), mm.group(2))
    return ir, picks, o

# 1) push
put(LOCAL, REMOTE)
log('[1] PUSH ok -> ' + REMOTE)

# 2) compile
rc, o, e = runc('cd /home/azzr/divbench && '
                'g++ -O2 -std=c++23 -march=x86-64-v3 -o bin/d37 div_D37.cpp 2>&1 | tail -30 '
                '&& echo BUILD_DONE', timeout=1200)
log('[2] COMPILE rc=%d' % rc)
log(o[-2500:])
if 'BUILD_DONE' not in o:
    log('BUILD FAILED:\n' + e[-1500:])
    sys.exit(1)

# 3) bytecheck 26 例
rc, o, e = runc('cd /home/azzr/divbench && python3 _bytecheck.py d37 2>&1 | tail -20', timeout=2400)
log('[3] BYTECHECK rc=%d' % rc)
log(o[-2000:])
if 'ALL MATCH' not in o and 'bad=0' not in o:
    log('BYTECHECK FAILED, stop. ERR=%s' % e[-1000:])
    sys.exit(2)

# 4) callgrind 三 case, d37 vs d34
res = {}
for case in CASES:
    ir37, p37, _ = cg('d37', case)
    ir34, p34, _ = cg('d34', case)
    res[case] = (ir37, ir34, p37, p34)
    log('[4] %-30s D37 Ir=%s   D34 Ir=%s' % (case, ir37, ir34))
    for k in ('basicMul', 'absAddMul1', 'absMul1', 'absSub'):
        a = p37.get(k); b = p34.get(k)
        if a or b:
            log('      %-12s D37=%-12s(%s%%)  D34=%-12s(%s%%)' % (
                k,
                a[0] if a else '-', a[1] if a else '-',
                b[0] if b else '-', b[1] if b else '-'))

# 5) summary
log('')
log('--- D37 vs D34 (Ir) ---')
mx37 = 0; mx34 = 0
for case in CASES:
    ir37, ir34 = res[case][0], res[case][1]
    try:
        a = int(ir37); b = int(ir34)
        mx37 = max(mx37, a); mx34 = max(mx34, b)
        log('%-30s %12d -> %12d   %+.2f%%' % (case, b, a, (a / b - 1) * 100))
    except Exception:
        log('%-30s NA' % case)
if mx34:
    log('HEADLINE(max)  %12d -> %12d   %+.2f%%' % (mx34, mx37, (mx37 / mx34 - 1) * 100))
log('DONE')
