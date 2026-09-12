#!/usr/bin/env python3
# Test: route na<=2nb (newton) to bz_divide via BZALL=1. Check correctness + #0/MAX instructions.
import subprocess, os, glob, sys

CASEDIR = '/tmp/lccases'
FLAGS = "-O3 -std=c++20 -march=znver3 -mtune=znver3 -w"
TESTS = [('small',1),('medium',3),('large',2),('max',3),('a_max_b_random',3),
         ('power',1),('r_nearly_zero',3),('length_ratio_integer',6),('burnikel_ziegler_bound',4)]

def sh(c): return subprocess.run(c, shell=True, capture_output=True, text=True)

def cases():
    out=[]
    for name,num in TESTS:
        for seed in range(num):
            p=f'{CASEDIR}/{name}_{seed}.in'
            if os.path.isfile(p): out.append((f'{name}#{seed}',p))
    return out

def oracle_ok(binp, env):
    for tag,path in cases():
        o=subprocess.run([binp],stdin=open(path),capture_output=True,env={**os.environ,**env})
        r=subprocess.run(['/tmp/ref'],stdin=open(path),capture_output=True)
        if o.stdout!=r.stdout: return False,tag
    return True,''

def instr(binp,path,env):
    p=subprocess.run(['perf','stat','-e','instructions:u','-x,',binp],
                     stdin=open(path),capture_output=True,text=True,env={**os.environ,**env})
    for line in p.stderr.splitlines():
        if 'instructions:u' in line:
            try: return int(line.split(',')[0])
            except ValueError: pass
    return None

def main():
    cs=cases()
    # build ref (baseline) and opt with BZALL variant
    sh(f"g++-15 {FLAGS} -o /tmp/ref /tmp/ref_src.cpp")
    # build opt normally (BZALL off) and BZALL on
    r1=sh(f"g++-15 {FLAGS} -o /tmp/opt_off /tmp/opt_src.cpp")
    r2=sh(f"g++-15 {FLAGS} -o /tmp/opt_bz /tmp/opt_src.cpp")
    if r1.returncode or r2.returncode:
        print("BUILD_FAIL"); print(r1.stderr[:500],r2.stderr[:500]); sys.exit(1)
    # sanity: opt_off must match ref
    ok0,bad0=oracle_ok('/tmp/opt_off',{})
    print("opt_off oracle:", "OK" if ok0 else f"FAIL@{bad0}")
    # BZALL=1 correctness
    ok,bad=oracle_ok('/tmp/opt_bz',{'BZALL':'1'})
    print("BZALL=1 oracle:", "OK" if ok else f"FAIL@{bad}")
    if not ok:
        print("BZALL correctness FAILED -> abort"); sys.exit(1)
    # measure
    for label,binp,env in [("OFF",'/tmp/opt_off',{}),("BZALL1",'/tmp/opt_bz',{'BZALL':'1'})]:
        tot=0; mx=(0,''); v0=0
        for tag,path in cs:
            v=instr(binp,path,env) or 0
            tot+=v
            if v>mx[0]: mx=(v,tag)
            if tag=='length_ratio_integer#0': v0=v
        print(f"{label:7} #0={v0:>13d}  MAX={mx[0]:>13d} @ {mx[1]:<26} total={tot:>13d}",flush=True)
    print("=== DONE ===")

main()
