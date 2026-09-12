#!/usr/bin/env python3
"""D13: 采集全 26 例的 absDivMu 形状, 离线验证 nblk 代价模型, 再对比实测 paired ratio。

模型 (cyclic 情形, 已由 INVPROF 标定):
    Fi = fft_ceil_lin(2*in+1)          # 倒数 FFT, 且**每个块的第一次乘法也用它**
    Fc = max(fft_ceil_cycm(len2+1), fft_ceil_cycm((len2+in)//2+1))
    cost = A*W(Fi) + W(Fi) + W(Fc) + nblk*(2W(Fi) + 2W(Fc)),  W(N)=N*log2(N), A≈6.8
非 cyclic 情形块内第二次乘法用 Fl = fft_ceil_lin(len2+in)。
"""
import math
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run  # noqa: E402

PS = r"D:\precious_speed"
A_INV = float(sys.argv[1]) if len(sys.argv) > 1 else 6.8


# ---------- 档位阶梯 {2^k, 3*2^k} ----------
def int_ceil2(n):
    p = 1
    while p < n:
        p <<= 1
    return p


def fft_ceil(n):
    p = int_ceil2(n)
    h = p >> 2
    return h * 3 if (h * 3 >= n and h >= 1) else p


def W(n):
    return n * math.log2(n) if n > 1 else 0.0


def model_cost(len1, len2, nblk_extra, a_inv=A_INV):
    qn = len1 - len2
    nb_min = (qn - 1) // len2 + 1
    nb = nb_min + nblk_extra
    in_ = (qn - 1) // nb + 1
    if in_ > len2:
        return None
    nblk = (qn + in_ - 1) // in_
    Fi = fft_ceil(2 * in_ + 1)
    Fl = fft_ceil(len2 + in_)
    gate = max(int_ceil2(len2 + 1), int_ceil2((len2 + in_) // 2 + 1))
    cyc = (gate < in_ + len2) and (in_ >= 64)
    Fc = max(fft_ceil(len2 + 1), fft_ceil((len2 + in_) // 2 + 1)) if cyc else Fl
    pre = W(Fi) + W(Fc)
    blk = nblk * (2 * W(Fi) + 2 * W(Fc))
    return a_inv * W(Fi) + pre + blk, in_, nblk, Fi, Fc, cyc


def main():
    put(os.path.join(PS, "best", "div_D13.cpp"), "/home/azzr/divbench/src/div_D13.cpp")
    rc, out, err = run(
        "cd ~/divbench/src && g++ -O2 -std=c++23 -march=x86-64-v3 -DDIV_MUSHAPE "
        "-o ../bin/sh0 div_D13.cpp 2>&1 | tail -5 && echo BUILT", timeout=1800)
    print(out[-1500:])
    if "BUILT" not in out:
        sys.exit("build failed")

    remote = r'''
import subprocess, sys
from pathlib import Path
IN = Path('/home/azzr/lcp/big_integer/division_of_big_integers/in')
for f in sorted(IN.glob('*.in')):
    with open(f, 'rb') as fh:
        p = subprocess.run(['/home/azzr/divbench/bin/sh0'], stdin=fh,
                           capture_output=True, timeout=900)
    for l in p.stderr.decode('utf-8', 'replace').splitlines():
        if '[mushape]' in l:
            print(f.stem, l.replace('[mushape] ', ''))
'''
    tmp = os.path.join(PS, "lc_bench", "vm", "_model_remote.py")
    with open(tmp, "w", encoding="utf-8") as f:
        f.write(remote)
    put(tmp, "/tmp/model_remote.py")
    rc, out, err = run("python3 /tmp/model_remote.py", timeout=3600)
    raw = os.path.join(PS, "lc_bench", "vm", "_model_shapes.txt")
    with open(raw, "w", encoding="utf-8") as f:
        f.write(out)

    shapes = {}
    for line in out.splitlines():
        m = re.match(r"(\S+)\s+len1=(\d+)\s+len2=(\d+)", line.strip())
        if m:
            shapes.setdefault(m.group(1), []).append((int(m.group(2)), int(m.group(3))))

    # 实测 paired ratio (来自 _nb_d13.log)
    meas = {}
    log = os.path.join(PS, "lc_bench", "vm", "_nb_d13.log")
    if os.path.exists(log):
        started = False
        for line in open(log, encoding="utf-8"):
            if "paired ratio" in line:
                started = True
                continue
            if started:
                p = line.split()
                if len(p) == 5:
                    try:
                        meas[p[0]] = [1.0] + [float(x) for x in p[1:]]
                    except ValueError:
                        pass

    print("\n%-30s %-32s %-32s" % ("case", "model ratio d0..d4", "measured ratio d0..d4"))
    print("-" * 100)
    agree = tot = 0
    for case in sorted(shapes):
        costs = []
        for d in range(5):
            s = 0.0
            ok = True
            for (l1, l2) in shapes[case]:
                r = model_cost(l1, l2, d)
                if r is None:
                    ok = False
                    break
                s += r[0]
            costs.append(s if ok else None)
        if not costs[0]:
            continue
        mr = [(c / costs[0] if c else float('nan')) for c in costs]
        me = meas.get(case)
        mstr = " ".join("%5.3f" % x for x in mr)
        estr = " ".join("%5.3f" % x for x in me) if me else "-"
        mark = ""
        if me:
            tot += 1
            if mr.index(min(mr)) == me.index(min(me)):
                agree += 1
                mark = "  OK"
            else:
                mark = "  <-- model d%d / meas d%d" % (mr.index(min(mr)), me.index(min(me)))
        print("%-30s %-32s %-32s%s" % (case, mstr, estr, mark))
    print("-" * 100)
    print("argmin agreement: %d/%d   (A_INV=%.2f)" % (agree, tot, A_INV))


main()
