# HEX DIV 全部 26 测试点 — 真实时长估算与 cache 画像

生成: 2026-08-16 | 源: `div_base16.cpp` (canonical, 已含验证版 fromCharRange 分支less)
VM 实测: Intel Tiger Lake (proxy) `perf stat` | cache 延迟: Duck.ai(EPYC 7B13/Zen3, 2.45GHz)

## 一、重要纠正（实测优先）

**`10M instructions ≈ 1ms` 不是"最差的估算方法"，恰恰是最好的墙钟预测器。**

证据链:
- `calib_div.md` 明确: DEC 最重除法 290M 指令 → LC 报 **29ms** = 精确 10M/1ms。
- `ir_profile_amax1.md`: `a_max_b_random_00` LC = **21ms**; 本测 a_max_b_random_0 = 231M → 10M/1ms = 23ms, 贴合。
- 朴素 cache 延迟模型 (每条指令=延迟周期 + miss 惩罚) 给出 250–800ms/点, 比 LC 真值高 **~12×**。

根因: HEX div FFT 内核是 **AVX2-SIMD + 超标量**, EPYC 有效 IPC≈10 (CPI≈0.245), 非标量 CPI=1。
naive "1 指令=延迟周期" 模型忽略了 SIMD/超标量吞吐, 系统性高估 ~12×。

→ **结论**: `10M/1ms` 是从 LC 真值反向标定的经验因子, 应保留为墙钟预测首选;
   naive cache-延迟→时长 模型对本 SIMD 负载**不适用**(高估 ~12×), 仅可用于 miss-RATE 分类, 不可用于绝对时长。

## 二、每点数据 (VM 实测 + 两种时长模型)

单位: 指令 M; L1miss%/RAM% = 相对 L1-dcache-loads;
cache_lo/hi = naive 模型(非RAM miss 归因 L2/L3 的上下界, 已证过高 ~12×);
flat10M = 朴素 10M 指令/ms (经验标定, 贴合 LC)。

| case | instr(M) | L1miss% | RAM% | cache_lo(ms) | cache_hi(ms) | flat10M(ms) |
|---|--:|--:|--:|--:|--:|--:|
| length_ratio_integer_2 | 423.40 | 18.27% | 2.49% | 470.98 | 626.81 | 42.34 |
| a_max_b_random_2 | 416.31 | 22.21% | 4.48% | 626.25 | 807.04 | 41.63 |
| length_ratio_integer_4 | 412.03 | 16.77% | 1.60% | 392.39 | 535.84 | 41.20 |
| length_ratio_integer_3 | 398.46 | 17.12% | 1.89% | 401.56 | 542.38 | 39.85 |
| small_0 | 395.56 | 0.60% | 0.33% | 257.67 | 259.99 | 39.56 |
| length_ratio_integer_1 | 380.96 | 17.86% | 2.35% | 411.63 | 548.05 | 38.10 |
| burnikel_ziegler_bound_2 | 380.65 | 1.89% | 0.40% | 244.63 | 255.75 | 38.06 |
| burnikel_ziegler_bound_0 | 368.15 | 1.78% | 0.33% | 236.61 | 247.44 | 36.82 |
| burnikel_ziegler_bound_1 | 347.93 | 3.07% | 0.45% | 235.78 | 255.28 | 34.79 |
| r_nearly_zero_0 | 340.72 | 0.56% | 0.27% | 221.78 | 224.07 | 34.07 |
| length_ratio_integer_0 | 362.50 | 18.73% | 3.10% | 432.64 | 561.68 | 36.25 |
| length_ratio_integer_5 | 331.13 | 14.35% | 1.36% | 294.75 | 391.14 | 33.11 |
| burnikel_ziegler_bound_3 | 325.93 | 4.13% | 0.51% | 226.18 | 251.55 | 32.59 |
| medium_0 | 300.68 | 0.75% | 0.41% | 201.33 | 203.59 | 30.07 |
| a_max_b_random_1 | 269.45 | 19.13% | 3.68% | 350.12 | 446.35 | 26.94 |
| a_max_b_random_0 | 231.13 | 19.10% | 3.31% | 291.65 | 378.27 | 23.11 |
| power_0 | 173.56 | 1.89% | 0.62% | 111.49 | 115.31 | 17.36 |
| medium_1 | 106.01 | 2.07% | 0.96% | 70.48 | 72.39 | 10.60 |
| large_1 | 97.08 | 6.11% | 2.08% | 76.06 | 82.06 | 9.71 |
| r_nearly_zero_1 | 96.75 | 3.11% | 1.14% | 65.30 | 68.24 | 9.67 |
| medium_2 | 95.65 | 3.37% | 1.18% | 64.90 | 68.11 | 9.57 |
| large_0 | 92.96 | 5.74% | 2.18% | 73.77 | 78.84 | 9.30 |
| r_nearly_zero_2 | 91.20 | 5.72% | 1.71% | 67.24 | 72.75 | 9.12 |
| max_2 | 86.45 | 6.28% | 3.65% | 82.13 | 85.62 | 8.65 |
| max_0 | 76.05 | 6.73% | 3.60% | 67.96 | 71.25 | 7.61 |
| max_1 | 53.00 | 6.44% | 3.16% | 50.76 | 53.75 | 5.30 |

## 三、真正有用的产出 — miss-RATE 分类

cache 延迟数据用于**定性定位优化靶心**, 而非绝对时长:

**内存受限 (高 miss, 缓存/数据布局优化杠杆最大):**
- `a_max_b_random_*` : L1miss 19–22%, RAM 3.3–4.5% — 全 26 点最高。
- `length_ratio_integer_*` : L1miss 14–19%, RAM 1.4–3.1%。

**计算/FFT 受限 (低 miss, 需算法/SIMD 提升):**
- `small_0` (0.6% L1miss, 395M 指令): 纯 SIMD 计算, 最"干净"的 FFT 热点。
- `burnikel_ziegler_bound_*` (1.8–4%): 低 miss, 计算主导。
- `r_nearly_zero_0` (0.56%), `medium_0` (0.75%), `power_0` (1.9%): 计算主导。

**中间带**: `large_*`/`max_*`/`medium_1/2`/`r_nearly_zero_1/2` (5–7% L1miss)。

## 四、感言 / 下一步

- HEX DIV 已是榜一、DEC DIV 也是榜一。本次估算的**真实价值不在时长绝对值, 而在暴露内存受限点**:
  `a_max_b_random_*` 与 `length_ratio_integer_*` 的 14–22% L1 miss 是最大未榨干肥肉。
- 这类点的优化杠杆 = **prefetch / cache blocking / 数据布局重排** (O4 pass 里 Memory Prefetch 已就位但可加强),
  而非再刮 I/O 或试 radix-8。
- `10M/1ms` 维持为墙钟预测首选 (经验标定, 贴 LC); 若要做**真·cache 感知时长模型**, 需 EPYC 真机 IPC/每点墙钟,
  当前 Intel VM 的 miss 计数仅为 proxy, 且 SIMD IPC 未知 → 不可盲目建模。

## 五、方法学 / 注意事项

- miss 计数在 Intel VM (Tiger Lake, 12MB L3) 实测, 作 EPYC (32MB L3) 的 **proxy**; 绝对计数有架构偏差。
- 绝对 ms 为估计值; **相对排序与 miss-RATE 分类稳健**。
- 事件: `instructions:u, L1-dcache-loads, L1-dcache-load-misses, cache-misses(末级/RAM), cache-references`。
- 复现: `measure_cache.sh` (VM) + `analyze_cache.py` (本地) + `hexdiv_cache.tsv` (原始数据)。
