# -*- coding: utf-8 -*-
"""
perf_cal2.py —— 第二轮标定: 找能把 cycles 噪声压到最低的测量配方
  仍用 md5 相同的 A/B 二进制做哨兵 (任何配方的 A/B 比值理论必须 = 1.000)
  对比: 绑核 on/off  ×  统计量 (median-of-ratios / min-of-ratios / ratio-of-mins / trimmed)
"""
import os, sys, json

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "lc_bench", "vm"))
from vmctl import run, put  # noqa

RROOT = "/home/azzr/divbench"
INDIR = "/home/azzr/lcp/big_integer/division_of_big_integers/in"
CASES = ["r_nearly_zero_01", "burnikel_ziegler_bound_02", "a_max_b_random_02"]

REMOTE = r'''#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import subprocess, os, statistics, json
IN = "%(indir)s"; ROOT = "%(root)s"; CASES = %(cases)s
REPS = 25

def one(tag, case, pin):
    exe = "%%s/bin/perfcal_%%s" %% (ROOT, tag)
    cmd = (["taskset", "-c", "2"] if pin else []) + \
          ["perf", "stat", "-x,", "-e", "cycles,instructions", exe]
    with open("%%s/%%s.in" %% (IN, case), "rb") as fi:
        p = subprocess.run(cmd, stdin=fi, stdout=subprocess.DEVNULL,
                           stderr=subprocess.PIPE)
    d = {}
    for line in p.stderr.decode("utf-8", "replace").splitlines():
        f = line.split(",")
        if len(f) >= 3:
            try: d[f[2].strip()] = float(f[0])
            except ValueError: pass
    return d.get("cycles", 0.0), d.get("instructions", 0.0)

def trimmed(xs, frac=0.4):
    xs = sorted(xs); k = int(len(xs) * frac)
    xs = xs[:len(xs) - k] if k else xs
    return statistics.mean(xs)

def report(a, b):
    """a,b 为配对样本列表, 返回 4 种统计量给出的 B/A-1 (%%)"""
    ratios = [y / x for x, y in zip(a, b) if x]
    out = {}
    out["median-of-ratios"] = statistics.median(ratios)
    out["min-of-ratios"] = min(ratios)
    out["ratio-of-mins"] = min(b) / min(a)
    out["trimmed40%%"] = trimmed(b) / trimmed(a)
    return {k: (v - 1) * 100 for k, v in out.items()}

print("== 配方标定: A/B 为 md5 相同的二进制, 所有数字理论必须 = 0.000 ==", flush=True)
print("   REPS=%%d, 交替 A/B" %% REPS, flush=True)
print(flush=True)
STATS = ["median-of-ratios", "min-of-ratios", "ratio-of-mins", "trimmed40%%"]

for pin in (False, True):
    print("### %%s" %% ("taskset -c 2 (绑核)" if pin else "不绑核"), flush=True)
    print("    %%-26s %%-8s" %% ("case", "metric") +
          "".join("%%18s" %% s for s in STATS), flush=True)
    for case in CASES:
        one("A", case, pin); one("B", case, pin)     # 预热
        ca, cb, ia, ib = [], [], [], []
        for r in range(REPS):
            for tag in (("A", "B") if r %% 2 == 0 else ("B", "A")):
                c, i = one(tag, case, pin)
                (ca if tag == "A" else cb).append(c)
                (ia if tag == "A" else ib).append(i)
        for lbl, xa, xb in (("cycles", ca, cb), ("insns", ia, ib)):
            rp = report(xa, xb)
            print("    %%-26s %%-8s" %% (case, lbl) +
                  "".join("%%+18.3f" %% rp[s] for s in STATS), flush=True)
    print(flush=True)
print("== DONE ==", flush=True)
'''


def go():
    body = REMOTE % dict(indir=INDIR, root=RROOT, cases=json.dumps(CASES))
    lp = os.path.join(HERE, ".jobs", "_perfcal2_remote.py")
    os.makedirs(os.path.dirname(lp), exist_ok=True)
    with open(lp, "w", encoding="utf-8", newline="\n") as f:
        f.write(body)
    put(lp, "%s/perfcal2_remote.py" % RROOT)
    run("cd %s && nohup python3 perfcal2_remote.py > logs/perfcal2.log 2>&1 & echo started"
        % RROOT, timeout=60)
    print("[go] started")


def poll():
    rc, out, err = run("cat %s/logs/perfcal2.log 2>/dev/null; echo '--- alive:'; "
                       "pgrep -fa perfcal2_remote | head -3" % RROOT, timeout=60)
    print(out)


if __name__ == "__main__":
    {"go": go, "poll": poll}[sys.argv[1]]()
