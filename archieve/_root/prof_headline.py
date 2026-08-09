#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
prof_headline.py —— 对 LC headline 三点做 顶层四段(read/parse/div/fmt/write) 画像。

用法:  python prof_headline.py [版本, 默认 D47]

目的: 三个 29ms 并列点算法画像完全不同 (1次超大 / 3993次微小 / 2103次中等),
      唯一公共项是 ~4MB 十进制 IO。先量化 IO 常数地板, 再决定算法层下刀点。
"""
import os, subprocess, sys, time, re

ROOT = os.path.dirname(os.path.abspath(__file__))
VER = sys.argv[1] if len(sys.argv) > 1 else "D47"
SRC = os.path.join(ROOT, "best", "div_%s.cpp" % VER)
BD = os.path.join(ROOT, ".prof_build")
os.makedirs(BD, exist_ok=True)
EXE = os.path.join(BD, "div_%s_tp.exe" % VER)

CASES = [
    # headline 三点 (各 29ms)
    "a_max_b_random_02",
    "r_nearly_zero_01",
    "burnikel_ziegler_bound_02",
    # 对照
    "length_ratio_integer_03",
    "burnikel_ziegler_bound_00",
    "large_00",
    "small_00",
]

FLAGS = ["-O2", "-std=c++23", "-march=x86-64-v3", "-DTOPPROF"]


def sh(cmd, **kw):
    return subprocess.run(cmd, shell=isinstance(cmd, str), capture_output=True, text=True,
                          encoding="utf-8", errors="replace", **kw)


print("[build] %s -> %s" % (os.path.basename(SRC), os.path.basename(EXE)), flush=True)
r = sh(["g++"] + FLAGS + [SRC, "-o", EXE])
if r.returncode != 0:
    print("BUILD FAIL:\n" + (r.stderr or "")[-3000:])
    sys.exit(1)
print("[build] OK", flush=True)

PAT = re.compile(r"read=([\d.]+) parse=([\d.]+) div=([\d.]+) fmt=([\d.]+) write=([\d.]+)\s+total=([\d.]+)")

print("\n%-28s %8s %8s %8s %8s %8s %9s   | 占比 rd/ps/div/fmt/wr" %
      ("case", "read", "parse", "div", "fmt", "write", "TOTAL"))
print("-" * 118)
rows = []
for c in CASES:
    inp = os.path.join(ROOT, "lc_bench", "cases", "div", c + ".in")
    if not os.path.exists(inp):
        print("%-28s MISSING" % c)
        continue
    best = None
    for _ in range(3):                      # 取 3 轮最小, 消抖
        with open(inp, "rb") as fi:
            p = subprocess.run([EXE], stdin=fi, stdout=subprocess.DEVNULL,
                               stderr=subprocess.PIPE)
        m = PAT.search(p.stderr.decode("utf-8", "replace"))
        if not m:
            continue
        v = [float(x) for x in m.groups()]
        if best is None or v[5] < best[5]:
            best = v
    if best is None:
        print("%-28s NO PROFILE OUTPUT" % c)
        continue
    rd, ps, dv, fm, wr, tot = best
    io = rd + ps + fm + wr
    print("%-28s %8.2f %8.2f %8.2f %8.2f %8.2f %9.2f   | %.0f/%.0f/%.0f/%.0f/%.0f  非div=%.1f%%" %
          (c, rd, ps, dv, fm, wr, tot,
           100 * rd / tot, 100 * ps / tot, 100 * dv / tot, 100 * fm / tot, 100 * wr / tot,
           100 * io / tot))
    rows.append((c, rd, ps, dv, fm, wr, tot))

print("-" * 118)
if rows:
    hl = [r for r in rows if r[0] in ("a_max_b_random_02", "r_nearly_zero_01",
                                      "burnikel_ziegler_bound_02")]
    if len(hl) == 3:
        print("\n[headline 三点 非除法开销]")
        for c, rd, ps, dv, fm, wr, tot in hl:
            print("  %-28s IO+格式化=%.2f ms (%.1f%%)   纯除法=%.2f ms" %
                  (c, rd + ps + fm + wr, 100 * (rd + ps + fm + wr) / tot, dv))
        mn = min(r[1] + r[2] + r[4] + r[5] for r in hl)
        print("\n  => 三点共同的 IO/格式化地板 >= %.2f ms (本机)" % mn)
