"""Newton 倒数阶梯的 FFT 工作量模拟器（确定性结构分析，非计时）。

代价单位 = FFT 变换长度（ceil2 后的浮点点数），只比较相对量。
逐层代价（对照 div_D9.cpp absInvNewtonGMP）：
  cyclic ON : mn(=ceil2(k+1)) + ceil2(2*rn)            # fftMulModBm1 + absMul(xp_high, inv0_low)
  cyclic OFF: ceil2(2*(rn+1)) + ceil2(2*(rn+1)+k-1)    # absSqr(inv0) + absMul(inv0^2, m)

硬约束：
  rn >= ceil(k/2)     Newton 精度下界（等价 s <= (k-1)/2）
  rn <= 2k/3          组合逻辑 assert(2k >= 3rn)
  cyclic 生效: ceil2(k+1) <= k + rn 且 k >= CYCLIC_MIN_K
"""
CYCMIN = 4096
GUARD = 0.65   # rn/k 上限（硬上限 2/3，留一点余量）


def ceil2(x):
    m = 1
    while m < x:
        m <<= 1
    return m


def cost_level(k, rn, on):
    if on:
        return ceil2(k + 1) + ceil2(2 * rn)
    return ceil2(2 * (rn + 1)) + ceil2(2 * (rn + 1) + k - 1)


def pick_rn(k, mode):
    """返回该层使用的 rn。"""
    rn_min = (k + 1) // 2          # = k - (k-1)//2
    rn_max = int(k * GUARD)
    p2 = ceil2(k + 1)
    rn_cyc = p2 - k                # 开 cyclic 所需的最小 rn

    if mode == "d4":               # 原版：永远默认 split
        return rn_min

    if mode == "d9":               # 现状：只在本层能开 cyclic 时下调 s
        if k >= CYCMIN and rn_min < rn_cyc <= int(k * 0.6):
            return rn_cyc
        return rn_min

    if mode == "d10":              # 新策略：本层开不了就为子层铺路（且不加本层档位）
        if k < CYCMIN:
            return rn_min
        if rn_cyc <= rn_min:
            return rn_min          # 默认 split 就已经开着
        if rn_cyc <= rn_max:
            return rn_cyc          # 本层可开 → 抬到刚好开启
        # 本层开不了 cyclic（k 刚过 2 幂，需 rn>2k/3）。
        # 退而求其次：在「不抬高本层 fallback FFT 档位」前提下把 rn 顶到最大，
        # 让子层 k'=rn 的比值跳出死区。闭式（与 C++ 实现逐字对应）：
        c1 = ceil2(2 * (rn_min + 1))            # absSqr(inv0)
        c2 = ceil2(2 * (rn_min + 1) + k - 1)    # absMul(inv0^2, m)
        lim1 = c1 // 2 - 1                      # 2*(rn+1)      <= c1
        lim2 = (c2 - k - 1) // 2                # 2*(rn+1)+k-1  <= c2
        rn_free = min(lim1, lim2, rn_max)
        if rn_free <= rn_min:
            return rn_min
        # 候选：rn_free 与区间内最大的 2^j-1（比值 ≈1 的局部最优点）
        cands = [rn_free]
        p = ceil2(rn_free + 1) // 2 - 1
        if rn_min <= p <= rn_free:
            cands.append(p)
        best = max(cands, key=lambda r: r / ceil2(r + 1))
        # 闸门：只有当子层**真能**因此开启 cyclic 才动手，否则白白把子问题做大。
        #   (a) 子层规模须过 CYCLIC_MIN_K（否则子层被阈值挡掉，纯亏）；
        #   (b) 子层需要的 rn' = ceil2(k'+1)-k' 须 <= 0.65k'，即 ceil2(k'+1) <= 1.65k'。
        # 去掉这两条后，k0=4096 一带有 14 个点会 +0.8%（子层 2662 < 4096 被阈值挡掉）。
        if best < CYCMIN or ceil2(best + 1) * 20 > best * 33:
            return rn_min
        return best
    raise ValueError(mode)


def ladder(k0, mode, label="", verbose=True):
    if verbose:
        print(f"--- {label}  k0={k0}  mode={mode} ---")
    k, lvl, on_cnt, tot, total_cost = k0, 0, 0, 0, 0
    while k > 64 and lvl <= 14:
        p2 = ceil2(k + 1)
        rn = pick_rn(k, mode)
        on = (p2 <= k + rn) and (k >= CYCMIN)
        c = cost_level(k, rn, on)
        total_cost += c
        tot += 1
        on_cnt += on
        if verbose:
            print(f" L{lvl}: k={k:7d} p2={p2:7d} r={k/p2:.3f} rn={rn:6d} "
                  f"rn/k={rn/k:.3f} {'ON ' if on else 'off'} cost={c:9d}")
        k = rn
        lvl += 1
    if verbose:
        print(f" => cyclic ON {on_cnt}/{tot}   总 FFT 单位 = {total_cost}\n")
    return total_cost, on_cnt, tot


if __name__ == "__main__":
    CASES = [
        ("length_ratio_integer_02", 83334),
        ("a_max_b_random_02", 146990),
    ]
    for name, k0 in CASES:
        print(f"########## {name} ##########")
        res = {}
        for mode in ("d4", "d9", "d10"):
            res[mode] = ladder(k0, mode, name)
        b = res["d4"][0]
        print(f"  {name}: D9/D4 = {res['d9'][0]/b:.3f}   D10/D4 = {res['d10'][0]/b:.3f}"
              f"   D10/D9 = {res['d10'][0]/res['d9'][0]:.3f}\n")
