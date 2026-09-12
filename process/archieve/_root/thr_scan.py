#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""thr_scan.py —— 扫描 schoolbook/Newton 分派阈值, 标定「Newton 在中等尺寸是否亏」。

背景 (2026-08-07 发现):
  D47 的 absDivRem 分派只有两层半:
      len2<=64 或 qn<=64        -> absDivBasicCore  (schoolbook)
      len1 < 2*len2             -> absDivNewtonCore1(Newton 倒数 + FFT)
      否则                      -> absDivMu         (Barrett/mu)
  **缺了 GMP 标准三层里的中间层 Burnikel-Ziegler 分治 (dc_div_qr)。**
  而 LC headline 三点里的两点恰好卡在 len1 = 2*len2 - 1:
      r_nearly_zero_01          len1=167 len2=84   (2*len2=168, 差 1)
      burnikel_ziegler_bound_02 len1=317 len2=159  (2*len2=318, 差 1)
  两点都因此掉进 Newton+FFT。用例名本身就说明出题人故意卡这个边界。

本脚本把 64 参数化为 -DDIV_BASIC_LIMIT, 扫一遍看 schoolbook 能顶到多大。
  - schoolbook 反而更快 => Newton 在该区间净亏 => 补 BZ 分治层收益巨大
  - schoolbook 更慢     => 尺寸已够大, BZ 收益取决于介于两者之间的位置
默认值 64 时行为与 D47 逐字节等价 (只有显式 -D 才改变)。

用法: python thr_scan.py [版本, 默认 D47]
"""
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
VER = sys.argv[1] if len(sys.argv) > 1 else "D47"
SRC = os.path.join(ROOT, "best", "div_%s.cpp" % VER)
BD = os.path.join(ROOT, ".thr_build")
os.makedirs(BD, exist_ok=True)

# 被替换的原句 (必须唯一)
OLD = "if (len2 <= 64 || (len1 - len2) <= 64)"
NEW = "if (len2 <= DIV_BASIC_LIMIT || (len1 - len2) <= DIV_BASIC_LIMIT)"
GUARD = ("#ifndef DIV_BASIC_LIMIT\n"
         "#define DIV_BASIC_LIMIT 64\n"
         "#endif\n")

CASES = [
    "r_nearly_zero_01",            # len1=167 len2=84   <- 卡边界
    "burnikel_ziegler_bound_02",   # len1=317 len2=159  <- 卡边界
    "burnikel_ziegler_bound_00",
    "length_ratio_integer_03",
    "large_00",
    "medium_01",
]
LIMITS = [64, 96, 128, 168, 200, 256, 320, 400]
PAT = re.compile(r"div=([\d.]+)")


def gen_param_src():
    raw = open(SRC, "r", encoding="utf-8", newline="").read()
    n = raw.count(OLD)
    if n != 1:
        print("FATAL: 分派句出现 %d 次 (需恰好 1 次): %r" % (n, OLD))
        sys.exit(1)
    body = raw.replace(OLD, NEW)
    # 把 guard 插到第一个 #include 之前
    i = body.find("#include")
    if i < 0:
        print("FATAL: 找不到 #include")
        sys.exit(1)
    crlf = "\r\n" in raw
    g = GUARD.replace("\n", "\r\n") if crlf else GUARD
    body = body[:i] + g + body[i:]
    p = os.path.join(BD, "div_param.cpp")
    open(p, "w", encoding="utf-8", newline="").write(body)
    return p


PSRC = gen_param_src()
print("[gen] %s  (DIV_BASIC_LIMIT 参数化, 默认 64 == D47 原行为)" % PSRC, flush=True)

exes = {}
for L in LIMITS:
    exe = os.path.join(BD, "d_%d.exe" % L)
    r = subprocess.run(["g++", "-O2", "-std=c++23", "-march=x86-64-v3",
                        "-DTOPPROF", "-DDIV_BASIC_LIMIT=%d" % L, PSRC, "-o", exe],
                       capture_output=True, text=True, encoding="utf-8", errors="replace")
    if r.returncode != 0:
        print("BUILD FAIL L=%d:\n%s" % (L, (r.stderr or "")[-1500:]))
        sys.exit(1)
    exes[L] = exe
    print("[build] DIV_BASIC_LIMIT=%d OK" % L, flush=True)

print()
hdr = "%-28s" % "case" + "".join("%9s" % ("L=%d" % L) for L in LIMITS) + "   最优"
print(hdr)
print("-" * len(hdr))

for c in CASES:
    inp = os.path.join(ROOT, "lc_bench", "cases", "div", c + ".in")
    if not os.path.exists(inp):
        print("%-28s MISSING" % c)
        continue
    res = {}
    for L in LIMITS:
        best = None
        for _ in range(3):
            with open(inp, "rb") as fi:
                p = subprocess.run([exes[L]], stdin=fi, stdout=subprocess.DEVNULL,
                                   stderr=subprocess.PIPE)
            m = PAT.search(p.stderr.decode("utf-8", "replace"))
            if m:
                v = float(m.group(1))
                if best is None or v < best:
                    best = v
        res[L] = best
    base = res.get(64)
    line = "%-28s" % c
    for L in LIMITS:
        v = res[L]
        line += "%9.2f" % v if v is not None else "%9s" % "-"
    ok = {L: v for L, v in res.items() if v is not None}
    if ok and base:
        bl = min(ok, key=lambda k: ok[k])
        line += "   L=%-4d %+.1f%%" % (bl, (ok[bl] / base - 1) * 100)
    print(line, flush=True)

print("-" * len(hdr))
print("(数字 = div 段耗时 ms, 3 轮取最小; L=64 即 D47 现状)")
