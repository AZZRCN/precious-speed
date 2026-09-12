"""测量 DIV 在瓶颈用例上的 FFT 长度浪费 (need->used)，决定 mixed-radix 是否该加 5/7。
用法: python _ceilhist.py            (本地跑, 经 vmctl SSH 到 VM 编译+执行)
依赖: best/div_D45.cpp 已存在; vmctl 自动发现 VM IP。
"""
import sys, os, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import run, put

RD = '/home/azzr/divbench'
SRC = 'div_D45.cpp'
BIN = 'bin/d45ch'
IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
LOCAL_SRC = r'D:\precious_speed\best\div_D45.cpp'
# 重点: 真瓶颈 r_nearly_zero_01 + 旧 headline + 长度浪费点
CASES = ['r_nearly_zero_01', 'r_nearly_zero_02',
         'length_ratio_integer_00', 'length_ratio_integer_01',
         'length_ratio_integer_02', 'a_max_b_random_02',
         'burnikel_ziegler_bound_02']

def L(*x):
    s = ' '.join(str(i) for i in x)
    print(s, flush=True)

L('[start] 上传源码 + 编译 -DCEILHIST')
put(LOCAL_SRC, RD + '/' + SRC)
rc, o, e = run('cd %s && g++ -O2 -std=c++23 -march=x86-64-v3 -DCEILHIST %s -o %s 2>&1 | tail -4'
               % (RD, SRC, BIN), timeout=1200)
if rc != 0:
    L('[BUILD FAILED]', o.strip()[:300], e.strip()[:300]); sys.exit(1)
L('[build] ok ->', BIN)

for c in CASES:
    p = IN + c + '.in'
    rc, o, e = run('cd %s && test -f %s && echo HAVE || echo NONE' % (RD, p), timeout=30)
    if 'NONE' in o:
        L('[skip] %s .in 缺失' % c); continue
    rc, o, e = run('cd %s && ./%s < %s 2>case_%s.ch.log >/dev/null; echo EXIT=$?'
                   % (RD, BIN, p, c), timeout=600)
    rc2, hist, _ = run('cd %s && grep -A 22 "fft_ceil need->used" case_%s.ch.log 2>/dev/null || echo NO_HIST'
                       % (RD, c), timeout=60)
    L('==== CASE %s (exit=%s) ====' % (c, o.strip().replace('EXIT=', '')))
    L(hist.strip() if hist.strip() else '(no histogram)')

L('[done]')
