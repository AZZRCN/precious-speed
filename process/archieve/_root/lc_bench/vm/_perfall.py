#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""全用例 perf 基准表 (task-clock / cycles / instructions / IPC)。

发现: perf task-clock 比墙钟干净得多, 且与 LC 判题机耗时几乎 1:1
      (rnz_01 本地 35.7ms vs LC 36ms)。以后本地就用它当基准。

用法: python3 _perfall.py <bin1,bin2,...> [reps]
输出: 每用例每二进制的中位 task-clock, 以及相对第一个二进制的 cy 比值;
      末尾给出各二进制的 headline (= 最慢用例) —— LC 就是按这个计分。
"""
import os
import statistics
import subprocess
import sys

ROOT = "/home/azzr/divbench"
IN = "/home/azzr/lcp/big_integer/division_of_big_integers/in"
BINS = sys.argv[1].split(",") if len(sys.argv) > 1 else ["d25"]
REPS = int(sys.argv[2]) if len(sys.argv) > 2 else 5
CASES = sorted(f[:-3] for f in os.listdir(IN) if f.endswith(".in"))
EV = "task-clock,cycles,instructions"


def one(exe, case):
    p = subprocess.run(
        "perf stat -x, -e %s %s/bin/%s < %s/%s.in > /dev/null"
        % (EV, ROOT, exe, IN, case),
        shell=True, capture_output=True, text=True)
    d = {}
    for line in p.stderr.splitlines():
        f = line.split(",")
        if len(f) >= 3 and f[0] not in ("", "<not counted>"):
            try:
                d[f[2]] = float(f[0])
            except ValueError:
                pass
    return d


hdr = "%-30s" % "case"
for b in BINS:
    hdr += " %9s" % (b + "_ms")
for b in BINS[1:]:
    hdr += " %8s" % ("cy/" + BINS[0])
print(hdr)
print("-" * len(hdr))

peak = {b: (0.0, "") for b in BINS}
tot = {b: 0.0 for b in BINS}
for c in CASES:
    acc = {b: {"ms": [], "cy": []} for b in BINS}
    for b in BINS:
        one(b, c)                              # 预热丢弃
    for k in range(REPS):
        for b in (BINS if k % 2 == 0 else BINS[::-1]):
            d = one(b, c)
            acc[b]["ms"].append(d.get("task-clock", 0))
            acc[b]["cy"].append(d.get("cycles", 0))
    row = "%-30s" % c
    for b in BINS:
        ms = statistics.median(acc[b]["ms"])
        tot[b] += ms
        if ms > peak[b][0]:
            peak[b] = (ms, c)
        row += " %9.1f" % ms
    base = statistics.median(acc[BINS[0]]["cy"])
    for b in BINS[1:]:
        row += " %8.4f" % (statistics.median(acc[b]["cy"]) / base)
    print(row, flush=True)

print("-" * len(hdr))
for b in BINS:
    print("%-6s headline(最慢用例) = %6.1f ms  @ %-28s   sum=%.0f ms"
          % (b, peak[b][0], peak[b][1], tot[b]))
