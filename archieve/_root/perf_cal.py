# -*- coding: utf-8 -*-
"""
perf_cal.py —— 标定 VM 上三种测量手段的噪声地板
  A/B 是【同一份源码编译出的两个二进制】(仅文件名不同) => 任何指标的 A/B 比值理论必须 = 1.000
  实测偏离 1.000 的幅度 = 该指标的噪声地板

用法:
  python perf_cal.py up     # 上传+编译
  python perf_cal.py go     # 后台跑标定
  python perf_cal.py poll   # 看结果
"""
import os, sys, json

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "lc_bench", "vm"))
from vmctl import run, put  # noqa

RROOT = "/home/azzr/divbench"
INDIR = "/home/azzr/lcp/big_integer/division_of_big_integers/in"
CASES = ["r_nearly_zero_01", "burnikel_ziegler_bound_02",
         "a_max_b_random_02", "length_ratio_integer_03"]
FLAGS = "-O2 -std=c++23 -march=native -funroll-loops"


def cmd_up():
    put(os.path.join(HERE, "best", "div_D47.cpp"), "%s/src/perfcal.cpp" % RROOT)
    cmds = ["mkdir -p %s/bin %s/logs && cd %s" % (RROOT, RROOT, RROOT)]
    for tag in ("A", "B"):
        cmds.append("g++ %s src/perfcal.cpp -o bin/perfcal_%s 2>&1 | tail -5 "
                    "&& echo 'OK %s'" % (FLAGS, tag, tag))
    cmds.append("md5sum bin/perfcal_A bin/perfcal_B")
    rc, out, err = run(" && ".join(cmds), timeout=1800)
    print(out or err)


REMOTE = r'''#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import subprocess, os, re, statistics, json
IN = "%(indir)s"; ROOT = "%(root)s"
CASES = %(cases)s
REPS = 9

EV = "cycles,instructions,branch-misses,cache-misses"

def one(tag, case):
    """跑一次, 返回 dict(cycles, insns, bmiss, cmiss, task_ms, wall_ms)"""
    exe = "%%s/bin/perfcal_%%s" %% (ROOT, tag)
    inf = "%%s/%%s.in" %% (IN, case)
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
print("   REPS=%%d  交替 A/B 消偏  事件=%%s + task-clock" %% (REPS, EV), flush=True)
print(flush=True)

KEYS = [("cycles","cycles"), ("instructions","insns"),
        ("task-clock","task_ms"), ("branch-misses","bmiss"),
        ("cache-misses","cmiss")]

for case in CASES:
    # 预热
    one("A", case); one("B", case)
    acc = {"A": {}, "B": {}}
    for r in range(REPS):
        for tag in (("A","B") if r %% 2 == 0 else ("B","A")):
            d = one(tag, case)
            for k, _ in KEYS:
                acc[tag].setdefault(k, []).append(d.get(k, 0.0))
    print("--- %%s" %% case, flush=True)
    print("    %%-14s %%14s %%14s %%9s %%9s %%9s" %%
          ("event","A(median)","B(median)","B/A-1%%","CV_A%%","CV_B%%"), flush=True)
    for k, lbl in KEYS:
        a = acc["A"][k]; b = acc["B"][k]
        if not any(a): continue
        ma = statistics.median(a); mb = statistics.median(b)
        # 配对比值 median-of-ratios
        ratios = [y/x for x, y in zip(a, b) if x]
        mr = statistics.median(ratios) if ratios else 0
        print("    %%-14s %%14.0f %%14.0f %%+9.3f %%9.3f %%9.3f" %%
              (lbl, ma, mb, (mr-1)*100, cv(a), cv(b)), flush=True)
    print(flush=True)
print("== DONE ==", flush=True)
'''


def cmd_go():
    body = REMOTE % dict(indir=INDIR, root=RROOT, cases=json.dumps(CASES))
    lp = os.path.join(HERE, ".jobs", "_perfcal_remote.py")
    os.makedirs(os.path.dirname(lp), exist_ok=True)
    with open(lp, "w", encoding="utf-8", newline="\n") as f:
        f.write(body)
    put(lp, "%s/perfcal_remote.py" % RROOT)
    run("cd %s && nohup python3 perfcal_remote.py > logs/perfcal.log 2>&1 & echo started"
        % RROOT, timeout=60)
    print("[go] started, use: python perf_cal.py poll")


def cmd_poll():
    rc, out, err = run("cat %s/logs/perfcal.log 2>/dev/null; echo '--- alive:'; "
                       "pgrep -fa perfcal_remote | head -3" % RROOT, timeout=60)
    print(out)


if __name__ == "__main__":
    {"up": cmd_up, "go": cmd_go, "poll": cmd_poll}[sys.argv[1]]()
