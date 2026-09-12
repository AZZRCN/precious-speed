#!/usr/bin/env python3
# 驱动: 在 VM 上编译 -S 并统计热点函数的 SIMD 宽度 (ymm vs xmm)
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
REMOTE = '/home/azzr/divbench'

SRC = sys.argv[1] if len(sys.argv) > 1 else 'div_D16'

vmctl.put(os.path.join(ROOT, 'best', SRC + '.cpp'), REMOTE + '/src/' + SRC + '.cpp')
vmctl.put(os.path.join(HERE, '_simdwidth_remote.py'), REMOTE + '/_simdwidth_remote.py')

r = vmctl.run('cd %s && python3 _simdwidth_remote.py %s' % (REMOTE, SRC), timeout=1800)
out = r[1] if isinstance(r, (tuple, list)) else str(r)
print(out)
open(os.path.join(HERE, '_simdwidth_%s.log' % SRC), 'w', encoding='utf-8').write(out)
