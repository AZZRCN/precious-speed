#!/usr/bin/env python3
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl
ROOT = 'D:/hex_precious_speed'
DB = '/home/azzr/divbench'
vmctl.put(os.path.join(ROOT, 'work/div/v9.cpp'), f'{DB}/src/div_v9.cpp')
vmctl.put(os.path.join(ROOT, 'tools/d8_verify.py'), f'{DB}/d8_verify.py')
rc, o, _ = vmctl.run(f'cd {DB} && python3 d8_verify.py div_v9 2>&1 | tail -50')
print(o)
