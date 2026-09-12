#!/usr/bin/env python3
"""DIV 本地计时标杆（VM，符合新测量纪律：白天热损耗低，绝对耗时可信）。

用法: python3 bench_div.py <bin> [reps] [warm]
- 跑 ~/lcp/.../division_of_big_integers/in/ 下全部 26 个官方用例
- 每用例: warm 次预热(丢弃) + reps 次计时, 取中位数 wall-clock (ms)
- LC 计分 = 各用例最大值 (max), 与 LC "Time" 列对齐
- 内嵌 D4 #390120 的 LC 逐点(ms) 作参照, 打印本地 vs LC 的逐点差与相关性

注意: VM(Tiger Lake) != LC(Zen3) 微架构, 绝对 ms 不会相等, 但:
  - 本地 MAX 用例应与 LC 一致 (length_ratio_integer_02)
  - 候选 vs D4 的逐点比值在 VM 上稳定, 用于快速筛选
"""
import sys, os, subprocess, time, glob, statistics

BIN = sys.argv[1] if len(sys.argv) > 1 else "bin/div_D4"
REPS = int(sys.argv[2]) if len(sys.argv) > 2 else 7
WARM = int(sys.argv[3]) if len(sys.argv) > 3 else 3
INDIR = os.path.expanduser("~/lcp/big_integer/division_of_big_integers/in")

# D4 #390120 的 LC 逐点(ms) 作参照
LC_D4 = {
    "example_00": 1, "small_00": 12, "medium_00": 17, "medium_01": 37, "medium_02": 44,
    "large_00": 40, "large_01": 45, "max_00": 12, "max_01": 5, "max_02": 4,
    "a_max_b_random_00": 44, "a_max_b_random_01": 61, "a_max_b_random_02": 71,
    "r_nearly_zero_00": 16, "r_nearly_zero_01": 69, "r_nearly_zero_02": 41,
    "length_ratio_integer_00": 68, "length_ratio_integer_01": 66, "length_ratio_integer_02": 75,
    "length_ratio_integer_03": 63, "length_ratio_integer_04": 56, "length_ratio_integer_05": 60,
    "burnikel_ziegler_bound_00": 48, "burnikel_ziegler_bound_01": 42,
    "burnikel_ziegler_bound_02": 53, "burnikel_ziegler_bound_03": 42,
}

cases = sorted(glob.glob(os.path.join(INDIR, "*.in")))
results = {}
for cf in cases:
    name = os.path.basename(cf)[:-3]
    for _ in range(WARM):
        subprocess.run([BIN], stdin=open(cf, "rb"), stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL, check=True)
    times = []
    for _ in range(REPS):
        t0 = time.perf_counter()
        subprocess.run([BIN], stdin=open(cf, "rb"), stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL, check=True)
        times.append((time.perf_counter() - t0) * 1000.0)
    results[name] = statistics.median(times)

score = max(results.values())
print(f"bin={BIN} reps={REPS}  LOCAL score(max)={score:.1f} ms")
print(f"{'case':30s} {'local_ms':>9s} {'LC_D4':>7s} {'diff':>8s}")
for name in sorted(results, key=lambda x: -results[x]):
    loc = results[name]
    lc = LC_D4.get(name, float("nan"))
    diff = (loc - lc) if name in LC_D4 else float("nan")
    print(f"{name:30s} {loc:9.2f} {lc:7.1f} {diff:+8.1f}")

# 相关性 (本地 vs LC 仅对共有用例)
common = [n for n in results if n in LC_D4]
if len(common) > 2:
    xs = [results[n] for n in common]
    ys = [LC_D4[n] for n in common]
    n = len(xs)
    mx, my = sum(xs) / n, sum(ys) / n
    cov = sum((x - mx) * (y - my) for x, y in zip(xs, ys))
    sx = (sum((x - mx) ** 2 for x in xs)) ** 0.5
    sy = (sum((y - my) ** 2 for y in ys)) ** 0.5
    corr = cov / (sx * sy) if sx and sy else float("nan")
    print(f"\n本地 vs LC_D4 逐点相关系数 r = {corr:.3f}")
    print(f"本地 MAX 用例 = {max(results, key=lambda x: results[x])}  (LC MAX = length_ratio_integer_02)")
