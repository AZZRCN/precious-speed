#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""对 cases_hex 每组算: 路由(knuth/newton/bz)、块长 n、块数 t,
   以及 Barrett 块循环里两次乘法的 FFT 长度:
     lm_pow2  = 现行 fm_prep 的 next_pow2(coeffs)   (纯 2 幂, 可能浪费 2x)
     lm_tier  = fft_ceil_tiers(ceil(64u/k))         (混合档, mul_fft 已在用)
     lm_cyc   = 环形 mod(B^mc-1) 的 FFT 长度         (mc>=n+2, 半尺寸)
   最后按「每块 transform 长度总和」估算指令数收益。
"""
import os, sys, math

BZ_CUTOFF = 64
BZ_MIN = BZ_CUTOFF * 2 + 32     # 160
KD_QMAX = None                  # 从 div.cpp 读
BARRETT_NMIN = 512

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


def valid_tier(L):
    """L 是否是合法 FFT 档: 2^j / 3*2^(j-2) / 5*2^(j-3)"""
    if L < 32:
        return False
    if (L & (L - 1)) == 0:
        return True
    if L % 3 == 0 and ((L // 3) & (L // 3 - 1)) == 0:
        return True
    if L % 5 == 0 and ((L // 5) & (L // 5 - 1)) == 0:
        return True
    return False


def lm_pow2_for(u, k):
    coeffs = (u * 64 // k) + 1
    return 2 << (coeffs.bit_length() - 1)


def pick_cyclic(nmin, kmax=19, kmin=8):
    """找最小 FFT 长度 L(合法档) 使存在 k, 64|L*k, mc=L*k/64>=nmin, 精度 2^(2k)*L<=2^48。
       返回 (L, k, mc) 或 None"""
    best = None
    # 枚举所有合法档
    Ls = []
    for j in range(5, 25):
        for f, d in ((1, 1), (3, 4), (5, 8)):
            L = (1 << j) * f // d if (1 << j) * f % d == 0 else None
            if L and L >= 32:
                Ls.append(L)
    for L in sorted(set(Ls)):
        for k in range(kmax, kmin - 1, -1):
            if (L * k) % 64:
                continue
            mc = L * k // 64
            if mc < nmin:
                continue
            if (1 << (2 * k)) > (1 << 48) // L:
                continue
            if best is None or L < best[0]:
                best = (L, k, mc)
            break
        if best:
            break
    return best


def limbs(hexs):
    bits = len(hexs.lstrip('0')) * 4
    if bits == 0:
        bits = 1
    v = int(hexs, 16)
    return max(1, (v.bit_length() + 63) // 64), v


def bz_params(na, nb):
    q = nb // BZ_CUTOFF
    m = 1 << ((q if q else 1).bit_length())   # 1 << (32-clz(q)) == next_pow2 strictly greater
    j = (nb + m - 1) // m
    n = j * m
    return n


def analyze(path, kdqmax):
    with open(path) as f:
        T = int(f.readline())
        rows = []
        for _ in range(T):
            ln = f.readline().split()
            if len(ln) < 2:
                continue
            na, av = limbs(ln[0])
            nb, bv = limbs(ln[1])
            if av < bv:
                rows.append(('cmp', na, nb, 0, 0))
                continue
            if nb == 1:
                rows.append(('d1', na, nb, 0, 0))
                continue
            if nb < BZ_MIN or (na - nb + 1) <= kdqmax:
                rows.append(('knuth', na, nb, 0, 0))
                continue
            if na <= 2 * nb:
                rows.append(('newton', na, nb, nb, 0))
                continue
            n = bz_params(na, nb)
            # t
            nas = na + 1   # sigma>0 时 +1 (近似)
            abits = 64 * nas
            t = abits // (64 * n) + 1
            if t < 2:
                t = 2
            rows.append(('bz', na, nb, n, t))
        return rows


def main():
    d = sys.argv[1] if len(sys.argv) > 1 else 'cases_hex'
    kdqmax = int(sys.argv[2]) if len(sys.argv) > 2 else 8
    groups = sorted(os.listdir(d))
    print(f"{'group':<18}{'route':<8}{'#':>4} {'na':>7} {'nb':>7} {'n':>7} {'t':>4} "
          f"{'k':>3} {'lm_pow2':>9} {'lm_tier':>9} {'lm_cyc':>8} {'mc':>7} {'kc':>3} {'blkgain':>8}")
    for g in groups:
        if not g.endswith('.in'):
            continue
        rows = analyze(os.path.join(d, g), kdqmax)
        # 汇总: 按 route 分类, 取 na*nb 最大的代表 case
        for route in ('bz', 'newton'):
            sel = [r for r in rows if r[0] == route]
            if not sel:
                continue
            sel.sort(key=lambda r: r[1] * r[2])
            r = sel[-1]
            _, na, nb, n, t = r
            if route == 'bz':
                u = 2 * n
                k = pick_k(u)
                lp = lm_pow2_for(u, k)
                lt = fft_ceil_tiers((u * 64 + k - 1) // k)
                cyc = pick_cyclic(n + 2)
                lc, kc, mc = cyc if cyc else (0, 0, 0)
                # 每块 transform 长度总和: 现行 = 4*lp (两次 fm_mul, 各 fwd+inv)
                #   tier 版 = 4*lt; tier+cyc = 2*lt + 2*lc
                cur = 4 * lp
                new = 2 * lt + 2 * lc if lc else 4 * lt
                gain = new / cur
                print(f"{g[:-3]:<18}{route:<8}{len(sel):>4} {na:>7} {nb:>7} {n:>7} {t:>4} "
                      f"{k:>3} {lp:>9} {lt:>9} {lc:>8} {mc:>7} {kc:>3} {gain:>8.3f}")
            else:
                print(f"{g[:-3]:<18}{route:<8}{len(sel):>4} {na:>7} {nb:>7} {n:>7} {t:>4}")
        others = {}
        for r in rows:
            if r[0] not in ('bz', 'newton'):
                others[r[0]] = others.get(r[0], 0) + 1
        if others:
            print(f"{g[:-3]:<18}{'other':<8} {others}")


main()
