#!/usr/bin/env python3
"""用 LC #390120 的逐点耗时标定结构代价模型, 再预测各 SB_LIMIT 的逐点耗时。

模型:  t = d*bytes + a*wfft + e*nxf + b*wsb + c*ncall
只用 LC 官方计时 (允许) + 确定性结构计数器 (无计时)。
LC 判定 = 各测试点耗时的最大值。
"""
import json
from pathlib import Path

import numpy as np

HERE = Path(__file__).parent
M = json.loads((HERE / 'model.json').read_text())

LC = {  # #390120 (D4, 75ms) 逐点耗时
    'example_00': 1, 'small_00': 12, 'medium_00': 17, 'medium_01': 37, 'medium_02': 44,
    'large_00': 40, 'large_01': 45, 'max_00': 12, 'max_01': 5, 'max_02': 4,
    'a_max_b_random_00': 44, 'a_max_b_random_01': 61, 'a_max_b_random_02': 71,
    'r_nearly_zero_00': 16, 'r_nearly_zero_01': 69, 'r_nearly_zero_02': 41,
    'length_ratio_integer_00': 68, 'length_ratio_integer_01': 66, 'length_ratio_integer_02': 75,
    'length_ratio_integer_03': 63, 'length_ratio_integer_04': 56, 'length_ratio_integer_05': 60,
    'burnikel_ziegler_bound_00': 48, 'burnikel_ziegler_bound_01': 42,
    'burnikel_ziegler_bound_02': 53, 'burnikel_ziegler_bound_03': 42,
}

SIZES = M['sizes']
CASES = sorted(LC)
# 列: bytes, wfft, nxf, wsb, ncall, nbig
SC = [1e-6, 1e-9, 1e-4, 1e-9, 1e-4, 1e-3]
NAMES = ['bytes(1e6)', 'wfft(1e9)', 'nxf(1e4)', 'wsb(1e9)', 'ncall(1e4)', 'nbig(1e3)']


def design(run):
    return np.array([[SIZES[c] * SC[0], run[c][0] * SC[1], run[c][1] * SC[2],
                      run[c][2] * SC[3], run[c][3] * SC[4], run[c][4] * SC[5]]
                     for c in CASES])


def nnls(A, y, iters=300000):
    """投影梯度 NNLS (避免 scipy 依赖)。"""
    x = np.ones(A.shape[1]) * 0.01
    L = np.linalg.norm(A, 2) ** 2
    for _ in range(iters):
        g = A.T @ (A @ x - y)
        x = np.maximum(0.0, x - g / L)
    return x


def main():
    base = M['runs']['0']
    A = design(base)
    y = np.array([float(LC[c]) for c in CASES])
    x = nnls(A, y)
    pred = A @ x
    print('=== 拟合系数 (ms / 单位) ===')
    for n, v in zip(NAMES, x):
        print(f'  {n:12s} {v:12.5f}')
    print(f'  RMSE = {np.sqrt(((pred - y) ** 2).mean()):.2f} ms, '
          f'max|err| = {np.abs(pred - y).max():.1f} ms')
    print()
    print(f'{"case":26s} {"LC":>4s} {"fit":>6s} {"err":>6s}')
    for c, p, t in zip(CASES, pred, y):
        print(f'{c:26s} {t:>4.0f} {p:>6.1f} {p - t:>6.1f}')

    # 差分预测: pred(case,lim) = LC(case) + [fit(case,lim) - fit(case,0)]
    # 每个用例的未建模残差视为常量, 比绝对预测可靠得多。
    p0 = A @ x
    print('\n=== 各 SB_LIMIT 差分预测 (LC + 模型增量) ===')
    print(f'{"limit":>9s} {"max(ms)":>8s} {"sum":>7s}   worst-3')
    tab = {}
    for lim, run in M['runs'].items():
        p = y + (design(run) @ x - p0)
        tab[int(lim)] = p
        order = np.argsort(-p)[:3]
        w = ' '.join(f'{CASES[i]}={p[i]:.0f}' for i in order)
        print(f'{int(lim):>9d} {p.max():>8.1f} {p.sum():>7.0f}   {w}')

    lims = sorted(tab)
    print('\n=== 逐点 x limit ===')
    print(f'{"case":26s} ' + ' '.join(f'{l:>8d}' for l in lims))
    for i, c in enumerate(CASES):
        print(f'{c:26s} ' + ' '.join(f'{tab[l][i]:>8.1f}' for l in lims))

    print('\n=== 各 limit 下 wsb 最大的用例 (schoolbook 风险) ===')
    for lim in lims:
        run = M['runs'][str(lim)]
        j = max(CASES, key=lambda c: run[c][2])
        print(f'limit={lim:>9d}  max wsb = {run[j][2]:.4g} @ {j}  '
              f'(≈{run[j][2] * x[3] * SC[3]:.1f} ms)')


if __name__ == '__main__':
    main()
