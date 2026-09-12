import subprocess, time, sys
REPS = 15
BIN_D47 = '/home/azzr/divbench/bin/div_D47'
BIN_D49 = '/home/azzr/divbench/bin/div_D49'
def bench(binpath, infile):
    ts = []
    for _ in range(REPS):
        with open(infile,'rb') as f:
            t0 = time.perf_counter()
            p = subprocess.run([binpath], stdin=f, capture_output=True, timeout=120)
            t1 = time.perf_counter()
        if p.returncode != 0:
            raise RuntimeError(f'{binpath} rc={p.returncode}')
        ts.append(t1 - t0)
    return min(ts), sorted(ts)[len(ts)//2]
for kind in ['rnz','amax','bz']:
    inf = f'/home/azzr/divbench/in_{kind}.txt'
    m47, med47 = bench(BIN_D47, inf)
    m49, med49 = bench(BIN_D49, inf)
    ratio = m49 / m47
    print(f'{kind:5s} D47 min={m47*1000:7.2f}ms med={med47*1000:7.2f}ms | D49 min={m49*1000:7.2f}ms med={med49*1000:7.2f}ms | ratio(min)={ratio:.4f}  (D49/D47)', flush=True)
