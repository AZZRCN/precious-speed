#!/usr/bin/env python3
"""Profile div_D47 on big-division cases via callgrind (Ir attribution).
Answers: which FUNCTION dominates the Newton-inverse / FFT path for the
headline a_max_b_random_02 case. Run on the VM (Linux == LC env)."""
import sys, subprocess, os

SRC = '/home/azzr/divbench/src/div_D47.cpp'
BIN = '/home/azzr/divbench/bin/div_D47_g'
CG  = '/home/azzr/divbench/cg.out'

CASES = sys.argv[1:] or [
    '/home/azzr/divbench/cases/a_max_b_random_02.in',
    '/home/azzr/divbench/cases/c_mudiv.txt',
    '/home/azzr/divbench/cases/c_newton.txt',
]

def build():
    print('[build] g++ -g ...', flush=True)
    r = subprocess.run(['g++', '-std=c++23', '-O2', '-march=x86-64-v3', '-g',
                        '-o', BIN, SRC], capture_output=True, text=True, timeout=300)
    if r.returncode != 0:
        print('BUILD FAIL\n', r.stderr[-2000:]); sys.exit(1)
    print('[build] ok', flush=True)

def profile(case):
    print(f'\n===== CASE {os.path.basename(case)} =====', flush=True)
    if not os.path.exists(case):
        print('  MISSING case file, skip', flush=True); return
    with open(case, 'rb') as f:
        r = subprocess.run(['valgrind', '--tool=callgrind',
                             '--callgrind-out-file=' + CG, BIN],
                            stdin=f, capture_output=True, text=True, timeout=900)
    print(f'  valgrind rc={r.returncode}', flush=True)
    # function-level Ir, sorted by self cost
    ann = subprocess.run(['callgrind_annotate', '--auto=yes', CG],
                         capture_output=True, text=True, timeout=300)
    # keep only the function summary table (first ~60 lines)
    lines = ann.stdout.splitlines()
    out = []
    for ln in lines:
        out.append(ln)
        if 'Ir' in ln and 'file:function' in ln:
            # header found; capture a bit more then stop after table
            pass
    txt = '\n'.join(out[:55])
    print(txt, flush=True)

if __name__ == '__main__':
    build()
    for c in CASES:
        profile(c)
