#!/usr/bin/env python3
# 驱动: FFT 长度直方图
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
REMOTE = '/home/azzr/divbench'

SRCS = sys.argv[1] if len(sys.argv) > 1 else 'div_D16'
CASES = sys.argv[2] if len(sys.argv) > 2 else 'length_ratio_integer_03'

for s in SRCS.split(','):
    vmctl.put(os.path.join(ROOT, 'best', s + '.cpp'), REMOTE + '/src/' + s + '.cpp')
vmctl.put(os.path.join(HERE, '_ffthist_remote.py'), REMOTE + '/_ffthist_remote.py')

r = vmctl.run('cd %s && python3 _ffthist_remote.py %s %s' % (REMOTE, SRCS, CASES), timeout=3600)
out = r[1] if isinstance(r, (tuple, list)) else str(r)
print(out)
open(os.path.join(HERE, '_ffthist.log'), 'w', encoding='utf-8').write(out)
