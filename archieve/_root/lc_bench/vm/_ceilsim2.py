# -*- coding: utf-8 -*-
"""按 tag(lin/mn/cycm) 拆分的档位模拟: 判断"只对 lin 开放 radix-7"够不够。
用法: python _ceilsim2.py [cases...]
"""
import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import run

CASES = sys.argv[1:] or ['length_ratio_integer_00', 'length_ratio_integer_01',
                         'length_ratio_integer_02', 'r_nearly_zero_01',
                         'burnikel_ziegler_bound_02', 'a_max_b_random_02']
IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in'
TAG = ['lin', 'mn', 'cycm']


def ceil_with(n, odds):
    if n < 2:
        return 2
    best = None
    for o in odds:
        v = o
        while v < n:
            v <<= 1
        if best is None or v < best:
            best = v
    return best


def w(n):
    return n * math.log2(n) if n >= 2 else 0.0


results = {}
for c in CASES:
    csv = '/tmp/ch_%s.csv' % c
    cmd = ('cd /home/azzr/divbench && CEILHIST_CSV=%s ./bin/d39ch < %s/%s.in > /dev/null 2>/dev/null; '
           'cat %s' % (csv, IN, c, csv))
    rc, o, e = run(cmd, timeout=900, verbose=False)
    rows = []
    for line in o.splitlines():
        p = line.strip().split(',')
        if len(p) == 4 and p[0].isdigit():
            rows.append((int(p[0]), int(p[1]), int(p[2]), int(p[3])))  # tag,need,used,cnt
    if not rows:
        print('[skip] %s %s' % (c, e[-200:]))
        continue
    results[c] = rows
    print('[ok] %s rows=%d' % (c, len(rows)))

print('\n=== per-tag share of current {1,3} total N*logN ===')
print('%-28s %10s %10s %10s' % ('case', 'lin', 'mn', 'cycm'))
for c, rows in results.items():
    tot = sum(w(ceil_with(n, [1, 3])) * cnt for t, n, u, cnt in rows if n >= 2)
    line = '%-28s' % c[:28]
    for tg in range(3):
        s = sum(w(ceil_with(n, [1, 3])) * cnt for t, n, u, cnt in rows if n >= 2 and t == tg)
        line += '%9.1f%%' % (100.0 * s / tot)
    print(line)

print('\n=== saving vs current {1,3}: LIN-only radix7 vs ALL-tag radix7 ===')
print('%-28s %12s %12s %12s %12s' % ('case', 'lin7', 'all7', 'lin7+21', 'all7+21'))
for c, rows in results.items():
    cur = sum(w(ceil_with(n, [1, 3])) * cnt for t, n, u, cnt in rows if n >= 2)
    out = []
    for odds in ([1, 3, 7], [1, 3, 7, 21]):
        # lin-only: tag0 用新档位集, tag1/2 仍用 {1,3}
        s = sum(w(ceil_with(n, odds if t == 0 else [1, 3])) * cnt
                for t, n, u, cnt in rows if n >= 2)
        out.append(100.0 * (s / cur - 1.0))
        s2 = sum(w(ceil_with(n, odds)) * cnt for t, n, u, cnt in rows if n >= 2)
        out.append(100.0 * (s2 / cur - 1.0))
    print('%-28s %11.2f%% %11.2f%% %11.2f%% %11.2f%%'
          % (c[:28], out[0], out[1], out[2], out[3]))

print('\n=== top lin-tag gaps (need -> cur used -> with7) ===')
for c, rows in results.items():
    lst = []
    for t, n, u, cnt in rows:
        if n < 2 or t != 0:
            continue
        cu = ceil_with(n, [1, 3])
        n7 = ceil_with(n, [1, 3, 7])
        d = (w(cu) - w(n7)) * cnt
        if d > 0:
            lst.append((d, n, cu, n7, cnt))
    lst.sort(reverse=True)
    if not lst:
        continue
    print('-- %s' % c)
    for d, n, cu, n7, cnt in lst[:6]:
        print('   need=%-9d cur=%-9d ->7 %-9d x%-5d dNlogN=%.4g' % (n, cu, n7, cnt, d))
print('DONE')
