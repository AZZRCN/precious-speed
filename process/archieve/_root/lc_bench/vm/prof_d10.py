#!/usr/bin/env python3
"""编译 D10 (PROFILE_DIV), 跑 LC 剩余三个瓶颈点, 汇总各段耗时占比。

目的: 定位 69ms 里 倒数(reciprocal) vs 块乘法(fftMulPre#2) 各占多少,
      决定下一步是攻 A(单块cyclic) / B(多块cyclic) 还是别的。
用法: 在 VM ~/divbench 下 python3 prof_d10.py
"""
import subprocess, re, sys, collections

CASES = ["length_ratio_integer_02", "r_nearly_zero_01", "a_max_b_random_02",
         "length_ratio_integer_00", "large_01"]

BUILD = ("g++ -O2 -std=c++23 -march=x86-64-v3 -DPROFILE_DIV "
         "-o bin/div_D10_prof src/div_D10.cpp")

r = subprocess.run(BUILD, shell=True, capture_output=True, text=True)
if r.returncode != 0:
    print("BUILD FAIL", r.stderr[-3000:]); sys.exit(1)
print("compiled div_D10_prof")

for case in CASES:
    cf = f"/home/azzr/lcp/big_integer/division_of_big_integers/in/{case}.in"
    subprocess.run("rm -f prof_detail.log", shell=True)
    rr = subprocess.run(f"bin/div_D10_prof < {cf} > /dev/null 2>/dev/null",
                        shell=True, capture_output=True, text=True)
    if rr.returncode != 0:
        print(case, "RUN FAIL rc=", rr.returncode); continue
    try:
        log = open("prof_detail.log", encoding="utf-8", errors="replace").read()
    except FileNotFoundError:
        print(case, "no prof_detail.log"); continue

    # 汇总: 按标签累加毫秒
    buckets = collections.OrderedDict()
    nblk = 0
    for line in log.splitlines():
        m = re.search(r"\[prof\]\s+(.*?):\s*([\d.]+)\s*ms", line)
        if not m:
            continue
        tag, ms = m.group(1).strip(), float(m.group(2))
        # 归一化 block 序号
        tag = re.sub(r"block \d+", "block", tag)
        tag = re.sub(r"\(k=\d+\)", "(k)", tag)
        tag = re.sub(r"conv_len=\d+", "conv_len", tag)
        tag = re.sub(r"fl=\d+", "fl", tag)
        tag = re.sub(r"in=\d+", "in", tag)
        tag = re.sub(r"inv_fl=\d+|inv_float_len=\d+", "inv_fl", tag)
        tag = re.sub(r"div_fl=\d+|divisor_float_len=\d+", "div_fl", tag)
        tag = re.sub(r"B\^\d+-1", "B^m-1", tag)
        tag = re.sub(r"\d+ blocks", "N blocks", tag)
        tag = re.sub(r"\d+ fftMulPre calls", "N fftMulPre calls", tag)
        if "block" in tag:
            nblk += 1
        e = buckets.setdefault(tag, [0.0, 0])
        e[0] += ms; e[1] += 1

    print(f"\n===== {case} =====")
    tot = sum(v[0] for k, v in buckets.items() if "absDivMu total" in k or "TOTAL" in k)
    rows = sorted(buckets.items(), key=lambda kv: -kv[1][0])
    for tag, (ms, n) in rows[:22]:
        print(f"  {ms:9.3f} ms  x{n:<4d}  {tag[:100]}")
