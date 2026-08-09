#!/usr/bin/env python3
# 配对比值基准: 两个二进制在同一用例上交替运行, 丢预热, 取 median-of-ratios
# 用法: python3 _pair_remote.py <binA> <binB> <case1,case2,...> [reps]
import os, sys, time, subprocess, statistics

A, B = sys.argv[1], sys.argv[2]
CASES = sys.argv[3].split(",")
REP = int(sys.argv[4]) if len(sys.argv) > 4 else 11

ROOT = "/home/azzr/divbench"
IN = "/home/azzr/lcp/big_integer/division_of_big_integers/in"


def run(exe, case):
    fin = open("%s/%s.in" % (IN, case), "rb")
    fout = open("/dev/null", "wb")
    t0 = time.perf_counter()
    subprocess.run([os.path.join(ROOT, "bin", exe)], stdin=fin, stdout=fout)
    t1 = time.perf_counter()
    fin.close(); fout.close()
    return (t1 - t0) * 1000.0


print("%-32s %9s %9s %9s %9s" % ("case", A, B, "B/A", "n"))
for c in CASES:
    # 预热 2 轮
    for _ in range(2):
        run(A, c); run(B, c)
    ta, tb, ratios = [], [], []
    for r in range(REP):
        # 轮转顺序消位置偏置
        if r % 2 == 0:
            x = run(A, c); y = run(B, c)
        else:
            y = run(B, c); x = run(A, c)
        ta.append(x); tb.append(y); ratios.append(y / x)
    print("%-32s %9.2f %9.2f %9.4f %9d" % (
        c, statistics.median(ta), statistics.median(tb),
        statistics.median(ratios), REP), flush=True)
