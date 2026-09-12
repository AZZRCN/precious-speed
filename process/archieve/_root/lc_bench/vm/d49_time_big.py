import subprocess, time
REPS = 12
B47='/home/azzr/divbench/bin/div_D47'; B49='/home/azzr/divbench/bin/div_D49'
def bench(b, inf):
    ts=[]
    for _ in range(REPS):
        with open(inf,'rb') as f:
            t0=time.perf_counter(); p=subprocess.run([b],stdin=f,capture_output=True,timeout=200); t1=time.perf_counter()
        if p.returncode!=0: raise RuntimeError(f'{b} rc={p.returncode}')
        ts.append(t1-t0)
    return min(ts), sorted(ts)[len(ts)//2]
for tag in ['l16k','l20k']:
    inf=f'/home/azzr/divbench/in_big_{tag}.txt'
    m47,med47=bench(B47,inf); m49,med49=bench(B49,inf)
    print(f'{tag:5s} D47 min={m47*1000:8.2f}ms med={med47*1000:8.2f}ms | D49 min={m49*1000:8.2f}ms med={med49*1000:8.2f}ms | ratio(min)={m49/m47:.4f}', flush=True)
