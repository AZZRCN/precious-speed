# -*- coding: utf-8 -*-
"""多二进制 x 多用例的 CEILHIST 总量对比 (sum used N*logN / overhead)。
用法: python _ceilcmp.py bin1,bin2,... [case,case,...]
"""
import sys, os, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import run

BINS = (sys.argv[1] if len(sys.argv) > 1 else 'd39ch,d40ch,d40ach').split(',')
CASES = (sys.argv[2].split(',') if len(sys.argv) > 2 else [
    'length_ratio_integer_00', 'length_ratio_integer_01', 'length_ratio_integer_02',
    'r_nearly_zero_01', 'burnikel_ziegler_bound_02', 'a_max_b_random_02',
    'max_00', 'medium_01'])
IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in'

res = {}
for b in BINS:
    for c in CASES:
        rc, o, e = run('cd /home/azzr/divbench && ./bin/%s < %s/%s.in > /dev/null'
                       % (b, IN, c), timeout=900, verbose=False)
        m = re.search(r'TOTAL used=([\d.eE+]+) ideal=([\d.eE+]+) overhead=([\d.-]+)%', e)
        res[(b, c)] = (float(m.group(1)), float(m.group(2)), float(m.group(3))) if m else None

base = BINS[0]
print('\n%-28s %s' % ('case', ''.join('%22s' % b for b in BINS)))
print('-' * (28 + 22 * len(BINS)))
for c in CASES:
    line = '%-28s' % c[:28]
    b0 = res.get((base, c))
    for b in BINS:
        r = res.get((b, c))
        if r is None:
            line += '%22s' % 'NA'
            continue
        rel = 100.0 * (r[0] / b0[0] - 1.0) if b0 else 0.0
        line += '%12.4g(%+6.2f%%)' % (r[0], rel)
    print(line)

print('\n=== overhead vs own ideal ===')
print('%-28s %s' % ('case', ''.join('%12s' % b for b in BINS)))
for c in CASES:
    line = '%-28s' % c[:28]
    for b in BINS:
        r = res.get((b, c))
        line += '%11.2f%%' % (r[2] if r else float('nan'))
    print(line)
print('DONE')
