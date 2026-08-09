#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""prof_vm.py —— 在 Linux VM 上做 顶层五段(read/parse/div/fmt/write) 画像。

为什么必须在 VM 跑而不是本机:
  div_*.cpp 的 initInput() 在 __linux__ 下走 **mmap 零拷贝 + MADV_WILLNEED**,
  在 Windows 下退化为 fread 一次性拷 8MB 静态缓冲。两条路径的 read/parse
  成本划分完全不同 => 本机 Windows 画像不能外推到 LC(Linux)。

用法:
    python prof_vm.py up          # 上传源码 + 编译带 -DTOPPROF 的插桩版
    python prof_vm.py go          # 后台跑画像
    python prof_vm.py poll        # 查看结果

注意: 需 paramiko => 用 "C:/Program Files/Python311/python.exe" 跑。
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "lc_bench", "vm"))
from vmctl import put, run  # noqa: E402

VER = "D47"
LOG = "/home/azzr/divbench/logs/prof_%s.log" % VER
CASES = [
    "a_max_b_random_02",          # headline 29ms  T=1    a=2e6 digits
    "r_nearly_zero_01",           # headline 29ms  T=3993 a~666
    "burnikel_ziegler_bound_02",  # headline 29ms  T=2103 a~1268
    "length_ratio_integer_03",
    "burnikel_ziegler_bound_00",
    "large_00",
    "small_00",
    "max_01",
]

cmd = sys.argv[1] if len(sys.argv) > 1 else "poll"

if cmd == "up":
    put(os.path.join(HERE, "best", "div_%s.cpp" % VER),
        "/home/azzr/divbench/src/div_%s.cpp" % VER)
    rc, out, err = run(
        "mkdir -p /home/azzr/divbench/bin && cd /home/azzr/divbench && "
        "g++ -O2 -std=c++23 -march=x86-64-v3 -DTOPPROF "
        "src/div_%s.cpp -o bin/%s_tp 2>&1 | tail -20 && "
        "ls -la bin/%s_tp" % (VER, VER.lower(), VER.lower()), timeout=600)
    print(out or err)
    sys.exit(0)

if cmd == "poll":
    rc, out, err = run("cat %s 2>/dev/null; echo '--- alive:'; "
                       "pgrep -fa prof_vm | head -3" % LOG, timeout=40)
    print(out)
    sys.exit(0)

REMOTE = r'''#!/usr/bin/env python3
import subprocess, os, re
IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
CASES = %r
EXE = '/home/azzr/divbench/bin/%s_tp'
PAT = re.compile(r"read=([\d.]+) parse=([\d.]+) div=([\d.]+) fmt=([\d.]+) write=([\d.]+)\s+total=([\d.]+)")

hdr = '%%-28s %%7s %%7s %%8s %%7s %%7s %%9s   rd/ps/div/fmt/wr   nondiv%%%%' %% (
    'case','read','parse','div','fmt','write','TOTAL')
print(hdr, flush=True); print('-'*len(hdr), flush=True)
rows=[]
for c in CASES:
    f = IN + c + '.in'
    if not os.path.exists(f):
        print('%%-28s MISSING' %% c, flush=True); continue
    best=None
    for _ in range(5):                       # 5 轮取 total 最小, 消抖
        with open(f,'rb') as fi:
            p = subprocess.run([EXE], stdin=fi, stdout=subprocess.DEVNULL,
                               stderr=subprocess.PIPE)
        m = PAT.search(p.stderr.decode('utf-8','replace'))
        if not m: continue
        v=[float(x) for x in m.groups()]
        if best is None or v[5]<best[5]: best=v
    if best is None:
        print('%%-28s NOPROF' %% c, flush=True); continue
    rd,ps,dv,fm,wr,tot = best
    io = rd+ps+fm+wr
    print('%%-28s %%7.2f %%7.2f %%8.2f %%7.2f %%7.2f %%9.2f   %%.0f/%%.0f/%%.0f/%%.0f/%%.0f   %%.1f' %% (
        c,rd,ps,dv,fm,wr,tot,
        100*rd/tot,100*ps/tot,100*dv/tot,100*fm/tot,100*wr/tot,100*io/tot), flush=True)
    rows.append((c,rd,ps,dv,fm,wr,tot))
print('-'*len(hdr), flush=True)
HL=('a_max_b_random_02','r_nearly_zero_01','burnikel_ziegler_bound_02')
hl=[r for r in rows if r[0] in HL]
if len(hl)==3:
    print('', flush=True)
    print('[headline 三点 / Linux mmap 真实路径]', flush=True)
    for c,rd,ps,dv,fm,wr,tot in hl:
        print('  %%-28s 非除法=%%6.2f ms (%%4.1f%%%%)   纯除法=%%6.2f ms' %% (
            c, rd+ps+fm+wr, 100*(rd+ps+fm+wr)/tot, dv), flush=True)
    mn=min(r[1]+r[2]+r[4]+r[5] for r in hl)
    print('', flush=True)
    print('  => 三点公共 IO/格式化地板 >= %%.2f ms (VM Linux)' %% mn, flush=True)
print('DONE', flush=True)
''' % (CASES, VER.lower())

p = os.path.join(HERE, "_prof_vm_remote.py")
with open(p, "w", encoding="utf-8", newline="\n") as f:
    f.write(REMOTE)
put(p, "/home/azzr/prof_vm.py")
run("mkdir -p /home/azzr/divbench/logs && rm -f " + LOG)
run("cd /home/azzr && nohup python3 prof_vm.py > %s 2>&1 & echo started" % LOG,
    timeout=30)
print("started; poll with: python prof_vm.py poll")
