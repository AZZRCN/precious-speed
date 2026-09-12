#!/usr/bin/env python3
# T111: 多形态用例下 radix-8 vs split-radix 的 instructions:u 对比
# LC 取单点 max, 故必须找到最慢形态; B 极短时 Newton 迭代最长, FFT 占比最高。
import os, random, subprocess, sys

LOG16 = 1600000  # LC HEX div: A 最长 1.6e6 hex digits
random.seed(0xBEEF)

def rand_hex(nd):
    if nd <= 0: nd = 1
    v = random.getrandbits(nd * 4)
    s = format(v, 'X')
    if len(s) < nd:
        s = 'F' + s[1:] if s else 'F'
    return s[:nd] if len(s) >= nd else s

SHAPES = [
    ("B=1",        LOG16, 1),
    ("B=A/1000",   LOG16, LOG16 // 1000),
    ("B=A/100",    LOG16, LOG16 // 100),
    ("B=A/4",      LOG16, LOG16 // 4),
    ("B=A/2",      LOG16, LOG16 // 2),
    ("B=A-1",      LOG16, LOG16 - 1),
]

BINS = [("R8=0", "/tmp/dv_0_32"), ("R8=1", "/tmp/dv_1_32")]

def instr(binp, path):
    p = subprocess.run(['perf', 'stat', '-e', 'instructions:u', '-x,', binp],
                       stdin=open(path), capture_output=True, text=True)
    for line in p.stderr.splitlines():
        if 'instructions:u' in line:
            try:
                return int(line.split(',')[0])
            except ValueError:
                pass
    return None

print("%-12s %14s %14s %9s" % ("shape", "R8=0", "R8=1", "delta%"))
for name, na, nb in SHAPES:
    A = rand_hex(na)
    B = rand_hex(nb)
    path = '/tmp/shape.txt'
    with open(path, 'w') as f:
        f.write("1\n%s %s\n" % (A, B))
    vals = []
    for tag, b in BINS:
        # 取 3 次最小值, 排除偶发抖动 (指令数本身稳定, 保险)
        r = min(x for x in (instr(b, path) for _ in range(3)) if x)
        vals.append(r)
    d = (vals[1] - vals[0]) / vals[0] * 100.0
    print("%-12s %14d %14d %+8.3f%%" % (name, vals[0], vals[1], d))
