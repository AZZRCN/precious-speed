#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""环形卷积选型: 对每个 bz 块长 n, 找最小合法 FFT 档 L 使
     k in [8,19], 64 | L*k, mc = L*k/64 >= n+2,
     精度: d*2^(2k) <= 2^BUD  其中 d = ceil(64n/k) = 单操作数非零 digit 数
           (环形卷积输出系数 = 至多 d 项 digit 积之和; 与线性路径 (lm/2)*2^(2k) <= 2^47 同安全级)
   对比现行 pow2 线性长度 / tier 线性长度。
"""
import sys

GK = [19 << 4, 18 << 6, 17 << 8, 16 << 10, 15 << 12, 14 << 14, 13 << 16,
      12 << 18, 11 << 20, 10 << 22, 1 << 62]


def pick_k(u):
    i = 0
    while u > GK[i]:
        i += 1
    return 19 - i


def next_pow2(c):
    return c if (c & (c - 1)) == 0 else (1 << c.bit_length())


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


def tiers(maxj=25):
    out = set()
    for j in range(5, maxj):
        out.add(1 << j)
        if (1 << j) % 4 == 0:
            out.add((1 << j) // 4 * 3)
        if (1 << j) % 8 == 0:
            out.add((1 << j) // 8 * 5)
    return sorted(x for x in out if x >= 32)


TIERS = tiers()


def pick_cyc(n, bud):
    nmin = n + 2
    for L in TIERS:
        for k in range(19, 7, -1):
            if (L * k) % 64:
                continue
            mc = L * k // 64
            if mc < nmin:
                continue
            d = (64 * n + k - 1) // k
            if d * (1 << (2 * k)) > (1 << bud):
                continue
            return (L, k, mc, d * (1 << (2 * k)))
    return None


def lm_pow2_for(u, k):
    coeffs = (u * 64 // k) + 1
    return 2 << (coeffs.bit_length() - 1)


CASES = [
    ('amax_2', 25088), ('length_ratio_1', 25088), ('length_ratio_2', 16896),
    ('length_ratio_3', 9216), ('length_ratio_4', 4864), ('length_ratio_5', 1984),
    ('large_0', 2240), ('large_1', 1760), ('rnear_2', 2176),
    ('medium_0', 296), ('medium_1', 280), ('medium_2', 272), ('small_0', 288),
]

for bud in (48, 47):
    print(f"===== budget 2^{bud} =====")
    print(f"{'group':<16}{'n':>7} {'k':>3} {'lm_pow2':>9} {'lm_tier':>9} | "
          f"{'L_cyc':>8} {'kc':>3} {'mc':>7} {'coef/2^48':>10} {'blk_new/cur':>12} {'vs_tier':>8}")
    for g, n in CASES:
        u = 2 * n
        k = pick_k(u)
        lp = lm_pow2_for(u, k)
        lt = fft_ceil_tiers((u * 64 + k - 1) // k)
        c = pick_cyc(n, bud)
        if not c:
            print(f"{g:<16}{n:>7} {k:>3} {lp:>9} {lt:>9} |  (no cyc)")
            continue
        L, kc, mc, coef = c
        cur = 4 * lp
        new = 2 * lt + 2 * L
        print(f"{g:<16}{n:>7} {k:>3} {lp:>9} {lt:>9} | {L:>8} {kc:>3} {mc:>7} "
              f"{coef/2**48:>10.3f} {new/cur:>12.3f} {(2*lt+2*L)/(4*lt):>8.3f}")
    print()
