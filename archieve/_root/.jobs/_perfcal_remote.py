#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import subprocess, os, re, statistics, json
IN = "/home/azzr/lcp/big_integer/division_of_big_integers/in"; ROOT = "/home/azzr/divbench"
CASES = ["r_nearly_zero_01", "burnikel_ziegler_bound_02", "a_max_b_random_02", "length_ratio_integer_03"]
REPS = 9

EV = "cycles,instructions,branch-misses,cache-misses"

def one(tag, case):
    """跑一次, 返回 dict(cycles, insns, bmiss, cmiss, task_ms, wall_ms)"""
    exe = "%s/bin/perfcal_%s" % (ROOT, tag)
    inf = "%s/%s.in" % (IN, case)
    cmd = ["perf", "stat", "-x,", "-e", EV, "-e", "task-clock",
           exe]
    with open(inf, "rb") as fi:
        p = subprocess.run(cmd, stdin=fi, stdout=subprocess.DEVNULL,
                           stderr=subprocess.PIPE)
    d = {}
    for line in p.stderr.decode("utf-8", "replace").splitlines():
        f = line.split(",")
        if len(f) < 3: continue
        try: val = float(f[0])
        except ValueError: continue
        ev = f[2].strip()
        d[ev] = val
    return d

def cv(xs):
    if len(xs) < 2 or statistics.mean(xs) == 0: return 0.0
    return 100.0 * statistics.pstdev(xs) / statistics.mean(xs)

print("== perf 噪声标定: A/B 为同源同 flag 编译的两个二进制, 理论比值恒 = 1.000 ==", flush=True)
print("   REPS=%d  交替 A/B 消偏  事件=%s + task-clock" % (REPS, EV), flush=True)
print(flush=True)

KEYS = [("cycles","cycles"), ("instructions","insns"),
        ("task-clock","task_ms"), ("branch-misses","bmiss"),
        ("cache-misses","cmiss")]

for case in CASES:
    # 预热
    one("A", case); one("B", case)
    acc = {"A": {}, "B": {}}
    for r in range(REPS):
        for tag in (("A","B") if r % 2 == 0 else ("B","A")):
            d = one(tag, case)
            for k, _ in KEYS:
                acc[tag].setdefault(k, []).append(d.get(k, 0.0))
    print("--- %s" % case, flush=True)
    print("    %-14s %14s %14s %9s %9s %9s" %
          ("event","A(median)","B(median)","B/A-1%","CV_A%","CV_B%"), flush=True)
    for k, lbl in KEYS:
        a = acc["A"][k]; b = acc["B"][k]
        if not any(a): continue
        ma = statistics.median(a); mb = statistics.median(b)
        # 配对比值 median-of-ratios
        ratios = [y/x for x, y in zip(a, b) if x]
        mr = statistics.median(ratios) if ratios else 0
        print("    %-14s %14.0f %14.0f %+9.3f %9.3f %9.3f" %
              (lbl, ma, mb, (mr-1)*100, cv(a), cv(b)), flush=True)
    print(flush=True)
print("== DONE ==", flush=True)
