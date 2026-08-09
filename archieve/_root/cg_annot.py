#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""cg_annot.py —— 对 LC headline 三点做 **函数级** callgrind 归因。

为什么要它:
  顶层五段画像已证明 Linux 下 94% 时间在 div 内部 (IO 仅 5.5-6.4%),
  而三点的算法路径分布完全不同:
      r_nearly_zero_01           schoolbook 66% 工作量  (len2 中位 73)
      burnikel_ziegler_bound_02  mu 53% + newton 46%    (len2 中位 159)
      a_max_b_random_02          mu 100% (单次 500000/206025 limbs)
  再往下必须看**函数级 Ir 占比**才能精确制导, 不能靠猜。

用法:
    python cg_annot.py up     # 上传 + 编译带 -g 的 D47
    python cg_annot.py go     # 后台跑 callgrind + callgrind_annotate
    python cg_annot.py poll   # 看结果

注意: 需 paramiko => 用 "C:/Program Files/Python311/python.exe" 跑。
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "lc_bench", "vm"))
from vmctl import put, run  # noqa: E402

VER = "D47"
LOG = "/home/azzr/divbench/logs/annot_%s.log" % VER
CASES = ["r_nearly_zero_01", "burnikel_ziegler_bound_02", "a_max_b_random_02"]

cmd = sys.argv[1] if len(sys.argv) > 1 else "poll"

if cmd == "up":
    put(os.path.join(HERE, "best", "div_%s.cpp" % VER),
        "/home/azzr/divbench/src/div_%s.cpp" % VER)
    # -g 保留符号; -fno-omit-frame-pointer 让调用图可读;
    # 不加 -fno-inline: 那会改变被测代码, 归因就不是真实二进制了。
    rc, out, err = run(
        "mkdir -p /home/azzr/divbench/bin && cd /home/azzr/divbench && "
        "g++ -O2 -g -fno-omit-frame-pointer -std=c++23 -march=x86-64-v3 "
        "src/div_%s.cpp -o bin/%s_g 2>&1 | tail -20 && ls -la bin/%s_g"
        % (VER, VER.lower(), VER.lower()), timeout=600)
    print(out or err)
    sys.exit(0)

if cmd == "poll":
    rc, out, err = run("cat %s 2>/dev/null; echo '--- alive:'; "
                       "pgrep -fa cg_annot | head -3" % LOG, timeout=60)
    print(out)
    sys.exit(0)

REMOTE = r'''#!/usr/bin/env python3
import subprocess, os, re
IN  = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
EXE = '/home/azzr/divbench/bin/%s_g'
CASES = %r
TOPN = 16

for c in CASES:
    f = IN + c + '.in'
    if not os.path.exists(f):
        print('MISSING', c, flush=True); continue
    outf = '/tmp/annot_%%s.out' %% c
    cmd = ('valgrind --tool=callgrind --callgrind-out-file=' + outf +
           ' ' + EXE + ' < ' + f + ' > /dev/null 2>/dev/null')
    subprocess.run(cmd, shell=True)
    r = subprocess.run(['callgrind_annotate', '--auto=no', '--threshold=99', outf],
                       capture_output=True, text=True)
    txt = r.stdout
    # 解析总 Ir
    tot = None
    m = re.search(r'([\d,]+)\s+PROGRAM TOTALS', txt)
    if m: tot = int(m.group(1).replace(',',''))
    print('', flush=True)
    print('='*100, flush=True)
    print('### %%s   总 Ir = %%s' %% (c, '{:,}'.format(tot) if tot else '?'), flush=True)
    print('='*100, flush=True)
    started = False; n = 0
    for line in txt.splitlines():
        if re.match(r'^-+$', line.strip()) and not started:
            continue
        if 'file:function' in line or ('Ir' in line and 'file' in line):
            started = True; continue
        if not started: continue
        mm = re.match(r'^\s*([\d,]+)\s+(?:\(\s*[\d.]+%%\)\s+)?(.+)$', line)
        if not mm: continue
        ir = int(mm.group(1).replace(',',''))
        nm = mm.group(2).strip()
        if 'PROGRAM TOTALS' in nm: continue
        n += 1
        if n > TOPN: break
        pct = 100.0*ir/tot if tot else 0.0
        # 缩短符号名
        nm = re.sub(r'\(.*?\)', '()', nm)
        if len(nm) > 78: nm = nm[:75]+'...'
        print('  %%7.3f%%%%  %%14s  %%s' %% (pct, '{:,}'.format(ir), nm), flush=True)
    os.unlink(outf)
print('', flush=True)
print('DONE', flush=True)
''' % (VER.lower(), CASES)

p = os.path.join(HERE, "_cg_annot_remote.py")
with open(p, "w", encoding="utf-8", newline="\n") as f:
    f.write(REMOTE)
put(p, "/home/azzr/cg_annot.py")
run("mkdir -p /home/azzr/divbench/logs && rm -f " + LOG)
run("cd /home/azzr && nohup python3 cg_annot.py > %s 2>&1 & echo started" % LOG,
    timeout=30)
print("started; poll with: python cg_annot.py poll")
