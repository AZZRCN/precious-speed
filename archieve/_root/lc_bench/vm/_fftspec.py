#!/usr/bin/env python3
import subprocess, os, math, csv
R='/home/azzr/divbench'; IN='/home/azzr/lcp/big_integer/division_of_big_integers/in/'
SRC='div_D40'; CASES=['length_ratio_integer_00']
os.chdir(R); os.makedirs('bin', exist_ok=True)
tag='fs_'+SRC
cmd='g++ -O2 -std=c++23 -march=x86-64-v3 -DCEILHIST -o bin/%s src/%s.cpp'%(tag,SRC)
r=subprocess.run(cmd,shell=True,capture_output=True,text=True)
if r.returncode!=0:
    print('COMPILE FAIL'); print(r.stderr[-3000:]); raise SystemExit
TAG={'0':'lin','1':'mn','2':'cycm'}
for case in CASES:
    csvf='/tmp/fs_%s_%s.csv'%(SRC,case)
    env=dict(os.environ); env['CEILHIST_CSV']=csvf
    subprocess.run('./bin/%s < %s%s.in > /dev/null'%(tag,IN,case),
                   shell=True,capture_output=True,text=True,env=env)
    if not os.path.exists(csvf):
        print('##### %s : no csv'%case); continue
    rows=[r for r in csv.DictReader(open(csvf)) if int(r['need'])>=2]
    W=lambda n: n*math.log2(n) if n>1 else 0.0
    tot=sum(W(int(r['used']))*int(r['cnt']) for r in rows)
    idl=sum(W(int(r['need']))*int(r['cnt']) for r in rows)
    ncall=sum(int(r['cnt']) for r in rows)
    print('##### %s   calls=%d  sum used*NlogN=%.6g  ideal=%.6g  overhead=%+.2f%%'
          %(case,ncall,tot,idl,100.0*(tot/idl-1.0)), flush=True)
    print('  %-6s %10s %10s %7s %12s %7s  %s'%('tag','need','used','cnt','NlogN','share','waste'), flush=True)
    rows.sort(key=lambda r: -W(int(r['used']))*int(r['cnt']))
    acc=0.0
    for r in rows[:22]:
        need,used,cnt=int(r['need']),int(r['used']),int(r['cnt'])
        w=W(used)*cnt; acc+=w
        print('  %-6s %10d %10d %7d %12.5g %6.2f%%  %+.1f%%'
              %(TAG.get(r['tag'],r['tag']),need,used,cnt,w,100.0*w/tot,
                100.0*(W(used)/W(need)-1.0) if need>1 else 0.0), flush=True)
    print('  (top22 covers %.1f%%)'%(100.0*acc/tot), flush=True)
    agg={}
    for r in rows:
        t=TAG.get(r['tag'],r['tag']); need,used,cnt=int(r['need']),int(r['used']),int(r['cnt'])
        a=agg.setdefault(t,[0.0,0.0,0])
        a[0]+=W(used)*cnt; a[1]+=W(need)*cnt; a[2]+=cnt
    for t,(u,n,c) in sorted(agg.items()):
        print('   [%s] calls=%d used=%.5g ideal=%.5g overhead=%+.2f%% share=%.1f%%'
              %(t,c,u,n,100.0*(u/n-1.0),100.0*u/tot), flush=True)
    print(flush=True)
print('DONE', flush=True)
