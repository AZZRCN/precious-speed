# -*- coding: utf-8 -*-
"""对指定 bin 列表 x case 列表 跑 callgrind Ir 对比 (不重编译).

用法: python _ir3.py "d34,d37,d38" "length_ratio_integer_00,length_ratio_integer_01" [topn]
"""
import sys, os, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import run

BINS = (sys.argv[1] if len(sys.argv) > 1 else 'd34,d37,d38').split(',')
CASES = (sys.argv[2] if len(sys.argv) > 2 else
         'length_ratio_integer_00,length_ratio_integer_01').split(',')
TOPN = sys.argv[3] if len(sys.argv) > 3 else '14'
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '_ir3_out.txt')

lines = []
def log(s):
    lines.append(str(s)); print(s)
    try:
        open(OUT, 'w', encoding='utf-8').write('\n'.join(lines))
    except Exception:
        pass

res = {}
tops = {}
for case in CASES:
    for b in BINS:
        rc, o, e = run('cd /home/azzr/divbench && python3 _top_remote.py %s %s %s'
                       % (b, case, TOPN), timeout=5400, verbose=False)
        m = re.search(r'TOTALS Ir=(\d+)', o)
        res[(case, b)] = int(m.group(1)) if m else None
        tops[(case, b)] = o
        log('[ir] %-30s %-6s = %s' % (case, b, res[(case, b)]))

log('')
log('%-32s %s' % ('case', '  '.join('%14s' % b for b in BINS)))
for case in CASES:
    row = '%-32s' % case
    base = res[(case, BINS[0])]
    for b in BINS:
        v = res[(case, b)]
        if v is None:
            row += ' %14s' % 'NA'
        elif b == BINS[0]:
            row += ' %14d' % v
        else:
            row += ' %9d(%+.2f%%)' % (v, (v / base - 1) * 100)
    log(row)

log('')
for case in CASES:
    log('=== TOP %s @ %s (%s) ===' % (TOPN, case, BINS[0]))
    log(tops[(case, BINS[0])][:3500])
log('DONE')
