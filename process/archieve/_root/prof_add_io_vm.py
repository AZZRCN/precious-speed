#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""prof_add_io_vm.py —— 在 VM(.66) 用 add.cpp 自带 -DPROFILE_DIV 拿 ADD 的 I/O vs 计算拆分。
read=token扫描 parse=大数构建+进位加+格式化 write=输出 nl=换行 loop=循环总 flush=刷缓冲
用法: python prof_add_io_vm.py
"""
import os, sys, time
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "lc_bench", "vm"))
from vmctl import put, run

REMOTE = r'''
import subprocess, os, re, random, time
H = '/home/azzr/addmulbench'
os.makedirs(H+'/in', exist_ok=True); os.makedirs(H+'/bin', exist_ok=True)
SRC=H+'/src'; IN=H+'/in'; BIN=H+'/bin'
def rnd(n): return ''.join(random.choices('0123456789', k=n))
def gen(name,n1,n2,seed=0):
    random.seed(seed)
    with open(IN+name+'.in','w') as f:
        f.write('1\n'+rnd(n1)+' '+rnd(n2)+'\n')
CASES=[('small',3000,3000),('medium',300000,300000),('large',1200000,1200000),
       ('max',2000000,2000000),('unbal',2000000,40)]
for nm,n1,n2 in CASES:
    gen(nm,n1,n2,seed=abs(hash(nm))%1000000)
def sh(c): return subprocess.run(c,shell=True,capture_output=True,text=True)

print("=== compile add (-DPROFILE_DIV) ===", flush=True)
r=sh("cd %s && g++ -O2 -std=c++23 -march=x86-64-v3 -DPROFILE_DIV src/add.cpp -o bin/add_d 2>&1 | tail -15" % H)
print(r.stdout or r.stderr, flush=True)

print("\n========== ADD I/O split (PROFILE_DIV, min of 3 runs) ==========", flush=True)
import glob
def find_prof():
    for cand in (H+'/prof_result.txt', '/home/azzr/prof_result.txt', os.path.expanduser('~/prof_result.txt')):
        if os.path.exists(cand): return cand
    hits=glob.glob('/home/azzr/**/prof_result.txt', recursive=True)
    return hits[0] if hits else None
for nm,n1,n2 in CASES:
    f=IN+nm+'.in'; best_line=None; best_t=1e9
    for _ in range(3):
        t0=time.perf_counter()
        p=subprocess.run("cd %s && ./bin/add_d < %s > /dev/null 2>&1" % (H,f),shell=True)
        dt=(time.perf_counter()-t0)*1000
        cand=find_prof()
        if cand:
            line=open(cand).read().strip()
            try: os.remove(cand)
            except: pass
            if dt<best_t: best_t=dt; best_line=line
    print("--- %-7s wall=%.2f ms | %s" % (nm,best_t,best_line), flush=True)
print("DONE", flush=True)
'''
with open("/tmp/addio_remote.py","w") as f: f.write(REMOTE)
put(os.path.join(HERE,"best","add.cpp"), "/home/azzr/addmulbench/src/add.cpp")
put("/tmp/addio_remote.py", "/home/azzr/addmulbench/run_io.py")
rc,out,err = run("python3 /home/azzr/addmulbench/run_io.py 2>&1 | tail -40", timeout=600)
print(out or err)
