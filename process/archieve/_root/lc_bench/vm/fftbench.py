#!/usr/bin/env python3
# 上传 fftbench.cpp + 源文件, 在 VM 编译并用 callgrind 差分测每个 FFT 原语的 Ir
# 用法: fftbench.py [srcs逗号分隔] [ops逗号分隔]
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
REMOTE = '/home/azzr/divbench'

SRCS = sys.argv[1] if len(sys.argv) > 1 else 'div_D16'
OPS = sys.argv[2] if len(sys.argv) > 2 else 'dif,idit,dot,conv'

vmctl.put(os.path.join(ROOT, 'fftbench.cpp'), REMOTE + '/fftbench.cpp')
for s in SRCS.split(','):
    vmctl.put(os.path.join(ROOT, 'best', s + '.cpp'), REMOTE + '/src/' + s + '.cpp')
vmctl.put(os.path.join(HERE, '_fftbench_remote.py'), REMOTE + '/_fftbench_remote.py')

r = vmctl.run('cd %s && python3 _fftbench_remote.py %s %s' % (REMOTE, SRCS, OPS),
              timeout=10800)
out = r[1] if isinstance(r, (tuple, list)) else str(r)
print(out)
open(os.path.join(HERE, '_fftbench.log'), 'w', encoding='utf-8').write(out)
