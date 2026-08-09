# -*- coding: utf-8 -*-
"""定位 lri_00 里 memset / memcpy 的调用来源 (callgrind caller 分析)."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run

HERE = os.path.dirname(os.path.abspath(__file__))
BIN = sys.argv[1] if len(sys.argv) > 1 else 'd38'
CASE = sys.argv[2] if len(sys.argv) > 2 else 'length_ratio_integer_00'
OUT = os.path.join(HERE, '_memsrc_%s.txt' % BIN)

put(os.path.join(HERE, '_callers.py'), '/home/azzr/divbench/_callers.py')

buf = []
rc, o, e = run('ls -la /tmp/cgT_%s_%s.out 2>&1' % (BIN, CASE), timeout=60, verbose=False)
buf.append(o)
print(o)

for sub in ('memset', 'memcpy', 'fftMulModBm1Pre', 'fftMulPre'):
    rc, o, e = run('cd /home/azzr/divbench && python3 _callers.py /tmp/cgT_%s_%s.out %s 2>&1 | head -40'
                   % (BIN, CASE, sub), timeout=900, verbose=False)
    buf.append('\n===== callers of %s =====\n%s' % (sub, o))
    print(buf[-1])

open(OUT, 'w', encoding='utf-8').write('\n'.join(buf))
print('--> saved', OUT)
