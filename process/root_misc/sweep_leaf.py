#!/usr/bin/env python3
# Sweep FFT_LEAF_LOG and INV_BASE on 393027_opt.cpp (cyclic wired).
# For each combo: build -> oracle byte-check (vs ref) -> perf 26 LC cases -> report #0 & MAX.
import subprocess, glob, os, sys

CASEDIR = '/tmp/lccases'
FLAGS = "-O3 -std=c++20 -march=znver3 -mtune=znver3 -w"
TESTS = [('small',1),('medium',3),('large',2),('max',3),('a_max_b_random',3),
         ('power',1),('r_nearly_zero',3),('length_ratio_integer',6),('burnikel_ziegler_bound',4)]

def sh(c):
    return subprocess.run(c, shell=True, capture_output=True, text=True)

def cases():
    out = []
    for name, num in TESTS:
        for seed in range(num):
            p = f'{CASEDIR}/{name}_{seed}.in'
            if os.path.isfile(p): out.append((f'{name}#{seed}', p))
    return out

def oracle_ok(binp):
    for tag, path in cases():
        o = subprocess.run([binp], stdin=open(path), capture_output=True)
        r = subprocess.run(['/tmp/ref'], stdin=open(path), capture_output=True)
        if o.stdout != r.stdout:
            return False, tag
    return True, ''

def instr(binp, path):
    p = subprocess.run(['perf','stat','-e','instructions:u','-x,',binp],
                       stdin=open(path), capture_output=True, text=True)
    for line in p.stderr.splitlines():
        if 'instructions:u' in line:
            try: return int(line.split(',')[0])
            except ValueError: pass
    return None

def main():
    grid = [(ll, ib) for ll in (6,7,8,9,10) for ib in (48,)]
    # build ref once
    r = sh(f"g++-15 {FLAGS} -o /tmp/ref /tmp/ref_src.cpp")
    if r.returncode:
        print("REF BUILD FAIL\n", r.stderr[:800]); sys.exit(1)
    cs = cases()
    print(f"{'LL':>3} {'IB':>3} {'oracle':>7} {'#0':>14} {'MAX':>14} {'@MAX':>26} {'total':>14}")
    for ll, ib in grid:
        tag = f"LL{ll}_IB{ib}"
        b = f"/tmp/sw_{tag}"
        rb = sh(f"g++-15 {FLAGS} -DFFT_LEAF_LOG={ll} -DINV_BASE={ib} -o {b} /tmp/opt_src.cpp")
        if rb.returncode:
            print(f"{ll:>3} {ib:>3} BUILD_FAIL"); continue
        ok, bad = oracle_ok(b)
        if not ok:
            print(f"{ll:>3} {ib:>3} ORACLE_FAIL@{bad}"); continue
        tot = 0; mx = (0,''); v0 = 0
        for ctag, cpath in cs:
            v = instr(b, cpath) or 0
            tot += v
            if v > mx[0]: mx = (v, ctag)
            if ctag == 'length_ratio_integer#0': v0 = v
        print(f"{ll:>3} {ib:>3} {'OK':>7} {v0:>14d} {mx[0]:>14d} {mx[1]:>26} {tot:>14d}", flush=True)
    print("=== DONE ===")

main()
