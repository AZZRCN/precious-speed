#!/usr/bin/env python3
"""Diagnose D50 rnz mismatch: compare div_D50 vs div_orig on r_nearly_zero seeds.
Prints q/r diff to reveal the error pattern (off-by-one => cyclic truncation bug;
arbitrary => path incompatibility)."""
import subprocess, sys
GEN = '/tmp/gen_r_nearly_zero'
B50 = '/home/azzr/divbench/bin/div_D50'
BORIG = '/home/azzr/divbench/bin/div_orig'
for seed in (61, 64, 70, 1):
    with open('/tmp/r.txt', 'wb') as f:
        subprocess.run([GEN, str(seed)], stdout=f)
    o50 = subprocess.run([B50], stdin=open('/tmp/r.txt', 'rb'), capture_output=True).stdout
    oor = subprocess.run([BORIG], stdin=open('/tmp/r.txt', 'rb'), capture_output=True).stdout
    try:
        qa, ra = map(int, o50.split())
        qb, rb = map(int, oor.split())
    except Exception as e:
        print(f'seed={seed} PARSE FAIL d50={o50[:60]!r} orig={oor[:60]!r}'); continue
    print(f'seed={seed:3d} D50 q={qa} r={ra}')
    print(f'         ORIG q={qb} r={rb}   dq={qa-qb} dr={ra-rb}')
