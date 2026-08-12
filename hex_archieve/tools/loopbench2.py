#!/usr/bin/env python3
"""loopbench2.py - 抗热衰退的 A/B 墙钟判优器

用法:
    python3 loopbench2.py CASEFILE N R binA binB [binC ...]
      CASEFILE  输入用例
      N         每个 block 内连跑次数 (摊薄 fork 抖动)
      R         轮数, 至少 10 (默认建议 12)

核心设计 (针对热衰退 / thermal decay 与频率漂移):
  1. **ABBA 蛇形交替**: 偶数轮反序执行, 抵消"先跑者占便宜"的一阶漂移项。
  2. **同轮配对比值 (paired ratio)** 是主判据, 不是全局 MIN。
     同一轮内 A/B 紧邻执行, 温度/频率状态近似相同, ratio 天然抵消慢漂移。
     报告 ratio 的 median + IQR, 而非均值 (抗离群)。
  3. **漂移诊断**: 对每个 bin 的时间序列做最小二乘线性拟合, 给出 %/round 斜率;
     再算前半 vs 后半均值差。若 |drift| > 3%, 打印 THERMAL_DRIFT 警告 ->
     此时墙钟结论不可采信, 必须改用 cachegrind Ir / L2esc 判优 (铁律 M3/M10)。
  4. **显著性闸门**: 若 ratio 的 IQR 覆盖 1.0, 判 "NO_DIFF" —— 不许把噪声当成收益。
"""
import subprocess, sys, time, os, statistics as st

def run_block(binp, case, n, cpu):
    with open(case, 'rb') as f:
        data = f.read()
    devnull = subprocess.DEVNULL
    t0 = time.perf_counter_ns()
    for _ in range(n):
        p = subprocess.Popen(['taskset', '-c', str(cpu), binp],
                             stdin=subprocess.PIPE, stdout=devnull, stderr=devnull)
        p.communicate(data)
    t1 = time.perf_counter_ns()
    return (t1 - t0) / n / 1000.0  # us

def binom_two_sided(k, n):
    """双尾二项检验 (p=0.5), 精确计算, 不依赖 scipy"""
    from math import comb
    if n == 0:
        return 1.0
    tot = 2.0 ** n
    pk = comb(n, k) / tot
    s = 0.0
    for i in range(n + 1):
        pi = comb(n, i) / tot
        if pi <= pk * (1 + 1e-12):
            s += pi
    return min(1.0, s)

def slope_pct(ys):
    """最小二乘斜率, 归一化为 %/round"""
    n = len(ys)
    if n < 3:
        return 0.0
    mx = (n - 1) / 2.0
    my = sum(ys) / n
    num = sum((i - mx) * (y - my) for i, y in enumerate(ys))
    den = sum((i - mx) ** 2 for i in range(n))
    if den == 0 or my == 0:
        return 0.0
    return num / den / my * 100.0

def main():
    if len(sys.argv) < 5:
        print(__doc__)
        sys.exit(1)
    case, n, r = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
    bins = sys.argv[4:]
    cpu = int(os.environ.get('ZB_CPU', '3'))
    if r < 10:
        print(f"!! R={r} < 10, 违反铁律 M10 (墙钟至少 10 轮)。强制提升到 10。")
        r = 10

    burn = int(os.environ.get('ZB_BURN', '3'))
    print(f"case={os.path.basename(case)} N={n} R={r} burn={burn} cpu={cpu} bins={[os.path.basename(b) for b in bins]}")
    # burn-in: 实测发现前几轮系统性偏慢 (频率 boost 爬坡 + 页缓存冷),
    # drift 可达 -0.8%/round。必须跑满 burn 个完整 block 并丢弃, 否则先跑者吃亏。
    for _ in range(burn):
        for b in bins:
            run_block(b, case, n, cpu)

    series = {b: [] for b in bins}
    order_log = []
    for rd in range(r):
        order = bins if rd % 2 == 0 else list(reversed(bins))  # ABBA 蛇形
        order_log.append('F' if rd % 2 == 0 else 'R')
        for b in order:
            series[b].append(run_block(b, case, n, cpu))

    print("\n--- per-round (us) ---")
    hdr = "round dir " + " ".join(f"{os.path.basename(b):>12}" for b in bins)
    print(hdr)
    for i in range(r):
        row = f"{i:5d} {order_log[i]}   " + " ".join(f"{series[b][i]:12.1f}" for b in bins)
        print(row)

    print("\n--- stats ---")
    print(f"{'bin':<14}{'MIN':>10}{'MED':>10}{'drift%/rd':>11}{'half-delta%':>13}")
    drift_bad = False
    for b in bins:
        ys = series[b]
        h = len(ys) // 2
        first, second = sum(ys[:h]) / h, sum(ys[h:]) / (len(ys) - h)
        hd = (second - first) / first * 100.0
        sl = slope_pct(ys)
        if abs(sl) > 1.0 or abs(hd) > 3.0:
            drift_bad = True
        print(f"{os.path.basename(b):<14}{min(ys):10.1f}{st.median(ys):10.1f}{sl:11.3f}{hd:13.2f}")

    if len(bins) >= 2:
        base = bins[0]
        print("\n--- paired ratio vs {} (同轮配对, 抗漂移主判据) ---".format(os.path.basename(base)))
        for b in bins[1:]:
            ratios = [series[b][i] / series[base][i] for i in range(r)]
            ratios_s = sorted(ratios)
            q1 = ratios_s[len(ratios_s) // 4]
            q3 = ratios_s[(len(ratios_s) * 3) // 4]
            med = st.median(ratios)
            # 符号检验: IQR 是分位数, 对"方向一致的小效应"不敏感。
            # 若 R 轮里胜负方向高度一致, 即使幅度 < IQR 宽度也是真实效应。
            wins = sum(1 for x in ratios if x < 1.0)
            p = binom_two_sided(wins, len(ratios))
            iqr_sig = not (q1 <= 1.0 <= q3)
            sign_sig = p < 0.05
            if iqr_sig or sign_sig:
                verdict = "FASTER" if med < 1.0 else "SLOWER"
                if not iqr_sig:
                    verdict += "(仅符号检验显著, 幅度<墙钟地板, 需 cachegrind 佐证)"
            else:
                verdict = "NO_DIFF"
            print(f"  {os.path.basename(b):<14} med={med:.4f}  IQR=[{q1:.4f},{q3:.4f}]  "
                  f"wins={wins}/{len(ratios)} p={p:.4f}  minMIN_rel={min(series[b])/min(series[base]):.4f}")
            print(f"  {'':<14} -> {verdict}")

    if drift_bad:
        print("\n!! THERMAL_DRIFT 警告: 检测到显著单调漂移 (|slope|>1%/round 或 |half-delta|>3%)。")
        print("   墙钟绝对值不可信。结论只能采信 paired ratio, 且必须用 cachegrind Ir/L2esc 交叉验证 (M3/M10)。")
    else:
        print("\n[OK] 无显著热漂移, 墙钟与 paired ratio 均可采信。")

if __name__ == '__main__':
    main()
