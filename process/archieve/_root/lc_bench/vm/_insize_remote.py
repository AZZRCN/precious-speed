#!/usr/bin/env python3
# 列出 LC 用例的输入规模: 文件字节数, T(测试组数), 最长 A/B 位数, 总位数
import os, sys

IN = "/home/azzr/lcp/big_integer/division_of_big_integers/in"

cases = sys.argv[1].split(",") if len(sys.argv) > 1 else sorted(
    f[:-3] for f in os.listdir(IN) if f.endswith(".in"))

print("%-32s %10s %6s %8s %8s %10s %10s" % (
    "case", "bytes", "T", "maxA", "maxB", "sumDigits", "outBytes"))
for c in cases:
    p = os.path.join(IN, c + ".in")
    if not os.path.exists(p):
        continue
    sz = os.path.getsize(p)
    with open(p) as f:
        t = int(f.readline())
        mA = mB = 0
        tot = 0
        outb = 0
        for _ in range(t):
            ln = f.readline().split()
            if len(ln) < 2:
                break
            a, b = len(ln[0]), len(ln[1])
            mA = max(mA, a); mB = max(mB, b); tot += a + b
            outb += max(1, a - b + 1) + b + 2   # 商+余数+分隔
    print("%-32s %10d %6d %8d %8d %10d %10d" % (c, sz, t, mA, mB, tot, outb))
