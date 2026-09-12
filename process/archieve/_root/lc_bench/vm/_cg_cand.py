# -*- coding: utf-8 -*-
"""通用候选验证: push -> 编译 -> 26例逐字节对拍 -> callgrind Ir 对比基线.

用法: python _cg_cand.py D38 [d37]
      argv[1] = 候选 TAG (对应 best/div_<TAG>.cpp)
      argv[2] = 对比基线的远端 bin 名 (默认 d37)
结果写 _<tag>_out.txt。
"""
import sys, os, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run

TAG = sys.argv[1] if len(sys.argv) > 1 else 'D38'
REF = sys.argv[2] if len(sys.argv) > 2 else 'd37'
tag = TAG.lower()

LOCAL = r'D:\precious_speed\best\div_%s.cpp' % TAG
REMOTE = '/home/azzr/divbench/div_%s.cpp' % TAG
OUT = r'D:\precious_speed\lc_bench\vm\_%s_out.txt' % tag
# CASES 必须含 LC 真 headline。D25 #390643 回执: lri_00=45ms 是唯一离群点,
# 其余点全在 31~35 → LC 计分(max)由 lri_00 钉死。曾误用 lri_02 做 headline, 导致
# D37/D38 的 -6% 是假收益 (真 headline lri_00 上只有 -1.05%)。
CASES = ['length_ratio_integer_00', 'r_nearly_zero_01', 'burnikel_ziegler_bound_02']
WATCH = ('basicMul', 'absAddMul1', 'absSubMul1', 'absMul1', 'absSub', 'absDivBasicCore')

lines = []
def log(s):
    lines.append(str(s))
    print(s)
    try:
        open(OUT, 'w', encoding='utf-8').write('\n'.join(lines))
    except Exception:
        pass

def runc(cmd, timeout=3600):
    return run(cmd, timeout=timeout, verbose=False)

def cg(binname, case, timeout=5400):
    rc, o, e = runc('cd /home/azzr/divbench && python3 _top_remote.py %s %s 14' % (binname, case),
                    timeout=timeout)
    m = re.search(r'TOTALS Ir=(\d+)', o)
    ir = m.group(1) if m else 'NA'
    picks = {}
    for key in WATCH:
        mm = re.search(r'^\s*(\d+)\s+([\d.]+)%.*?::' + key + r'\(', o, re.M)
        if mm:
            picks[key] = (int(mm.group(1)), mm.group(2))
    return ir, picks

log('=== candidate %s  vs  ref %s ===' % (TAG, REF))
put(LOCAL, REMOTE)
log('[1] PUSH ok -> ' + REMOTE)

rc, o, e = runc('cd /home/azzr/divbench && '
                'g++ -O2 -std=c++23 -march=x86-64-v3 -o bin/%s div_%s.cpp 2>&1 | tail -30 '
                '&& echo BUILD_DONE' % (tag, TAG), timeout=1800)
log('[2] COMPILE rc=%d' % rc)
log(o[-2500:])
if 'BUILD_DONE' not in o:
    log('BUILD FAILED:\n' + e[-1500:])
    sys.exit(1)

rc, o, e = runc('cd /home/azzr/divbench && python3 _bytecheck.py %s 2>&1 | tail -20' % tag,
                timeout=3600)
log('[3] BYTECHECK rc=%d' % rc)
log(o[-2000:])
if 'ALL MATCH' not in o and 'bad=0' not in o:
    log('BYTECHECK FAILED, stop. ERR=%s' % e[-1000:])
    sys.exit(2)

res = {}
for case in CASES:
    irc, pc = cg(tag, case)
    irr, pr = cg(REF, case)
    res[case] = (irc, irr)
    log('[4] %-30s %s=%s   %s=%s' % (case, TAG, irc, REF, irr))
    for k in WATCH:
        a, b = pc.get(k), pr.get(k)
        if a or b:
            log('      %-16s %s=%-11s(%5s%%)   %s=%-11s(%5s%%)' % (
                k, TAG, a[0] if a else '-', a[1] if a else '-',
                REF, b[0] if b else '-', b[1] if b else '-'))

log('')
log('--- %s vs %s (Ir) ---' % (TAG, REF))
mxc = mxr = 0
for case in CASES:
    ic, ir = res[case]
    try:
        a, b = int(ic), int(ir)
        mxc, mxr = max(mxc, a), max(mxr, b)
        log('%-30s %12d -> %12d   %+.2f%%' % (case, b, a, (a / b - 1) * 100))
    except Exception:
        log('%-30s NA' % case)
if mxr:
    log('HEADLINE(max)  %12d -> %12d   %+.2f%%' % (mxr, mxc, (mxc / mxr - 1) * 100))
log('DONE')
