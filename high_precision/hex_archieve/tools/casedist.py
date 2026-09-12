#!/usr/bin/env python3
# AZZRCN
# https://github.com/AZZRCN
"""casedist.py — 分析每个测试点的 FFT 长度档位分布

回答的问题: LC headline 点 (最慢那个) 的乘法, FFT 长度落在哪个档?
  * 贴着 2^k 上沿  -> 混合基 0 收益 (还倒赔常数)
  * 在 2^k 下沿    -> 混合基砍 25~37.5%, 巨大胜利

用法: python3 casedist.py <datadir> [case ...]
输出: 每个 case 按 (path, lm) 聚合, 给出组数 / 总系数量 / 浪费率 c/lm。
      浪费率越低越好 (=1.0 表示长度零浪费)。
"""
import os
import sys
from collections import defaultdict

GK = [(19 - i) << (2 * i + 4) for i in range(10)] + [1 << 62]


def pick_k(u):
    i = 0
    while u > GK[i]:
        i += 1
    return 19 - i


def next_pow2(c):
    return c if (c & (c - 1)) == 0 else 1 << c.bit_length()


def fft_ceil_tiers(c):
    p = next_pow2(c)
    best = p
    if c != p and p >= 1024:
        h = (p >> 2) * 3
        if c <= h < best:
            best = h
        h5 = (p >> 3) * 5
        if c <= h5 < best:
            best = h5
    return best


def analyze(path):
    lines = open(path, "rb").read().split(b"\n")
    t = int(lines[0])
    agg = defaultdict(lambda: [0, 0, 0.0])   # (path,lm,p2) -> [groups, coeffs, waste_sum]
    bf = 0
    for i in range(1, t + 1):
        a, b = lines[i].split()
        na, nb = (len(a) + 15) // 16, (len(b) + 15) // 16
        if min(na, nb) <= 48:
            bf += 1
            continue
        u = na + nb
        k = pick_k(u)
        c = (u * 64 + k - 1) // k
        lm = fft_ceil_tiers(c)
        p2 = next_pow2(c)
        pa = 3 if lm % 3 == 0 else (5 if lm % 5 == 0 else 2)
        e = agg[(pa, lm, p2)]
        e[0] += 1
        e[1] += c
        e[2] += lm / c
    return t, bf, agg


def main():
    d = sys.argv[1]
    sel = sys.argv[2:]
    files = sorted(f for f in os.listdir(d) if f.endswith(".in"))
    if sel:
        files = [f for f in files if f[:-3] in sel]
    for f in files:
        t, bf, agg = analyze(os.path.join(d, f))
        name = f[:-3]
        tot_c = sum(v[1] for v in agg.values())
        print(f"\n### {name}   T={t}  bf(schoolbook)={bf}  fft_groups={sum(v[0] for v in agg.values())}"
              f"  sum_coeffs={tot_c}")
        if not agg:
            continue
        print("   %-6s %10s %10s %8s %12s %8s %7s" %
              ("path", "lm", "next_p2", "groups", "sum_c", "lm/c", "share"))
        for (pa, lm, p2), (g, cs, ws) in sorted(agg.items(), key=lambda x: -x[1][1]):
            print("   %-6d %10d %10d %8d %12d %8.4f %6.1f%%" %
                  (pa, lm, p2, g, cs, ws / g, 100.0 * cs / tot_c))
        # 若全走 2 幂, 报告"若开混合档能省多少"
        pot = 0.0
        for (pa, lm, p2), (g, cs, ws) in agg.items():
            if pa == 2 and lm == p2:
                pot += cs * 0.0
        mix_share = 100.0 * sum(v[1] for (p_, _l, _p) , v in agg.items() if p_ != 2) / tot_c
        print(f"   -> 混合档覆盖 {mix_share:.1f}% 的系数量")


if __name__ == "__main__":
    main()
