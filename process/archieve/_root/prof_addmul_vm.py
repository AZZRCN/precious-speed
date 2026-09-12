#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""prof_addmul_vm.py —— 在 Linux VM(.66) 上画像 ADD / MUL 当前最佳的瓶颈。

为什么在 VM 跑: div_*.cpp / add.cpp 的 initInput 在 __linux__ 走 mmap 零拷贝，
Windows 走 fread 退化路径，read/parse 划分不同 => 本机画像不能外推 LC。
ADD 无内置 phase profiler，用 callgrind 函数级归因; MUL 有 -DPROFILE_MUL 五段。

用法: python prof_addmul_vm.py        # up+go+poll 一次跑完 (后台)
"""
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "lc_bench", "vm"))
from vmctl import put, run  # noqa: E402

REMOTE = r'''
import subprocess, os, re, random, time

H = '/home/azzr/addmulbench'
os.makedirs(H+'/in', exist_ok=True); os.makedirs(H+'/bin', exist_ok=True)
SRC=H+'/src'; IN=H+'/in'; BIN=H+'/bin'

def rnd(n):
    return ''.join(random.choices('0123456789', k=n))

def gen(name,n1,n2,seed=0):
    random.seed(seed)
    # 格式: 首行 t=用例数, 之后每行 "A B" (空格分隔一个 case). 两文件 main() 都按此解析.
    with open(IN+name+'.in','w') as f:
        f.write('1\n'+rnd(n1)+' '+rnd(n2)+'\n')

# 大小阶梯: small/medium/large/max + unbal(一长一短, 压 ADD/MUL 的非平衡路径)
CASES=[('small',3000,3000),('medium',300000,300000),('large',1200000,1200000),
       ('max',2000000,2000000),('unbal',2000000,40)]
for nm,n1,n2 in CASES:
    gen(nm,n1,n2,seed=abs(hash(nm))%1000000)

print("=== sanity: mul_p<small.in stdout(head 24)+len ===", flush=True)
p=subprocess.run([BIN+'/mul_p'],stdin=open(IN+'small.in','rb'),stdout=subprocess.PIPE,stderr=subprocess.DEVNULL)
print("  mul_out_len=%d head=%r" % (len(p.stdout), p.stdout[:24]), flush=True)
print("=== sanity: add_p<small.in stdout(head 24)+len ===", flush=True)
p=subprocess.run([BIN+'/add_p'],stdin=open(IN+'small.in','rb'),stdout=subprocess.PIPE,stderr=subprocess.DEVNULL)
print("  add_out_len=%d head=%r" % (len(p.stdout), p.stdout[:24]), flush=True)

def sh(c):
    return subprocess.run(c,shell=True,capture_output=True,text=True)

print("=== compile mul (-DPROFILE_MUL) ===", flush=True)
r=sh("cd %s && g++ -O2 -std=c++23 -march=x86-64-v3 -DPROFILE_MUL src/mul.cpp -o bin/mul_p 2>&1 | tail -15" % H)
print(r.stdout or r.stderr, flush=True)
print("=== compile add (-O2 -g) ===", flush=True)
r=sh("cd %s && g++ -O2 -std=c++23 -march=x86-64-v3 -g src/add.cpp -o bin/add_p 2>&1 | tail -15" % H)
print(r.stdout or r.stderr, flush=True)

print("\n========== MUL phase profile (min wall ms + phases) ==========", flush=True)
for nm,n1,n2 in CASES:
    f=IN+nm+'.in'; best=None; ph=None
    for _ in range(5):
        t0=time.perf_counter()
        p=subprocess.run([BIN+'/mul_p'],stdin=open(f,'rb'),stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
        dt=(time.perf_counter()-t0)*1000
        if best is None or dt<best: best=dt; ph=p.stderr
    print("--- %s  wall=%.1f ms ---" % (nm,best), flush=True)
    for l in ph.decode('utf-8','replace').splitlines():
        s=l.strip()
        if s and not s.startswith('----') and re.search(r'\d', s):
            print("   "+s, flush=True)

print("\n========== ADD wall timing (min ms) ==========", flush=True)
for nm,n1,n2 in CASES:
    f=IN+nm+'.in'; best=None
    for _ in range(5):
        t0=time.perf_counter()
        subprocess.run([BIN+'/add_p'],stdin=open(f,'rb'),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        dt=(time.perf_counter()-t0)*1000
        if best is None or dt<best: best=dt
    print("   %-8s %.1f ms" % (nm,best), flush=True)

print("\n========== ADD callgrind (large, 函数级 Ir 归因) ==========", flush=True)
sh("valgrind --tool=callgrind --callgrind-out-file=%s/cg_add.out %s/add_p < %s/medium.in > /dev/null 2>&1" % (H,BIN,IN))
r=sh("callgrind_annotate --auto=yes --threshold=1 %s/cg_add.out 2>/dev/null | head -45" % H)
print(r.stdout, flush=True)
print("DONE", flush=True)
'''

with open("/tmp/addmul_remote.py", "w") as f:
    f.write(REMOTE)

put(os.path.join(HERE, "best", "add.cpp"), "/home/azzr/addmulbench/src/add.cpp")
put(os.path.join(HERE, "best", "mul.cpp"), "/home/azzr/addmulbench/src/mul.cpp")
put("/tmp/addmul_remote.py", "/home/azzr/addmulbench/run.py")

rc, out, err = run("python3 /home/azzr/addmulbench/run.py 2>&1 | tail -90", timeout=900)
print(out or err)
