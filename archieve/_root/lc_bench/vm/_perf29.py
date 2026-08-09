#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""用 perf 分离「指令数」与「IPC」, 判定 D29 融合是否变成延迟瓶颈。

callgrind 只数指令, 看不见依赖链拉长。若 d29 的 instructions 更少
但 cycles 更多 / IPC 明显下降 => 串行链假设成立。

用法: python3 _perf29.py [reps] [bin1,bin2,...]
"""
import statistics
import subprocess
import sys

ROOT = "/home/azzr/divbench"
IN = "/home/azzr/lcp/big_integer/division_of_big_integers/in"
REPS = int(sys.argv[1]) if len(sys.argv) > 1 else 7
BINS = (sys.argv[2].split(",") if len(sys.argv) > 2 else ["d29b", "d29"])
CASES = ["r_nearly_zero_01", "medium_01", "burnikel_ziegler_bound_00",
         "length_ratio_integer_00"]
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


probe = one(BINS[0], CASES[0])
if "instructions" not in probe:
    print("perf unavailable / no counters:", probe)
    sys.exit(1)

print("%-28s %-6s %10s %12s %12s %6s"
      % ("case", "bin", "ms", "cycles", "instr", "IPC"))
print("-" * 80)
for c in CASES:
    acc = {b: {"ms": [], "cy": [], "ins": []} for b in BINS}
    for b in BINS:
        one(b, c)                      # 预热丢弃
    for k in range(REPS):
        order = BINS if k % 2 == 0 else BINS[::-1]
        for b in order:
            d = one(b, c)
            acc[b]["ms"].append(d.get("task-clock", 0))
            acc[b]["cy"].append(d.get("cycles", 0))
            acc[b]["ins"].append(d.get("instructions", 0))
    base = None
    for b in BINS:
        ms = statistics.median(acc[b]["ms"])
        cy = statistics.median(acc[b]["cy"])
        ins = statistics.median(acc[b]["ins"])
        ipc = ins / cy if cy else 0
        tail = ""
        if base is None:
            base = (cy, ins)
        else:
            tail = "   cy x%.4f  ins x%.4f" % (cy / base[0], ins / base[1])
        print("%-28s %-6s %10.1f %12.0f %12.0f %6.3f%s"
              % (c, b, ms, cy, ins, ipc, tail), flush=True)
    print()
