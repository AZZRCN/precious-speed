#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""A/B: radix-3 档位 开 vs 关, 测 lri_03 的 Zen3 est。

目的: 标定「FFT 工作量 W -> est_cycles」的传导系数 f。
      lri_03 在 NO_FFT3 下 Fi 98304->131072, Fc 49152->65536,
      模型 W 从 56.62M -> 77.40M (+36.7%)。
      若 est 增幅 ≈ 0.755*36.7% = +27.7% (即 530.5M -> 677M),
      则 f≈0.755 成立, 可放心用它预测「档位细化 (radix-5)」的收益。
      若 est 增幅明显更小, 说明 radix-3 每单位 W 更贵(实现低效),
      那么优化 radix-3 内核本身就是一根独立杠杆。
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = r"D:\precious_speed\best\div_D16.cpp"
LOG = "/home/azzr/divbench/logs/fft3_ab.log"

print("uploading D16 ...")
put(SRC, "/home/azzr/divbench/src/div_D16.cpp")

print("compiling d16 / d16_nf3 ...")
rc, out, err = run(
    "cd /home/azzr/divbench && mkdir -p bin && "
    "g++ -O2 -std=c++23 -march=x86-64-v3 -o bin/d16 src/div_D16.cpp 2>&1 | tail -5 && "
    "g++ -O2 -std=c++23 -march=x86-64-v3 -DNO_FFT3 -o bin/d16_nf3 src/div_D16.cpp 2>&1 | tail -5 && "
    "ls -la bin/d16 bin/d16_nf3",
    timeout=900,
)
print(out, err)

REMOTE = r'''#!/usr/bin/env python3
import subprocess, os
IN='/home/azzr/lcp/big_integer/division_of_big_integers/in/'
CACHE='--cache-sim=yes --D1=32768,8,64 --I1=32768,8,64 --LL=33554432,16,64'
JOBS=[('d16','length_ratio_integer_03'),('d16_nf3','length_ratio_integer_03'),
      ('d16','length_ratio_integer_02'),('d16_nf3','length_ratio_integer_02')]
def measure(b,c):
    o='/tmp/ab_%s_%s.out'%(b,c)
    cmd=('valgrind --tool=callgrind '+CACHE+' --callgrind-out-file='+o+
         ' ./bin/'+b+' < '+IN+c+'.in > /dev/null 2>/dev/null')
    if subprocess.run(cmd,shell=True,cwd='/home/azzr/divbench').returncode!=0: return None
    ev=tot=None
    for line in open(o):
        if line.startswith('events:'): ev=line.split()[1:]
        elif line.startswith('summary:') or line.startswith('totals:'):
            tot=[int(x) for x in line.split()[1:]]
    return dict(zip(ev,tot)) if ev and tot else None
print('%-10s %-26s %13s %11s %9s %13s'%('bin','case','Ir','D1m','DLm','est'),flush=True)
res={}
for b,c in JOBS:
    d=measure(b,c)
    if not d: print(b,c,'FAIL',flush=True); continue
    Ir=d.get('Ir',0); d1=d.get('D1mr',0)+d.get('D1mw',0); dl=d.get('DLmr',0)+d.get('DLmw',0)
    est=Ir+5*d1+200*dl; res[(b,c)]=est
    print('%-10s %-26s %13d %11d %9d %13d'%(b,c,Ir,d1,dl,est),flush=True)
print(flush=True)
for c in ['length_ratio_integer_03','length_ratio_integer_02']:
    a=res.get(('d16',c)); n=res.get(('d16_nf3',c))
    if a and n:
        print('%s: fft3=%d  nofft3=%d  ratio=%.4f  (nofft3 比 fft3 贵 %+.2f%%)'%(c,a,n,n/a,100*(n/a-1)),flush=True)
print('DONE',flush=True)
'''
p = os.path.join(HERE, "_fft3_ab.py")
with open(p, "w", encoding="utf-8", newline="\n") as f:
    f.write(REMOTE)
put(p, "/home/azzr/fft3_ab.py")
run("mkdir -p /home/azzr/divbench/logs && rm -f " + LOG)
run(f"cd /home/azzr && nohup python3 fft3_ab.py > {LOG} 2>&1 & echo started", timeout=30)
print("started; poll with: python fft3_ab.py poll")
