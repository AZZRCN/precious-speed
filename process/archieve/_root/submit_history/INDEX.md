# LeetCode/Library-Checker 提交记录总索引 — Division of Big Integers

> **铁律（用户 2026-08-07 21:21 明令）**
> 1. 用户每粘一次提交回执，**必须立即**在本目录建 `<提交ID>_<版本>_<时间>ms.md` 并更新本索引。**不许拖到最后、不许省略。**
> 2. 同时把回执**原文粘到对应 `best/div_Dxx.cpp` 文件开头**的 `/* ... */` 注释块里。
> 3. **评测机有抖动**：单次提交不能判定优劣，同点位可摆 ±8~10 ms。headline = 各点最大值 ⇒ 抖动会概率性抬高 headline。**结论必须建立在多次提交的分布上。**

## 总表（时间升序）

| 提交 ID | 时间 | 代码 | headline | 备注 |
|---|---|---|---|---|
| #390217 | 08-03 | D10（自适应 split + 负剩余类 + Newton 倒数 cyclic 阶梯） | 69 ms | 破 75 ms |
| #390892 | 08-06 | div_best_tmp_20260806004637.cpp | 35 ms | |
| #391066 | 08-07 | D45（内联汇编 mulx 魔数常驻 RDX + 大页 hparena） | 30 ms | |
| #391163 | 08-07 | D46（+ radix-5 mixed-radix，无限幅） | 37 ms | **真回归**：`length_ratio_integer_04` 大尺寸 5 路分解常数因子恶化 |
| #391165 | 08-07 | D46 重交 | 38 ms | 同点位复现 ⇒ 确认非抖动 |
| **#391180** | **08-07 11:16:23** | **D47 = D46 + `FFT5_MAX=256` 限幅** | **29 ms** | **★ 历史最佳 · 当前纪录持有者** |
| #391243 | 08-07 21:16:02 | D48 = D47 + SO SWAR 进位链恒等式（7 站） | 35 ms | 判定为**抖动**（`lri_03`/`bz_00` +10 但 `a_max_b_02`/`rnz_02` −8，改动物理上不可能致此），**待重交仲裁** |

## 当前状态
- **纪录 = 29 ms（#391180 / `best/div_D47.cpp`）**
- **待仲裁 = D48**（`best/div_D48.cpp`），需重交 1~2 次
- 备份文件：`best/div_best_tmp_20260807111623.cpp` = #391180 原件（含头部回执）

## 29 ms 的 headline 构成（决定下一步优化目标）
`a_max_b_random_02` = 29 · `r_nearly_zero_01` = 29 · `burnikel_ziegler_bound_02` = 29 —— **三点并列**。
⇒ 只压 `r_nearly_zero_01` 单点**不会掉 headline**。必须三点齐降，或找对三者共同的公共热路径下刀。

## 命名约定
`<提交ID>_<版本标签>_<headline>ms[_BEST].md`，例：`391180_D47_29ms_BEST.md`

---

# Multiplication of Big Integers — 提交记录

> 同 DIV 铁律：回执须归档 + 入 `best/mul.cpp` 文件头。评测机抖动 ±8~10 ms，结论须多次提交分布支撑。
> （DIV 归入上半部；MUL 单列于此，避免与 DIV 索引混淆。）

## 总表
| 提交 ID | 时间 | 代码 | headline | 备注 |
|---|---|---|---|---|
| #385663 | 07-15 | gold 原版（基准） | 36 ms | 原榜首 |
| #389886 | 08-01 | B = gold蝶形 + DFS分块(LEAF_LOG=11) + 2 MB 大页 | 25 ms | ★ **当前纪录持有者**（`best/mul.cpp`） |

## 25 ms 的 headline 构成
`max_max_06/07` / `fft_killer_01` / `large_small_00` 各 25 ms 并列顶。⇒ FFT 乘法是瓶颈（fft_killer 系列 24–25 ms）。
大页类在 VM 乐观 ~11%，但 LC 仅 ~2.8% 到头；再降须算法级变更（NTT / 混合基数优化 / 非平衡处理）。

## 命名约定（MUL）
`<提交ID>_mul_<headline>ms.md`，例：`389886_mul_25ms.md`
