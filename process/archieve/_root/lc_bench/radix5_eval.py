# -*- coding: utf-8 -*-
"""
精确复刻 div_D41.cpp 的 muBlockCost + nb 选择, 评估加 radix-5 档位对
length_ratio_integer 全族 (LC 最高点所在) 的收益。

档位系统:
  BASE3 = {2^k, 3*2^k}            (当前 D41)
  BASE5 = {2^k, 3*2^k, 5*2^k}     (候选: 单层 radix-5)
  BASE35= {2^a*3^b*5^c, b<=1,c<=1} (3*5*2^k 也允许)
"""
import math

def gen_ladder(allow):
    """allow: 奇因子集合, 如 {1,3} / {1,3,5} / {1,3,5,15}"""
    s = set()
    for odd in allow:
        v = odd
        while v <= 1 << 22:
            if v >= 16:
                s.add(v)
            v <<= 1
    return sorted(s)

def make_ceil(ladder):
    def f(n):
        for v in ladder:
            if v >= n:
                return v
        return ladder[-1]
    return f

def W(n):
    return n * math.log2(n) if n > 1 else 0.0

def mu_block_cost(qn, len2, inc, ceil_lin, ceil_cycm):
    """逐字复刻 muBlockCost"""
    if inc > len2 or inc < 64:
        return 1e300
    nblk = (qn + inc - 1) // inc
    Fi = ceil_lin(2 * inc + 1)
    Fl = ceil_lin(len2 + inc)
    Fc_cand = max(ceil_cycm(len2 + 1), ceil_cycm((len2 + inc) // 2 + 1))
    cyc = Fc_cand < inc + len2
    Fc = Fc_cand if cyc else Fl
    wi, wc = W(Fi), W(Fc)
    return 6.8 * wi + wi + wc + nblk * (2 * wi + 2 * wc)

def pick_in(qn, len2, ceil_lin, ceil_cycm):
    """复刻 absDivRem 第一分支的 nb 选择 (qn > len2)"""
    if qn > len2:
        nb = (qn - 1) // len2 + 1
        in_nat = (qn - 1) // nb + 1
        if in_nat >= 16384:
            cost_of = lambda c: mu_block_cost(qn, len2, (qn - 1) // c + 1, ceil_lin, ceil_cycm)
            base = cost_of(nb)
            best_nb, best_c = nb, base
            for c in range(nb + 1, nb + 5):
                cc = cost_of(c)
                if cc < best_c:
                    best_c, best_nb = cc, c
            if best_nb != nb and best_c < base * 0.95:
                nb = best_nb
        return (qn - 1) // nb + 1
    else:
        # 第二分支 (3*qn > len2)
        in2 = (qn - 1) // 2 + 1
        in4 = (qn - 1) // 4 + 1
        in2, in4 = min(in2, len2), min(in4, len2)
        mu_in = in4 if ceil_lin(len2 + in4) < ceil_lin(len2 + in2) else in2
        if mu_in >= 16384:
            base = mu_block_cost(qn, len2, mu_in, ceil_lin, ceil_cycm)
            best_in, best_c = mu_in, base
            for nbc in range(2, 9):
                inc = min((qn - 1) // nbc + 1, len2)
                cc = mu_block_cost(qn, len2, inc, ceil_lin, ceil_cycm)
                if cc < best_c:
                    best_c, best_in = cc, inc
            if best_in != mu_in and best_c < base * 0.95:
                mu_in = best_in
        return mu_in

SUM = 4000002
def lri_shape(seed):
    m = [2, 3, 5, 10, 20, 50][seed % 6]
    blen = SUM // ((m + 1) * 2)
    alen = blen * m
    return (alen + 3) // 4, (blen + 3) // 4, m

LADDERS = {
    'D41  {2^k,3*2^k}':      {1, 3},
    '+5   {2^k,3,5}':        {1, 3, 5},
    '+5+15 {..,5,15}':       {1, 3, 5, 15},
    '+5+7 {..,5,7}':         {1, 3, 5, 7},
    'full {1,3,5,7,9,15,21,25,27,45}': {1, 3, 5, 7, 9, 15, 21, 25, 27, 45},
}

print('%-34s %9s %9s %9s %9s %9s %9s   %8s' %
      ('ladder', '_00', '_01', '_02', '_03', '_04', '_05', 'MAXCASE'))
base_costs = None
for name, odds in LADDERS.items():
    lad = gen_ladder(odds)
    cl = make_ceil(lad)
    cc = make_ceil(lad)
    costs = []
    for seed in range(6):
        len1, len2, m = lri_shape(seed)
        qn = len1 - len2
        inc = pick_in(qn, len2, cl, cc)
        costs.append(mu_block_cost(qn, len2, inc, cl, cc))
    if base_costs is None:
        base_costs = costs[:]
        print('%-34s %9.4g %9.4g %9.4g %9.4g %9.4g %9.4g   %8.4g' %
              (name, *costs, max(costs)))
    else:
        r = [c / b for c, b in zip(costs, base_costs)]
        print('%-34s %9.4f %9.4f %9.4f %9.4f %9.4f %9.4f   %8.4f' %
              (name, *r, max(costs) / max(base_costs)))

print()
print('=== 详细: D41 vs +5 在最高点 _02 与全族的 in/nblk/Fi/Fc ===')
for name in ['D41  {2^k,3*2^k}', '+5   {2^k,3,5}']:
    lad = gen_ladder(LADDERS[name]); cl = make_ceil(lad)
    print('--- ' + name)
    for seed in range(6):
        len1, len2, m = lri_shape(seed)
        qn = len1 - len2
        inc = pick_in(qn, len2, cl, cl)
        nblk = (qn + inc - 1) // inc
        Fi = cl(2 * inc + 1)
        Fc = max(cl(len2 + 1), cl((len2 + inc) // 2 + 1))
        cost = mu_block_cost(qn, len2, inc, cl, cl)
        print('  _%02d m=%-2d len1=%7d len2=%6d qn=%6d in=%6d nblk=%d '
              'Fi=%6d(%.3f) Fc=%6d(%.3f) cost=%.4g'
              % (seed, m, len1, len2, qn, inc, nblk,
                 Fi, Fi / (2 * inc + 1), Fc, Fc / (len2 + 1), cost))
