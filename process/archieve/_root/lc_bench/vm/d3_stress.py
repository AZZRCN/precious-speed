import subprocess, os, sys

GEN_DIR = '/home/azzr/lcgen/division_of_big_integers/gen'
COMMON = '/home/azzr/lcgen/common'
ORIG = '/home/azzr/divbench/bin/div_orig'
D3 = '/home/azzr/divbench/bin/div_D3'

# generator -> number of seeds to sweep
GENS = {
    'a_max_b_random': 24,         # alen=2e6, blen random -> many quotient blocks (multi-block cyclic stress)
    'length_ratio_integer': 48,    # B*m=A, m in {2,3,5,10,20,50}; large operands, quotient blocks vary
    'r_nearly_zero': 80,          # remainder ~0 -> hardest for r-based cyclic correction (D3 risk)
    'medium': 40,
    'large': 24,
    'max': 24,
    'small': 40,
    'burnikel_ziegler_bound': 24,
}

bins = {}
for g in GENS:
    cpp = f'{GEN_DIR}/{g}.cpp'
    out = f'/tmp/gen_{g}'
    r = subprocess.run(['g++', '-O2', '-std=c++17', '-I', COMMON, cpp, '-o', out],
                       capture_output=True, text=True)
    if r.returncode != 0:
        print(f'COMPILE_FAIL {g}: {r.stderr[:400]}', flush=True)
    else:
        bins[g] = out
        print(f'compiled {g}', flush=True)

def run_bin(binpath, infile):
    with open(infile, 'rb') as f:
        p = subprocess.run([binpath], stdin=f, capture_output=True)
    return p.stdout, p.returncode

total = mismatch = runerr = 0
for g, nseed in GENS.items():
    if g not in bins:
        continue
    gb = bins[g]
    for seed in range(1, nseed + 1):
        inf = f'/tmp/stress_{g}_{seed}.in'
        with open(inf, 'wb') as f:
            subprocess.run([gb, str(seed)], stdout=f)
        o, rc1 = run_bin(ORIG, inf)
        d, rc2 = run_bin(D3, inf)
        total += 1
        if rc1 != 0 or rc2 != 0:
            runerr += 1
            print(f'RUNERR {g} seed={seed} rc1={rc1} rc2={rc2}', flush=True)
            continue
        if o != d:
            mismatch += 1
            ol = o.split(b'\n'); dl = d.split(b'\n')
            for i in range(min(len(ol), len(dl))):
                if ol[i] != dl[i]:
                    print(f'MISMATCH {g} seed={seed} line {i}: orig={ol[i][:50]!r} d3={dl[i][:50]!r}', flush=True)
                    break
            if len(ol) != len(dl):
                print(f'  (len orig={len(ol)} d3={len(dl)})', flush=True)
    print(f'--- done {g} (so far total={total} mismatch={mismatch} runerr={runerr}) ---', flush=True)

print(f'=== TOTAL={total} MISMATCH={mismatch} RUNERR={runerr} ===', flush=True)
