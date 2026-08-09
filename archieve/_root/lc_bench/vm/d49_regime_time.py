import subprocess, time, sys, os

REPS = 10
B47 = '/home/azzr/divbench/bin/div_D47'
B49 = '/home/azzr/divbench/bin/div_D49'

def bench(binpath, infile):
    ts = []
    for _ in range(REPS):
        with open(infile, 'rb') as f:
            t0 = time.perf_counter()
            p = subprocess.run([binpath], stdin=f, capture_output=True, timeout=200)
            t1 = time.perf_counter()
        if p.returncode != 0:
            raise RuntimeError(f'{binpath} rc={p.returncode} err={p.stderr[:200]}')
        ts.append((t1 - t0) * 1000.0)
    return min(ts), sorted(ts)[len(ts) // 2]

cases = sys.argv[1:]
print(f"{'case':22s} {'D47_min':>10s} {'D49_min':>10s} {'D49/D47':>9s}  verdict")
for c in cases:
    name = os.path.basename(c).replace('.txt', '')
    m47, med47 = bench(B47, c)
    m49, med49 = bench(B49, c)
    r = m49 / m47
    if m47 < 3.0:
        v = 'INCONCL(tiny)'
    elif r <= 0.97:
        v = 'D49 WINS'
    elif r >= 1.03:
        v = 'D49 LOSES'
    else:
        v = 'tie(noise)'
    print(f"{name:22s} {m47:9.2f}ms {m49:9.2f}ms {r:8.3f}  {v}", flush=True)
