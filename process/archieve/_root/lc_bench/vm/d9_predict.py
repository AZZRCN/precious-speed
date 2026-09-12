"""把 D9 的 VM 配对比值套到 D4 的 LC 实测逐点耗时上, 预测新瓶颈。

注意: 这只是排序参考。VM->LC 迁移率历史上是分层且方向相反的
(缺页类 VM 乐观 ~11%; 缓存/局部性类 VM 保守 ~214%)。
本次改动是**算法层 FFT 工作量削减**, 迁移率应接近 1:1, 但仍以 LC 回执为准。
"""

# D4 = LC #390120 = 75ms, 逐测试点实测 (ms)
LC_D4 = {
    "example_00": 1, "small_00": 12,
    "medium_00": 17, "medium_01": 37, "medium_02": 44,
    "large_00": 40, "large_01": 45,
    "max_00": 12, "max_01": 5, "max_02": 4,
    "a_max_b_random_00": 44, "a_max_b_random_01": 61, "a_max_b_random_02": 71,
    "r_nearly_zero_00": 16, "r_nearly_zero_01": 69, "r_nearly_zero_02": 41,
    "length_ratio_integer_00": 68, "length_ratio_integer_01": 66,
    "length_ratio_integer_02": 75, "length_ratio_integer_03": 63,
    "length_ratio_integer_04": 56, "length_ratio_integer_05": 60,
    "burnikel_ziegler_bound_00": 48, "burnikel_ziegler_bound_01": 42,
    "burnikel_ziegler_bound_02": 53, "burnikel_ziegler_bound_03": 42,
}

# cand/D4 配对比值中位数 (VM, divratio 9 reps, 2026-08-03 12:1x, 三方同跑一轮)
# 噪声地板标定: r_nearly_zero_02 上 D9=0.983 / D10=1.001, 但探针显示两者在该用例
# 行为**逐层完全相同**(6/32) -> 这 1.8% 纯属测量噪声。故 |ratio-1| < 0.02 不作数。
RATIO = {
    "D9": {
        "length_ratio_integer_00": 0.901, "length_ratio_integer_01": 0.893,
        "length_ratio_integer_02": 0.925, "length_ratio_integer_03": 0.949,
        "length_ratio_integer_04": 0.992, "length_ratio_integer_05": 0.976,
        "a_max_b_random_00": 0.916, "a_max_b_random_01": 0.982,
        "a_max_b_random_02": 0.995,
        "r_nearly_zero_00": 0.997, "r_nearly_zero_01": 0.989,
        "r_nearly_zero_02": 0.983,
        "burnikel_ziegler_bound_00": 0.999, "burnikel_ziegler_bound_01": 1.004,
        "burnikel_ziegler_bound_02": 0.996, "burnikel_ziegler_bound_03": 1.004,
        "large_00": 1.001, "large_01": 1.009,
        "medium_01": 0.996, "medium_02": 1.014,
        "max_00": 0.966, "max_01": 0.944, "max_02": 0.943,
    },
    "D10": {
        "length_ratio_integer_00": 0.903, "length_ratio_integer_01": 0.893,
        "length_ratio_integer_02": 0.926, "length_ratio_integer_03": 0.955,
        "length_ratio_integer_04": 0.985, "length_ratio_integer_05": 0.982,
        "a_max_b_random_00": 0.913, "a_max_b_random_01": 0.994,
        "a_max_b_random_02": 0.976,
        "r_nearly_zero_00": 0.994, "r_nearly_zero_01": 1.002,
        "r_nearly_zero_02": 1.001,
        "burnikel_ziegler_bound_00": 0.991, "burnikel_ziegler_bound_01": 1.006,
        "burnikel_ziegler_bound_02": 1.003, "burnikel_ziegler_bound_03": 1.004,
        "large_00": 0.994, "large_01": 0.993,
        "medium_01": 1.007, "medium_02": 1.002,
        "max_00": 0.937, "max_01": 0.956, "max_02": 0.863,
    },
}

for name, ratio in RATIO.items():
    rows = [(c, t, ratio.get(c), t * ratio[c] if c in ratio else None)
            for c, t in LC_D4.items()]
    rows.sort(key=lambda x: -(x[3] if x[3] is not None else x[1]))
    print(f"===== {name} =====")
    print(f"{'case':30s} {'D4(LC)':>7s} {'ratio':>7s} {'pred':>7s}")
    print("-" * 56)
    for case, t, r, p in rows[:6]:
        rs = f"{r:.3f}" if r else "  n/a"
        ps = f"{p:.1f}" if p else f"{t:.1f}*"
        print(f"{case:30s} {t:7d} {rs:>7s} {ps:>7s}")
    best = max((x for x in rows if x[3] is not None), key=lambda x: x[3])
    print(f"  -> 预测最慢 = {best[3]:.1f}ms ({best[0]})"
          f"   收益 ≈ {max(LC_D4.values()) - best[3]:.1f}ms\n")

print(f"D4 实测最慢 = {max(LC_D4.values())}ms (length_ratio_integer_02, LC #390120)")
