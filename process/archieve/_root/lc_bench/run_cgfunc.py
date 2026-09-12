# -*- coding: utf-8 -*-
"""VM 上跑函数级 callgrind 剖分 (最高点 length_ratio_integer_02)"""
import sys, os
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, 'vm'))
import vmctl

CASE = sys.argv[1] if len(sys.argv) > 1 else 'length_ratio_integer_02'

vmctl.put(os.path.join(HERE, 'vm', '_cgfunc.sh'), '/home/azzr/divbench/_cgfunc.sh')
rc, out, err = vmctl.run(
    "cd /home/azzr/divbench && tr -d '\\r' < _cgfunc.sh > _t.sh && mv _t.sh _cgfunc.sh "
    "&& chmod +x _cgfunc.sh && ./_cgfunc.sh " + CASE, timeout=3000)
print(out)
if err:
    print('--- STDERR ---')
    print(err[-2000:])
print('rc=', rc)
