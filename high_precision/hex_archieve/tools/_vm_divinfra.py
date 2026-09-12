#!/usr/bin/env python3
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl

cmds = [
    'ls /home/azzr/divbench/src/ 2>/dev/null | head',
    'ls /home/azzr/divbench/bin/ 2>/dev/null',
    'test -f /home/azzr/divbench/bin/div_orig && echo ORIG_OK || echo NO_ORIG',
    'ls /home/azzr/lcgen/division_of_big_integers/gen/ 2>/dev/null | head',
    'test -f /home/azzr/verify_official.py && echo VO_OK || echo NO_VO',
    'which g++ && g++ --version | head -1',
    'nproc',
]
for cmd in cmds:
    rc, o, _ = vmctl.run(cmd)
    print('$', cmd)
    print(o)
