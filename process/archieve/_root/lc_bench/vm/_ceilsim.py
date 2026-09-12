# -*- coding: utf-8 -*-
"""导出各用例的 CEILHIST CSV, 离线模拟不同档位集合的 N*logN overhead。
用法: python _ceilsim.py [cases...]
"""
import sys, os, math, io
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import run

CASES = sys.argv[1:] or ['length_ratio_integer_00', 'length_ratio_integer_01',
                         'length_ratio_integer_02', 'r_nearly_zero_01',
                         'burnikel_ziegler_bound_02', 'a_max_b_random_02',
                         'max_00', 'medium_01']
IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in'

# 档位集合: 奇数因子集合 (档位 = odd * 2^k)
SETS = {
    'pow2        ': [1],
    'cur {1,3}   ': [1, 3],
    '+7  {1,3,7} ': [1, 3, 7],
    '+7,21       ': [1, 3, 7, 21],
    '+5  {1,3,5} ': [1, 3, 5],
    '+5,7        ': [1, 3, 5, 7],
    '+5,7,9      ': [1, 3, 5, 7, 9],
    '+5,7,9,11,13': [1, 3, 5, 7, 9, 11, 13],
}

def ceil_with(n, odds):
    """在 {odd*2^k} 档位集合中找 >= n 的最小值"""
    if n < 2:
        return 2
    best = None
    for o in odds:
        # 找最小 k 使 o*2^k >= n
        k = 0
        v = o
        while v < n:
            v <<= 1
            k += 1
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
            rows.append((int(p[1]), int(p[2]), int(p[3])))  # need, used, cnt
    if not rows:
        print('[skip] %s (no rows) %s' % (c, e[-200:]))
        continue
    results[c] = rows
    print('[ok] %s rows=%d' % (c, len(rows)))

print('\n%-28s %s' % ('case', ''.join('%14s' % k for k in SETS)))
print('-' * (28 + 14 * len(SETS)))
for c, rows in results.items():
    ideal = sum(w(n) * cnt for n, u, cnt in rows if n >= 2)
    line = '%-28s' % c[:28]
    for name, odds in SETS.items():
        used = sum(w(ceil_with(n, odds)) * cnt for n, u, cnt in rows if n >= 2)
        line += '%13.2f%%' % (100.0 * (used / ideal - 1.0))
    print(line)

# 相对现状的 N*logN 节省
print('\n=== relative to current {1,3} (NlogN saving) ===')
print('%-28s %s' % ('case', ''.join('%14s' % k for k in SETS)))
for c, rows in results.items():
    cur = sum(w(ceil_with(n, [1, 3])) * cnt for n, u, cnt in rows if n >= 2)
    line = '%-28s' % c[:28]
    for name, odds in SETS.items():
        used = sum(w(ceil_with(n, odds)) * cnt for n, u, cnt in rows if n >= 2)
        line += '%13.2f%%' % (100.0 * (used / cur - 1.0))
    print(line)
print('DONE')
