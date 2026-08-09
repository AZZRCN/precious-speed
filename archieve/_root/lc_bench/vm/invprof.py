#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""-DINVPROF 编译并跑用例, 拿 Newton 倒数逐层 self_ms 分布 + 倒数占总时间比。

回答的问题: muBlockCost 里的 A≈6.8 (倒数 / 顶层变换) 是否仍然成立?
            headline 用例里倒数到底吃掉多少?

用法: python invprof.py <src> <case>[,<case>...]
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = sys.argv[1] if len(sys.argv) > 1 else "div_D39"
CASES = (sys.argv[2] if len(sys.argv) > 2 else "length_ratio_integer_00").split(",")

REMOTE = '''#!/usr/bin/env python3
import subprocess, os, time
R='/home/azzr/divbench'; IN='/home/azzr/lcp/big_integer/division_of_big_integers/in/'
SRC=%r; CASES=%r
os.chdir(R); os.makedirs('bin', exist_ok=True)
tag='ip_'+SRC
cmd='g++ -O2 -std=c++23 -march=x86-64-v3 -DINVPROF -o bin/%%s src/%%s.cpp'%%(tag,SRC)
r=subprocess.run(cmd,shell=True,capture_output=True,text=True)
if r.returncode!=0:
    print('COMPILE FAIL'); print(r.stderr[-3000:]); raise SystemExit
for case in CASES:
    # 先量总时间 (同一个 -DINVPROF 二进制, 保证可比)
    best=1e9
    for _ in range(3):
        t0=time.perf_counter()
        subprocess.run('./bin/%%s < %%s%%s.in > /dev/null 2>/tmp/ip.err'%%(tag,IN,case),
                       shell=True)
        best=min(best,(time.perf_counter()-t0)*1000)
    print('##### %%s   wall(best of 3, with INVPROF overhead) = %%.2f ms'%%(case,best), flush=True)
    print(open('/tmp/ip.err').read(), flush=True)
print('DONE', flush=True)
''' % (SRC, CASES)

p = os.path.join(HERE, "_invprof.py")
with open(p, "w", encoding="utf-8", newline="\n") as f:
    f.write(REMOTE)
put(p, "/home/azzr/invprof.py")
rc, out, err = run("cd /home/azzr && python3 invprof.py 2>&1 | head -60", timeout=600)
print(out)
