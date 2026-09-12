#!/usr/bin/env python3
# 驱动: 动态 ISA 宽度剖析
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
REMOTE = '/home/azzr/divbench'

SRC = sys.argv[1] if len(sys.argv) > 1 else 'div_D16'
CASE = sys.argv[2] if len(sys.argv) > 2 else 'length_ratio_integer_03'

vmctl.put(os.path.join(ROOT, 'best', SRC + '.cpp'), REMOTE + '/src/' + SRC + '.cpp')
vmctl.put(os.path.join(HERE, '_isaprof_remote.py'), REMOTE + '/_isaprof_remote.py')

r = vmctl.run('cd %s && python3 _isaprof_remote.py %s %s' % (REMOTE, SRC, CASE), timeout=7200)
out = r[1] if isinstance(r, (tuple, list)) else str(r)
print(out)
open(os.path.join(HERE, '_isaprof_%s_%s.log' % (SRC, CASE)), 'w', encoding='utf-8').write(out)
