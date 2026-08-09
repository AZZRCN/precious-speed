# D4 build + verification (v2: fix disk quota + digit limit).
# 1) clean /tmp, build div_D4 (-O2 -std=c++23 -march=x86-64-v3), detect build failure
# 2) verify_official.py div  (26 official cases, must be AC)
# 3) stress: div_D4 vs div_orig across official-gen distributions
# 4) targeted sweep: in<64 band (clamp to 64, linear) + mu_in>len2 band (clamp to len2, cyclic)
#    includes negatives.  0 mismatch required.
import subprocess, os, sys, random
sys.set_int_max_str_digits(2000000)

SRC = '/home/azzr/divbench/src/div_D4.cpp'
BIN = '/home/azzr/divbench/bin/div_D4'
ORIG = '/home/azzr/divbench/bin/div_orig'
GEN_DIR = '/home/azzr/lcgen/division_of_big_integers/gen'
COMMON = '/home/azzr/lcgen/common'
FLAGS = "-O2 -std=c++23 -march=x86-64-v3"

def sh(cmd):
    r = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    return r.returncode, r.stdout, r.stderr

# ---------- 0) clean /tmp (avoid disk quota) ----------
sh("rm -f /tmp/stress_*.in /tmp/gen_* /tmp/cc* /tmp/d4_build.log 2>/dev/null; df -h /tmp | tail -1")

# ---------- 1) build ----------
sh(f"g++ {FLAGS} -o {BIN} {SRC} > /tmp/d4_build.log 2>&1; echo BUILD_RC=$? >> /tmp/d4_build.log")
with open('/tmp/d4_build.log') as f:
    blog = f.read()
print("=== BUILD ===")
print(blog[-1500:])
if 'BUILD_RC=0' not in blog:
    print("BUILD FAILED"); sys.exit(1)

# ---------- 2) verify_official ----------
rc, out, err = sh(f"python3 ~/verify_official.py div {BIN} 2>&1 | tail -8")
print("=== verify_official ===")
print(out)
if 'FAIL' in out or 'MISMATCH' in out or 'RE' in out or 'BAD=2' in out or 'BAD=' in out:
    # require all OK
    if 'OK=26' not in out:
        print("VERIFY FAILED"); sys.exit(1)

# ---------- 3) official-gen stress (orig vs D4) ----------
GENS = {
    'a_max_b_random': 24, 'length_ratio_integer': 48, 'r_nearly_zero': 80,
    'medium': 40, 'large': 24, 'max': 24, 'small': 40, 'burnikel_ziegler_bound': 24,
}
bins = {}
for g, n in GENS.items():
    cpp = f'{GEN_DIR}/{g}.cpp'
    outp = f'/tmp/gen_{g}'
    r = subprocess.run(['g++', '-O2', '-std=c++17', '-I', COMMON, cpp, '-o', outp],
                       capture_output=True, text=True)
    if r.returncode == 0:
        bins[g] = outp
        print(f'compiled {g}', flush=True)
    else:
        print(f'COMPILE_FAIL {g}', flush=True)

def run_bin(bp, inf):
    with open(inf, 'rb') as f:
        p = subprocess.run([bp], stdin=f, capture_output=True)
    return p.stdout, p.returncode

total = mismatch = runerr = 0
for g, n in GENS.items():
    if g not in bins: continue
    gb = bins[g]
    for seed in range(1, n + 1):
        inf = f'/tmp/stress_{g}_{seed}.in'
        with open(inf, 'wb') as f:
            subprocess.run([gb, str(seed)], stdout=f)
        o, rc1 = run_bin(ORIG, inf)
        d, rc2 = run_bin(BIN, inf)
        total += 1
        if rc1 or rc2:
            runerr += 1
            print(f'RUNERR {g} seed={seed} rc1={rc1} rc2={rc2}', flush=True)
            continue
        if o != d:
            mismatch += 1
            ol = o.split(b'\n'); dl = d.split(b'\n')
            for i in range(min(len(ol), len(dl))):
                if ol[i] != dl[i]:
                    print(f'MISMATCH {g} seed={seed} line {i}: orig={ol[i][:60]!r} d4={dl[i][:60]!r}', flush=True)
                    break
    print(f'--- done {g} (total={total} mismatch={mismatch} runerr={runerr}) ---', flush=True)
    sh("rm -f /tmp/stress_*.in 2>/dev/null")  # keep gen binaries; only drop inputs to save space

# ---------- 4) targeted sweep (orig vs D4) ----------
def rand_bigint(nlimbs, rng):
    lo = 10000 ** (nlimbs - 1)
    hi = 10000 ** nlimbs
    return rng.randrange(lo, hi)

def run_pair(a, b):
    inp = f"1\n{a} {b}\n".encode()
    p1 = subprocess.run([ORIG], input=inp, capture_output=True)
    p2 = subprocess.run([BIN], input=inp, capture_output=True)
    return p1.stdout, p2.stdout, p1.returncode, p2.returncode

regimes = []
for len2 in (65, 100, 130):                 # in<64 band (ratio~2), clamp to 64 -> linear
    for qn in (len2, len2 + 30, len2 + 60):
        regimes.append((len2, qn))
for len2 in (100, 200):                      # mu_in>len2 high-ratio band, clamp to len2 -> cyclic
    for qn in (3 * len2, 10 * len2):
        regimes.append((len2, qn))
for len2 in (100, 200):                      # normal [64,len2] band (unchanged from D3)
    for qn in (int(len2 * 0.6), len2):
        regimes.append((len2, qn))

ts_total = ts_mismatch = ts_runerr = 0
rng = random.Random(20260802)
for (len2, qn) in regimes:
    for _ in range(8):
        b = rand_bigint(len2, rng)
        a = rand_bigint(len2 + qn, rng)
        if rng.random() < 0.5: a = -a
        if rng.random() < 0.5: b = -b
        if b == 0: b = 1
        o, d, rc1, rc2 = run_pair(a, b)
        ts_total += 1
        if rc1 or rc2:
            ts_runerr += 1
            print(f'TARGET_RUNERR len2={len2} qn={qn} rc1={rc1} rc2={rc2}', flush=True)
            continue
        if o != d:
            ts_mismatch += 1
            print(f'TARGET_MISMATCH len2={len2} qn={qn} a={str(a)[:30]!r} b={str(b)[:30]!r}', flush=True)
            print(f'  orig={o!r} d4={d!r}', flush=True)
    print(f'--- regime len2={len2} qn={qn} done (ts_total={ts_total} ts_mismatch={ts_mismatch}) ---', flush=True)

print(f'=== OFFICIAL_STRESS total={total} mismatch={mismatch} runerr={runerr} ===')
print(f'=== TARGETED_SWEEP total={ts_total} mismatch={ts_mismatch} runerr={ts_runerr} ===')
if mismatch or ts_mismatch or runerr or ts_runerr:
    print('RESULT: FAIL')
    sys.exit(1)
print('RESULT: PASS')
