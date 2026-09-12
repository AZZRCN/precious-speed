#!/usr/bin/env python3
# HEX division correctness battery: local LC gen (division_of_hex_big_integers) vs div_base16_cyc.exe
import subprocess, sys, os

HEX = 'D:/precious_speed/hex_best'
DIV = os.path.join(HEX, 'div_base16_cyc.exe')
GENBIN = os.path.join(HEX, 'genbin')

GENS = ['small','medium','large','max','a_max_b_random','power',
        'r_nearly_zero','length_ratio_integer','burnikel_ziegler_bound']

SEEDS = range(0, 100)  # 100 seeds each variant

def run(cmd, inp=None):
    p = subprocess.run(cmd, input=inp, capture_output=True)
    return p.returncode, p.stdout, p.stderr

def parse_io(text):
    # returns list of (A_hex, B_hex) and list of (q_hex, r_hex)
    lines = text.split(b'\n')
    # input: first line T, then A B per line
    # We re-parse gen output directly for oracle.
    return lines

def gen_cases(gen_out):
    lines = gen_out.split(b'\n')
    t = int(lines[0].strip())
    ab = []
    for i in range(1, 1+t):
        parts = lines[i].split()
        if len(parts) < 2: continue
        ab.append((parts[0].decode(), parts[1].decode()))
    return ab

def div_cases(div_out):
    out = []
    for ln in div_out.split(b'\n'):
        ln = ln.strip()
        if not ln: continue
        parts = ln.split()
        if len(parts) < 2: continue
        out.append((parts[0].decode(), parts[1].decode()))
    return out

fails = 0
total_cases = 0
checked = []
for g in GENS:
    gexe = os.path.join(GENBIN, g + '.exe')
    for seed in SEEDS:
        rc, gout, gerr = run([gexe, str(seed)])
        if rc != 0:
            print(f'[FAIL] gen {g} seed {seed} rc={rc} err={gerr[:200]}')
            fails += 1
            continue
        ab = gen_cases(gout)
        rc2, dout, derr = run([DIV], inp=gout)
        if rc2 != 0:
            print(f'[FAIL] div {g} seed {seed} rc={rc2} err={derr[:200]}')
            fails += 1
            continue
        got = div_cases(dout)
        if len(got) != len(ab):
            print(f'[FAIL] {g} seed {seed} linecount mismatch got={len(got)} exp={len(ab)}')
            fails += 1
            continue
        ok = True
        for (A,B),(q,r) in zip(ab, got):
            a = int(A, 16); b = int(B, 16)
            eq = a // b; er = a % b
            if (q.lower(), r.lower()) != (format(eq,'x'), format(er,'x')):
                # allow case-insensitive compare; also our output may uppercase
                if (q, r) != (format(eq,'X'), format(er,'X')) and (q.lower(), r.lower()) != (format(eq,'x'), format(er,'x')):
                    print(f'[MISMATCH] {g} seed {seed} A={A[:40]}.. B={B[:20]}.. exp q={format(eq,"x")[:40]} got q={q[:40]} exp r={format(er,"x")[:40]} got r={r[:40]}')
                    ok = False
                    fails += 1
                    break
        total_cases += len(ab)
        checked.append((g, seed, len(ab), ok))
        if not ok:
            break

print(f'=== DONE: variants={len(GENS)} seeds/variant={len(SEEDS)} total_cases={total_cases} fails={fails} ===')
if fails == 0:
    print('ALL CORRECT')
else:
    print('HAS FAILURES')
