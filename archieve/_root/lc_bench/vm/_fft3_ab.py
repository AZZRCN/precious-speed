#!/usr/bin/env python3
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
