#!/usr/bin/env python3
"""针对大用例形状, 搜索更优的 mu 参数 (in) 选择。

对每个 (len1, len2) 形状, 按 D4 现行公式算出 in, 并与候选 in 做建模代价比较。
代价单位 = FFT 变换的 n*log2(n) 之和 (确定性, 与源码一致的结构量)。
"""
import json
import math
from pathlib import Path

HERE = Path(__file__).parent
SH = json.loads((HERE / 'shapes.json').read_text())


def ceil2(x):
    n = 1
    while n < x:
        n <<= 1
    return n


def nlogn(n):
    return n * math.log2(n) if n > 1 else 0.0


def d4_in(qn, len2):
    """D4 的自然 mu_in 选择 (未钳制)。"""
    if qn > len2:
        return (qn - 1) // ((qn - 1) // len2 + 1) + 1
    if 3 * qn > len2:
        in2 = min((qn - 1) // 2 + 1, len2)
        in4 = min((qn - 1) // 4 + 1, len2)
        return in4 if ceil2(len2 + in4) < ceil2(len2 + in2) else in2
    return qn


import os
INV_W = float(os.environ.get('INV_W', '8'))   # 倒数代价权重, 由实测 INV 占比反标定


def mu_cost(qn, len2, inv):
    """absDivMu 的建模 FFT 代价。

    inv 计算: Newton 倍增, 顶层卷积 ~2*in, 总量近似 2 倍顶层 (几何级数) x 3 变换
    块循环: 每块 fftMulPre#1 (inv_float_len) + #2 (cyclic_m 或 divisor_float_len)
            fftMulPre 复用预计算 DFT -> 每次 2 个变换 (正 1 + 逆 1)
    """
    if inv < 1:
        return float('inf'), 0, 0, 0
    fl1 = ceil2(2 * inv + 1)
    fl2 = ceil2(len2 + inv)
    cyc = max(ceil2(len2 + 1), ceil2((len2 + inv) // 2 + 1))
    use_cyc = cyc < inv + len2 and inv >= 64
    fl2u = cyc if use_cyc else fl2
    # 倒数: 递归倍增, 各层 2*k 卷积; 近似为 2 x 顶层, 每层 3 个变换
    c_inv = INV_W * nlogn(fl1)
    blocks = max(1, math.ceil(qn / inv))
    # prepareDFT: divisor 1 次 (+cyclic 1 次), inv 1 次
    c_pre = nlogn(fl2) + (nlogn(cyc) if use_cyc else 0) + nlogn(fl1)
    c_blk = blocks * (2 * nlogn(fl1) + 2 * nlogn(fl2u))
    return c_inv + c_pre + c_blk, blocks, fl1, fl2u


def candidates(qn, len2):
    """候选 in 集合: 各种块数 + 幂次边界下探。"""
    s = set()
    for b in range(1, 33):
        v = (qn + b - 1) // b
        if 1 <= v <= len2:
            s.add(v)
    for v in list(s):
        # 下探到使 ceil2(2v+1) 减半的最大值
        f = ceil2(2 * v + 1)
        lo = (f // 2 - 1) // 2
        if 1 <= lo <= len2:
            s.add(lo)
        # 下探到使 ceil2(len2+v) 减半
        g = ceil2(len2 + v)
        lo2 = g // 2 - len2
        if 1 <= lo2 <= len2:
            s.add(lo2)
    s.add(len2)
    return sorted(x for x in s if 1 <= x <= len2)


def main():
    rows = []
    for case, shapes in SH.items():
        for l1, l2 in shapes:
            qn = l1 - l2
            if l2 <= 64 or qn <= 64 or l1 < 2 * l2:
                continue          # 非 mu 路径
            rows.append((case, l1, l2, qn))
    rows.sort(key=lambda r: -r[1])

    print(f'{"case":26s} {"len1":>8s} {"len2":>7s} {"qn":>7s} '
          f'{"in_d4":>7s} {"blk":>4s} {"cost_d4":>10s} | '
          f'{"in*":>7s} {"blk":>4s} {"cost*":>10s} {"gain":>6s}')
    print('-' * 118)
    seen = set()
    for case, l1, l2, qn in rows[:40]:
        nat = d4_in(qn, l2)
        cur = min(max(nat, 64), l2)
        c_cur, b_cur, _, _ = mu_cost(qn, l2, cur)
        best, c_best, b_best = cur, c_cur, b_cur
        for v in candidates(qn, l2):
            if v < 64:
                continue          # 保持 D4 的 in>=64 下限 (数值安全)
            c, b, _, _ = mu_cost(qn, l2, v)
            if c < c_best:
                best, c_best, b_best = v, c, b
        key = (l1, l2)
        if key in seen:
            continue
        seen.add(key)
        print(f'{case:26s} {l1:>8d} {l2:>7d} {qn:>7d} {cur:>7d} {b_cur:>4d} '
              f'{c_cur:>10.4g} | {best:>7d} {b_best:>4d} {c_best:>10.4g} '
              f'{100 * (c_best / c_cur - 1):>5.1f}%')


if __name__ == '__main__':
    main()
