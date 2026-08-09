#!/usr/bin/env python3
"""Time div_D47 vs div_D50 on the three headline cases (VM == LC env, same binary
baseline => relative ratio is a valid directional signal even with wall-clock noise).
D50 narrows the short-quotient Knuth threshold so rnz-like short-q large-divisor routes
to Newton/FFT multiply-subtract instead of per-digit Knuth submul.
"""
import subprocess, time, os

REPS = 20
B47 = '/home/azzr/divbench/bin/div_D47'
B50 = '/home/azzr/divbench/bin/div_D50'
CASES = [
    ('rnz_01', 'r_nearly_zero_01.in'),
    ('amax',   'a_max_b_random_02.in'),
    ('bz_02',  'burnikel_ziegler_bound_02.in'),
]

def bench(b, inf):
    ts = []
    for _ in range(REPS):
        with open(inf, 'rb') as f:
            t0 = time.perf_counter()
            p = subprocess.run([b], stdin=f, capture_output=True, timeout=180)
            t1 = time.perf_counter()
        if p.returncode != 0:
            raise RuntimeError('%s rc=%d' % (b, p.returncode))
        ts.append((t1 - t0) * 1000.0)
    ts.sort()
    return ts[0], ts[len(ts) // 2]

print('%-8s %10s %10s %10s  %s' % ('case', 'D47_min', 'D50_min', 'ratio', 'verdict'), flush=True)
for name, fn in CASES:
    inf = '/home/azzr/divbench/cases/' + fn
    m47, med47 = bench(B47, inf)
    m50, med50 = bench(B50, inf)
    r = m50 / m47
    v = 'D50 WINS' if r <= 0.97 else ('D50 LOSES' if r >= 1.03 else 'tie')
    print('%-8s %9.2fms %9.2fms %9.3f  %s' % (name, m47, m50, r, v), flush=True)
