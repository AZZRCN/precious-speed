#!/usr/bin/env python3
import subprocess, os, time
R='/home/azzr/divbench'; IN='/home/azzr/lcp/big_integer/division_of_big_integers/in/'
SRC='div_D39'; CASES=['length_ratio_integer_00']
os.chdir(R); os.makedirs('bin', exist_ok=True)
tag='ip_'+SRC
cmd='g++ -O2 -std=c++23 -march=x86-64-v3 -DINVPROF -o bin/%s src/%s.cpp'%(tag,SRC)
r=subprocess.run(cmd,shell=True,capture_output=True,text=True)
if r.returncode!=0:
    print('COMPILE FAIL'); print(r.stderr[-3000:]); raise SystemExit
for case in CASES:
    # 先量总时间 (同一个 -DINVPROF 二进制, 保证可比)
    best=1e9
    for _ in range(3):
        t0=time.perf_counter()
        subprocess.run('./bin/%s < %s%s.in > /dev/null 2>/tmp/ip.err'%(tag,IN,case),
                       shell=True)
        best=min(best,(time.perf_counter()-t0)*1000)
    print('##### %s   wall(best of 3, with INVPROF overhead) = %.2f ms'%(case,best), flush=True)
    print(open('/tmp/ip.err').read(), flush=True)
print('DONE', flush=True)
