#!/usr/bin/env python3
# 内核侧盲区探针: 缺页数 / peak RSS / user vs sys 时间 (callgrind 完全看不见的部分)
import os, sys, subprocess, re, statistics

BIN = sys.argv[1]
CASES = sys.argv[2].split(",")
REP = int(sys.argv[3]) if len(sys.argv) > 3 else 5

ROOT = "/home/azzr/divbench"
IN = "/home/azzr/lcp/big_integer/division_of_big_integers/in"
EXE = os.path.join(ROOT, "bin", BIN)

print("%-32s %10s %10s %10s %10s %10s %10s" % (
    "case", "minorPF", "majorPF", "peakRSS_K", "user_ms", "sys_ms", "wall_ms"))
for c in CASES:
    mins, uss, sys_, walls, rss = [], [], [], [], []
    for r in range(REP + 1):
        p = subprocess.run(
            "/usr/bin/time -v %s < %s/%s.in > /dev/null" % (EXE, IN, c),
            shell=True, capture_output=True, text=True)
        t = p.stderr
        def g(pat, cast=float):
            m = re.search(pat, t)
            return cast(m.group(1)) if m else 0
        if r == 0:
            continue   # warmup
        mins.append(g(r"Minor \(reclaiming a frame\) page faults: (\d+)", int))
        majp = g(r"Major \(requiring I/O\) page faults: (\d+)", int)
        rss.append(g(r"Maximum resident set size \(kbytes\): (\d+)", int))
        uss.append(g(r"User time \(seconds\): ([\d.]+)") * 1000)
        sys_.append(g(r"System time \(seconds\): ([\d.]+)") * 1000)
        m = re.search(r"wall clock\).*?(\d+):([\d.]+)", t)
        walls.append((int(m.group(1)) * 60 + float(m.group(2))) * 1000 if m else 0)
    md = statistics.median
    print("%-32s %10d %10d %10d %10.1f %10.1f %10.1f" % (
        c, md(mins), majp, md(rss), md(uss), md(sys_), md(walls)))
