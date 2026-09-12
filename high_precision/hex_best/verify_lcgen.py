#!/usr/bin/env python3
# HEX division correctness battery: run each LC gen -> div_base16_zn -> Python oracle.
import subprocess, sys, os, traceback

DIV = os.path.expanduser("~/precious_speed/hex_best/div_base16_zn")
GENBIN = os.path.expanduser("~/precious_speed/hex_best/genbin")
NSEED = int(sys.argv[1]) if len(sys.argv) > 1 else 8

# name -> seed base (None = no seed arg)
gens = {
    "small": None,
    "medium": 0,
    "large": 0,
    "max": 0,
    "a_max_b_random": 0,
    "power": 0,
    "r_nearly_zero": 0,
    "length_ratio_integer": 0,
    "burnikel_ziegler_bound": 0,
}

def run_div(inp):
    return subprocess.run([DIV], input=inp, capture_output=True, text=True).stdout

def check(inp, out):
    lines = inp.strip().split('\n')
    T = int(lines[0])
    pairs = [l.split() for l in lines[1:1+T]]
    olines = out.strip().split('\n')
    for i,(A,B) in enumerate(pairs):
        a = int(A,16); b = int(B,16)
        qa, qb = abs(a), abs(b)
        q = qa // qb; r = qa % qb
        parts = olines[i].split()
        qo = int(parts[0],16); ro = int(parts[1],16)
        if q != qo or r != ro:
            return False, i, (A, B, hex(q), hex(r), hex(qo), hex(ro))
    return True, None, None

total = 0; fails = 0
for name, base in gens.items():
    exe = os.path.join(GENBIN, name)
    try:
        if base is None:
            inp = subprocess.run([exe], capture_output=True, text=True).stdout
            out = run_div(inp)
            ok, idx, info = check(inp, out)
            total += 1
            if not ok:
                fails += 1; print(f"[FAIL] {name}: case{idx} {info}")
            else:
                print(f"[OK] {name}")
        else:
            ran = 0
            for s in range(base, base + NSEED):
                inp = subprocess.run([exe, str(s)], capture_output=True, text=True).stdout
                out = run_div(inp)
                ok, idx, info = check(inp, out)
                total += 1
                if not ok:
                    fails += 1
                    print(f"[FAIL] {name} seed={s}: case{idx} {info}")
                    break
                ran += 1
            print(f"[done] {name} ran={ran}/{NSEED}")
    except Exception as e:
        fails += 1; total += 1
        print(f"[ERROR] {name}: {e}")
        traceback.print_exc()

print(f"=== TOTAL={total} FAIL={fails} ===")
sys.exit(1 if fails else 0)
