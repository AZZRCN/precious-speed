import sys
sys.set_int_max_str_digits(800000)
BETA = 1 << 64

# reconstruct the test divisors from the same seed
import random
random.seed(20260813)
ns_list = [50, 100, 200, 300, 400, 600, 800, 1000, 1200, 1400, 1600, 1800,
           2000, 2200, 2400, 2525, 2600, 2800, 3000, 3200, 3514, 4000]
tests = []
for n in ns_list:
    for rep in range(3):
        top = random.randint(1 << 63, (1 << 64) - 1)
        limbs = [random.randint(0, (1 << 64) - 1) for _ in range(n - 1)]
        limbs.append(top)
        d = 0
        for v in reversed(limbs):
            d = (d << 64) | v
        tests.append((n, d))

lines = open(r'D:\precious_speed\tools\inv_sweep_out.txt').read().split('\n')
# each line: "<t> <n> <Vhex>"
fails = []
for ln in lines:
    if not ln.strip():
        continue
    parts = ln.split()
    t = int(parts[0]); n = int(parts[1]); vhex = parts[2]
    n_idx = t
    n_true, d = tests[n_idx]
    assert n == n_true, (n, n_true)
    bn = BETA ** n
    b2n = BETA ** (2 * n)
    true_v = (b2n // d) - bn
    vgot = int(vhex, 16)
    diff = true_v - vgot
    status = 'OK' if diff >= 0 and abs(diff).bit_length() <= 4 else ('UNDER' if diff > 0 else 'OVER')
    if status != 'OK':
        fails.append((n, status, abs(diff).bit_length()))
        print(f"t={t} n={n} {status} |diff|bits={abs(diff).bit_length()} trueVbits={true_v.bit_length()}")
print("---- summary by n ----")
from collections import defaultdict
by_n = defaultdict(list)
for i,(n,s,db) in enumerate(fails):
    by_n[n].append(s)
for n in ns_list:
    tot = 3
    f = by_n.get(n, [])
    print(f"n={n:5d}: fails={len(f)}/3  {f}")
