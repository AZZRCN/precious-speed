"""LC 代理指标（纯相对量，不做绝对时间比较）。

LC 的分数 = 该提交在所有用例上的最慢值。因此候选 X 相对基准 G 的 LC 比值为

    LC_ratio(X) = max_c X[c] / max_c G[c]
                = max_c ( X[c]/G[c] * G[c]/G[bn] )
                = max_c ( ratio_X[c] * share_G[c] )

其中 bn 是基准自己的最慢用例。两个因子都是相对量：
  ratio_X[c]  —— 同 (rep,case) 配对比值的中位数（漂移已约掉）
  share_G[c]  —— 基准自身跨用例占比（同一候选内部，无跨候选绝对比较）

关键：随着 FFT 变快，瓶颈用例会从 max_max 迁移到 large_small / small，
直接盯着 max_max 看会系统性高估收益。
"""
import json
import statistics as st
import sys

path = sys.argv[1] if len(sys.argv) > 1 else "/home/azzr/mulbench/coldratio.json"
d = json.load(open(path))
ratios = d["ratios"]
absd = d["abs_ms_diagnostic_only"]
base = d["baseline"]
cands = list(ratios.keys())
cases = list(ratios[base].keys())

bn = max(cases, key=lambda c: st.median(absd[base][c]))
share = {c: st.median(absd[base][c]) / st.median(absd[base][bn]) for c in cases}

print(f"基准 = {base}，其最慢用例 = {bn}")
print()
print(f"{'cand':<12}{'LC_ratio':>10}{'gain':>9}   {'制约用例':<20}{'naive@maxmax':>13}")
print("-" * 68)
rows = []
for x in cands:
    eff = {c: st.median(ratios[x][c]) * share[c] for c in cases}
    worst = max(cases, key=lambda c: eff[c])
    lc = eff[worst]
    naive = st.median(ratios[x][bn])
    rows.append((x, lc, worst, naive))
    print(f"{x:<12}{lc:10.3f}{(lc - 1) * 100:+8.1f}%   {worst:<20}{naive:13.3f}")

print()
print("各候选的用例贡献（ratio_X[c] * share_G[c]，取最大者为制约点）；只列 top5")
for x, lc, worst, naive in rows:
    eff = sorted(((st.median(ratios[x][c]) * share[c], c) for c in cases), reverse=True)
    print(f"  {x:<12}" + "  ".join(f"{c.replace('.in',''):>16}={v:.3f}" for v, c in eff[:5]))
