#!/usr/bin/env python3
# Sweep INV_BASE at fixed FFT_LEAF_LOG=8. Report #0 & MAX.
import subprocess, os, sys
CASEDIR='/tmp/lccases'
FLAGS="-O3 -std=c++20 -march=znver3 -mtune=znver3 -w"
TESTS=[('small',1),('medium',3),('large',2),('max',3),('a_max_b_random',3),
       ('power',1),('r_nearly_zero',3),('length_ratio_integer',6),('burnikel_ziegler_bound',4)]
def sh(c): return subprocess.run(c,shell=True,capture_output=True,text=True)
def cases():
    out=[]
    for n,num in TESTS:
        for s in range(num):
            p=f'{CASEDIR}/{n}_{s}.in'
            if os.path.isfile(p): out.append((f'{n}#{s}',p))
    return out
def oracle_ok(b):
    for t,p in cases():
        o=subprocess.run([b],stdin=open(p),capture_output=True)
        r=subprocess.run(['/tmp/ref'],stdin=open(p),capture_output=True)
        if o.stdout!=r.stdout: return False,t
    return True,''
def instr(b,p):
    x=subprocess.run(['perf','stat','-e','instructions:u','-x,',b],stdin=open(p),capture_output=True,text=True)
    for l in x.stderr.splitlines():
        if 'instructions:u' in l:
            try: return int(l.split(',')[0])
            except ValueError: pass
    return None
def main():
    sh(f"g++-15 {FLAGS} -o /tmp/ref /tmp/ref_src.cpp")
    cs=cases()
    print(f"{'IB':>4} {'oracle':>7} {'#0':>14} {'MAX':>14} {'@MAX':>26} {'total':>14}")
    for ib in (32,48,64,96,128):
        b=f"/tmp/inv_{ib}"
        rb=sh(f"g++-15 {FLAGS} -DFFT_LEAF_LOG=8 -DINV_BASE={ib} -o {b} /tmp/opt_src.cpp")
        if rb.returncode: print(f"{ib:>4} BUILD_FAIL"); continue
        ok,bad=oracle_ok(b)
        if not ok: print(f"{ib:>4} ORACLE_FAIL@{bad}"); continue
        tot=0; mx=(0,''); v0=0
        for t,p in cs:
            v=instr(b,p) or 0; tot+=v
            if v>mx[0]: mx=(v,t)
            if t=='length_ratio_integer#0': v0=v
        print(f"{ib:>4} {'OK':>7} {v0:>14d} {mx[0]:>14d} {mx[1]:>26} {tot:>14d}",flush=True)
    print("=== DONE ===")
main()
