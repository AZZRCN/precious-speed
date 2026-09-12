# -*- coding: utf-8 -*-
"""以 D40 all-7 为基线, 评估"继续加档位"的收益上界。

关键前提: D40 已把**决策口径锁死在 {1,3}**(fft_ceil_dec), 所以块划分 inc 不再随
执行档位集变化 => CEILHIST 采到的 need 分布是**稳定**的, 可以直接拿来外推
"如果再加 5/9/21 档会省多少", 不会重蹈 lri_02 那种"模型换了 inc 反而更贵"的坑。

模拟两档严格度:
  ideal  : 任意 odd*2^k 都可用, 无最小长度限制  -> 收益上界
  gated  : 每个 odd 有最小长度门槛 (仿 FFT3_MIN=192 / FFT7_MIN=448)
           小于门槛时该 odd 不可用 -> 更接近真实可实现收益

用法: python _ceilsim3.py [bin] [cases...]
"""
import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import run

BIN = 'd40ach'
args = sys.argv[1:]
if args and not args[0].startswith('length') and not args[0].startswith('a_') \
        and not args[0].startswith('r_') and not args[0].startswith('burn') \
        and not args[0].startswith('max') and not args[0].startswith('med'):
    BIN = args.pop(0)
CASES = args or ['length_ratio_integer_00', 'length_ratio_integer_01',
                 'length_ratio_integer_02', 'r_nearly_zero_01',
                 'burnikel_ziegler_bound_02', 'a_max_b_random_02']
IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in'

# odd -> 最小可用长度 (门槛). 1 恒可用。3/7 用源码常量; 5/9/21 按同尺度外推:
#   门槛 ~= odd * 64 (radix-r 蝶形要摊薄常数因子, 每个子块至少 64 点)
GATE = {1: 0, 3: 192, 5: 320, 7: 448, 9: 576, 11: 704, 21: 1344}


def ceil_with(n, odds, gated):
    if n < 2:
        return 2
    best = None
    for o in odds:
        v = o
        while v < n:
            v <<= 1
        if gated and o != 1 and v < GATE.get(o, 64 * o):
            continue
        if best is None or v < best:
            best = v
    return best


def w(n):
    return n * math.log2(n) if n >= 2 else 0.0


SETS = [
    ('base {1,3,7}', [1, 3, 7]),
    ('+5   {1,3,5,7}', [1, 3, 5, 7]),
    ('+9   {1,3,7,9}', [1, 3, 7, 9]),
    ('+21  {1,3,7,21}', [1, 3, 7, 21]),
    ('+5+21', [1, 3, 5, 7, 21]),
    ('+5+9+21', [1, 3, 5, 7, 9, 21]),
    ('+5+9+11+21', [1, 3, 5, 7, 9, 11, 21]),
]

results = {}
for c in CASES:
    csv = '/tmp/ch3_%s.csv' % c
    cmd = ('cd /home/azzr/divbench && CEILHIST_CSV=%s ./bin/%s < %s/%s.in > /dev/null 2>/dev/null; '
           'cat %s; rm -f %s' % (csv, BIN, IN, c, csv, csv))
    rc, o, e = run(cmd, timeout=900, verbose=False)
    rows = []
    for line in o.splitlines():
        p = line.strip().split(',')
        if len(p) == 4 and p[0].isdigit():
            rows.append((int(p[0]), int(p[1]), int(p[2]), int(p[3])))
    if not rows:
        print('[skip] %s %s' % (c, e[-200:]))
        continue
    results[c] = rows
    print('[ok] %s rows=%d' % (c, len(rows)))

for gated in (True, False):
    print('\n=== saving vs {1,3,7}  (%s) ===' % ('GATED, 可实现' if gated else 'IDEAL, 上界'))
    hdr = '%-26s' % 'case' + ''.join('%14s' % s[0] for s in SETS[1:])
    print(hdr)
    print('-' * len(hdr))
    for c, rows in results.items():
        cur = sum(w(ceil_with(n, SETS[0][1], gated)) * cnt for t, n, u, cnt in rows if n >= 2)
        line = '%-26s' % c[:26]
        for name, odds in SETS[1:]:
            s = sum(w(ceil_with(n, odds, gated)) * cnt for t, n, u, cnt in rows if n >= 2)
            line += '%13.2f%%' % (100.0 * (s / cur - 1.0))
        print(line)

print('\n=== 每个 case 的 top 缺口 (base {1,3,7} 下, GATED) ===')
for c, rows in results.items():
    lst = []
    for t, n, u, cnt in rows:
        if n < 2:
            continue
        cu = ceil_with(n, [1, 3, 7], True)
        nx = ceil_with(n, [1, 3, 5, 7, 9, 11, 21], True)
        d = (w(cu) - w(nx)) * cnt
        if d > 0:
            lst.append((d, ['lin', 'mn', 'cycm'][t], n, cu, nx, cnt))
    lst.sort(reverse=True)
    if not lst:
        continue
    tot = sum(w(ceil_with(n, [1, 3, 7], True)) * cnt for t, n, u, cnt in rows if n >= 2)
    print('-- %s  (total=%.4g)' % (c, tot))
    for d, tg, n, cu, nx, cnt in lst[:6]:
        print('   %-4s need=%-9d cur=%-9d -> %-9d x%-5d dNlogN=%.4g (%.2f%% of total)'
              % (tg, n, cu, nx, cnt, d, 100.0 * d / tot))
print('DONE')
