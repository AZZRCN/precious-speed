# -*- coding: utf-8 -*-
"""D33 验证: push -> 编译 -> 26例对拍 -> callgrind Ir 对比 (d31 vs d33).
结果写本地 _d33_out.txt (stdout 不可靠时靠文件回路).
"""
import sys, os, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run

LOCAL = r'D:\precious_speed\best\div_D33.cpp'
REMOTE = '/home/azzr/divbench/div_D33.cpp'
OUT = r'D:\precious_speed\lc_bench\vm\_d33_out.txt'

lines = []
def log(s):
    lines.append(str(s))
    print(s)

def runc(cmd, timeout=3600):
    rc, o, e = run(cmd, timeout=timeout, verbose=False)
    return rc, o, e

def cg(tag, case, timeout=3600):
    rc, o, e = runc('cd /home/azzr/divbench && python3 _top_remote.py %s %s 16' % (tag, case), timeout=timeout)
    m = re.search(r'TOTALS Ir=(\d+)', o)
    ir = m.group(1) if m else 'NA'
    ms = re.search(r'(\d+)\s+[\d.]+%\s+\d+\s+\d+\s+__memset', o)
    msline = (ms.group(1) + ' Ir memset') if ms else 'no-memset'
    return ir, msline, e[-600:]

# 1) push
put(LOCAL, REMOTE)
log('[1] PUSH ok -> ' + REMOTE)

# 2) compile
rc, o, e = runc('cd /home/azzr/divbench && '
                'g++ -O2 -std=c++23 -march=x86-64-v3 -o bin/d33 div_D33.cpp 2>&1 | tail -25 '
                '&& echo BUILD_DONE', timeout=900)
log('[2] COMPILE rc=%d' % rc)
log(o[-1800:])
if 'BUILD_DONE' not in o:
    log('BUILD FAILED:\n' + e[-1200:])
    open(OUT, 'w').write('\n'.join(lines))
    sys.exit(1)

# 3) bytecheck
rc, o, e = runc('cd /home/azzr/divbench && python3 _bytecheck.py d33 2>&1 | tail -20', timeout=1800)
log('[3] BYTECHECK rc=%d' % rc)
log(o[-1800:])
if 'ALL MATCH' not in o and 'bad=0' not in o and 'PASS' not in o:
    log('BYTECHECK suspicious, stopping. ERR=%s' % e[-800:])
    open(OUT, 'w').write('\n'.join(lines))
    sys.exit(2)

# 4) callgrind
ir33, ms33, err33 = cg('d33', 'length_ratio_integer_02')
log('[4a] D33 lri_02 Ir=%s  %s' % (ir33, ms33))
if err33.strip():
    log('     d33 lri_02 cg-err: ' + err33)
ir31, ms31, err31 = cg('d31', 'length_ratio_integer_02')
log('[4b] D31 lri_02 Ir=%s  %s' % (ir31, ms31))
if err31.strip():
    log('     d31 lri_02 cg-err: ' + err31)
ir33r, ms33r, _ = cg('d33', 'r_nearly_zero_01')
log('[4c] D33 rnz_01 Ir=%s  %s' % (ir33r, ms33r))

# 5) summary
try:
    a = int(ir33); b = int(ir31)
    log('[5] lri_02 D33/D31 Ir ratio = %.4f (%.2f%% reduction)' % (a/b, (1-a/b)*100))
except Exception as ex:
    log('[5] ratio n/a: ' + str(ex))

open(OUT, 'w').write('\n'.join(lines))
log('[done] wrote ' + OUT)
