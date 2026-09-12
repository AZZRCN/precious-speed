#!/usr/bin/env python3
"""Independent re-check of D50 correctness vs div_orig, bypassing d49_verify_vm.py.
Usage: python3 d50_check.py <gen_name> <n_seeds>"""
import subprocess, sys
GEN = '/tmp/gen_' + sys.argv[1]
N = int(sys.argv[2])
B50 = '/home/azzr/divbench/bin/div_D50'
BORIG = '/home/azzr/divbench/bin/div_orig'
bad = []
for seed in range(1, N + 1):
    subprocess.run([GEN, str(seed)], stdout=open('/tmp/c.txt', 'wb'))
    o50 = subprocess.run([B50], stdin=open('/tmp/c.txt', 'rb'), capture_output=True).stdout
    oor = subprocess.run([BORIG], stdin=open('/tmp/c.txt', 'rb'), capture_output=True).stdout
    if o50 != oor:
        bad.append(seed)
print(f'{sys.argv[1]}: bad={bad} count={len(bad)}/{N}')
