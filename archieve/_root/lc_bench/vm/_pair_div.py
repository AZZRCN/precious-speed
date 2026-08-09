# -*- coding: utf-8 -*-
"""本地驱动: 上传 _pair_div_remote.py 并在 VM 上跑多候选配对计时.

用法: python _pair_div.py d34 d37 d38 [--reps 11]
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run

HERE = os.path.dirname(os.path.abspath(__file__))
put(os.path.join(HERE, '_pair_div_remote.py'), '/home/azzr/divbench/_pair_div_remote.py')

argv = ' '.join(sys.argv[1:]) or 'd34 d37 d38'
tag = '_'.join(a for a in sys.argv[1:] if not a.startswith('--')) or 'default'
OUT = os.path.join(HERE, '_pair_%s.txt' % tag)

rc, o, e = run('cd /home/azzr/divbench && python3 _pair_div_remote.py %s 2>&1' % argv,
               timeout=7200, verbose=False)
print(o)
if e.strip():
    print('STDERR:', e[-1500:])
open(OUT, 'w', encoding='utf-8').write(o)
print('--> saved', OUT)
