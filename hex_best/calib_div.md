# HEX div 标定报告（calib_div）—— 2026-08-14

## 0. 评测方法论铁律（用户 2026-08-14 硬性指令）

1. **不再上 LC 实测**。本地用 LC 的 GEN（样例生成器）驱动评测。
2. **度量只在 VM Ubuntu（perf `instructions:u`）**。Windows 仅编辑/传输，禁止跑指令计数与墙钟。
3. **一份输入交给两个程序**：DEC GEN 生成整数 → 同整数转 hex（`int(a):X` 大写、去 `0x`）喂 HEX best；两程序都跑，比对**输出正确性**（`Q*B+R==A` 且 DEC/HEX 的 Q,R 完全一致）与**指令数**。
4. **禁止高风险杠杆**（4-step / 半尺寸 / split-radix / short product / Newton 复用 / Hensel 等结构性大改）。只做零/低风险可快速收口改动。
5. **GEN 源**：`D:\library-checker-problems-master\big_integer\*\gen\*.cpp`（真实 LC 生成器）。
6. **划算程度指标** = I/O 字节 ÷ 指令数（raw）；附 I/O 减除变体（ioadj，tiny 档 fit 的 k）与 log 变体。本报告以 **raw** 与 **HEX/DEC 指令数比** 为主结论，ioadj 在 tiny 档因 k-fit 失真是 artifact，仅作参考。

## 1. 框架

- 脚本：`tools/calibrate.py`（VM 上 `~/calib/calibrate.py`），全量后台跑通，产出 `calib/calib.json` + 对比表。
- 编译旗标：`-O3 -std=c++20 -march=znver3 -mtune=znver3 -w`（DEC/HEX BEST 均需 C++20：`std::is_constant_evaluated`）。
- DEC GEN 组（info.toml `number`）：small=1, medium=3, large=2, max=3, a_max_b_random=3, r_nearly_zero=3, length_ratio_integer=6, burnikel_ziegler_bound=4。
- 规模换算：DEC 位宽 ≈ (da+db)·log10(2)·3.322；hex 位宽 ≈ (da+db)·4（同整数）；limb ≈ da/9（DEC 十进制位 ÷ 9 hex 位/limb 近似）。

## 2. 发现并修复的崩溃（关键）

**现象**：`max` 组 seed 1（A=2,000,001 十进制位、B=1）→ HEX best `rc=134` + `*** buffer overflow detected ***`，输出 0 字节；DEC best 正常（2MB 输出）。

**根因**：HEX best 缓冲 `MAXC=100010`（第 80 行，注释「1.6M hex / 16」）对应 **HEX 题目原生上限**（HEX `SUM_OF_CHARACTER_LENGTH=3200002` ⇒ 每数 ≤1.6M hex ⇒ 100010 limb）。但标定用 DEC 的 max（2e6 十进制位 → 1.66M hex → **103,811 limb**）转 hex 喂 HEX，超出 `A/B/Qout`（`MAXC+8=100018`）。`nb==1` 路径 `divrem_1`（div.cpp:1569）向 `Qout` 写满 `na=103811` limb，比缓冲多 1 limb → `_FORTIFY` `memcpy_chk` 触发 abort。

> 这不是 HEX 逻辑 bug，而是「DEC 最大档比 HEX 原生最大档大 ~60K hex」导致标定把**超 HEX 规格**的输入喂了进去。真实 LC 上 HEX 不会收到此类输入，故 HEX 在其题目域内本来正确。

**修复（零算法风险，单常量）**：`MAXC` 100010 → 110000（覆盖 DEC 2e6 位上限 + 余量）。备份：`hex_best/div_pre_MAXCfix.cpp`（本地与 VM 各一份）。

**验证（VM，修复后 `-O3` 二进制）**：
- `max_1`：HEX 输出 1,660,968 B，`pair_ok=True`，与 DEC 完全一致、`Q*B+R==A` 成立，不再崩溃。
- `length_ratio_integer_5`（na≈101,725 > 旧上限，此前靠越界 UB 侥幸通过）：修复后在合法缓冲下仍 `pair_ok=True`。

## 3. 全量对比表（DEC BEST vs HEX BEST，同整数）

`ce = I/O字节/指令数(M)`（raw，越高越划算）；`hex/dec_instr` = 同整数下 HEX 指令数 ÷ DEC 指令数（>1 表示 HEX 更费指令）。
`max_1`、`length_ratio_integer_5` 的 HEX 指令数为修复后实测值。

| group | sd | A_dec位 | B_dec位 | bits | limb | dec_ioKB | dec_IM(M) | dec_ce | hex_ioKB | hex_IM(M) | hex_ce | hex/dec_instr | pair_ok |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| small | 0 | 1 | 1 | 7 | 0 | 8915 | 158.6 | 0.0575 | 8675 | 112.0 | 0.0775 | 0.706 | True |
| medium | 0 | 9 | 7 | 53 | 1 | 8128 | 99.8 | 0.0814 | 7255 | 62.3 | 0.1165 | 0.624 | True |
| medium | 1 | 296 | 261 | 1850 | 33 | 6656 | 146.8 | 0.0453 | 5546 | 28.3 | 0.1957 | 0.193 | True |
| medium | 2 | 9365 | 9108 | 61366 | 1040 | 6548 | 200.7 | 0.0326 | 5438 | 189.0 | 0.0288 | 0.942 | True |
| large | 0 | 82751 | 26258 | 362120 | 9194 | 6327 | 184.9 | 0.0342 | 5255 | 309.5 | 0.0170 | 1.674 | True |
| large | 1 | 93228 | 87533 | 600475 | 10358 | 6595 | 214.7 | 0.0307 | 5477 | 338.7 | 0.0162 | 1.578 | True |
| max | 0 | 2000001 | 2000001 | 13287719 | 222222 | 4000 | 14.3 | 0.2797 | 3322 | 9.3 | 0.3583 | 0.648 | True |
| max | 1 | 2000001 | 1 | 6643863 | 222222 | 4000 | 13.99 | 0.2859 | 3322* | 5.37 | 0.4626* | 0.384* | True |
| max | 2 | 2000000 | 2000001 | 13287716 | 222222 | 6000 | 13.49 | 0.4448 | 4983 | 7.7 | 0.6480 | 0.570 | True |
| a_max_b_random | 0 | 2000000 | 241236 | 7445225 | 222222 | 4241 | 130.0 | 0.0326 | 3522 | 205.3 | 0.0172 | 1.579 | True |
| a_max_b_random | 1 | 2000000 | 617516 | 8695200 | 222222 | 4618 | 167.2 | 0.0276 | 3835 | 302.5 | 0.0127 | 1.810 | True |
| a_max_b_random | 2 | 2000000 | 824100 | 9381457 | 222222 | 4824 | 161.9 | 0.0298 | 4006 | 307.6 | 0.0130 | 1.900 | True |
| r_nearly_zero | 0 | 7 | 5 | 40 | 0 | 7717 | 95.7 | 0.0806 | 6928 | 62.0 | 0.1118 | 0.647 | True |
| r_nearly_zero | 1 | 820 | 532 | 4491 | 91 | 5824 | 220.1 | 0.0265 | 4843 | 37.3 | 0.1298 | 0.169 | True |
| r_nearly_zero | 2 | 88965 | 31557 | 400365 | 9885 | 5539 | 189.2 | 0.0293 | 4600 | 317.4 | 0.0145 | 1.677 | True |
| length_ratio_integer | 0 | 1333334 | 666667 | 6643860 | 148148 | 6667 | 222.6 | 0.0299 | 5537 | 400.5 | 0.0138 | 1.799 | True |
| length_ratio_integer | 1 | 1500000 | 500000 | 6643856 | 166666 | 7000 | 219.7 | 0.0319 | 5813 | 354.5 | 0.0164 | 1.613 | True |
| length_ratio_integer | 2 | 1666665 | 333333 | 6643850 | 185185 | 7333 | 253.3 | 0.0290 | 6090 | 358.3 | 0.0170 | 1.415 | True |
| length_ratio_integer | 3 | 1818180 | 181818 | 6643850 | 202020 | 7636 | 238.3 | 0.0320 | 6342 | 342.3 | 0.0185 | 1.437 | True |
| length_ratio_integer | 4 | 1904760 | 95238 | 6643850 | 211640 | 7809 | 221.6 | 0.0352 | 6486 | 318.3 | 0.0204 | 1.436 | True |
| length_ratio_integer | 5 | 1960750 | 39215 | 6643740 | 217861 | 7921 | 251.3 | 0.0315 | 6579 | 267.4 | 0.0246 | 1.064 | True |
| burnikel_ziegler_bound | 0 | 5 | 2 | 23 | 0 | 6671 | 228.5 | 0.0292 | 5542 | 133.5 | 0.0415 | 0.584 | True |
| burnikel_ziegler_bound | 1 | 37 | 18 | 183 | 4 | 6655 | 216.4 | 0.0308 | 5528 | 258.3 | 0.0214 | 1.194 | True |
| burnikel_ziegler_bound | 2 | 2 | 1 | 10 | 0 | 6674 | 246.3 | 0.0271 | 5544 | 79.7 | 0.0695 | 0.324 | True |
| burnikel_ziegler_bound | 3 | 39 | 20 | 196 | 4 | 6650 | 214.4 | 0.0310 | 5523 | 168.4 | 0.0328 | 0.785 | True |

\* `max_1` 修复后实测（旧值 3.59M/截断无效）。`hex_ioKB` 为 hex_in+hex_out。

## 4. DEC 29ms 标杆（GEN 驱动）

换算基准：AMD EPYC 7B13 ≈ 10M 指令/ms ⇒ 29ms ≈ **290M 指令**。

GEN 谱系中 DEC 指令数最重的非平凡除法点为 `length_ratio_integer` 组（A:B≈5:1，A 达 1.33M–1.96M 十进制位），峰值：
- **`length_ratio_integer sd2`：dec_instr = 253.3M**（最近 290M），A=1,666,665 位、B=333,333 位、limb=185,185，输入 4,000,002 B / 输出 3,333,333 B。
- sd5 = 251.3M、sd0 = 222.6M、sd4 = 221.6M 次之。

**结论**：本机 VM 实测 DEC 最重非平凡除法 ≈ 253M 指令（≈25ms @ 10M ins/ms）。要达到 LC 报的 29ms（≈290M），需比当前 GEN 顶档略大的输入（≈1.8–2.0M 十进制位、`length_ratio_integer` 顶档规模）。**标杆规模 = `length_ratio_integer` 顶档（A≈1.7–2.0M 位、A:B≈5:1）**，即后续零风险优化要瞄准的「榜样」规模点。

## 5. 划算程度结论（核心交付）

在**大数除法**（large / a_max_b_random / r_nearly_zero / length_ratio_integer / burnikel 大档）上：

- **HEX best 指令数 = DEC best 的 1.4–1.9×**（hex/dec_instr 见上表，large 1.58–1.67、a_max_b_random 1.58–1.90、r_nearly_zero 1.68、length_ratio 1.06–1.80）。
- 对应 raw 划算程度：HEX `ce` ≈ DEC 的 **一半**（large 0.017 vs 0.034；a_max_b_random 0.013–0.017 vs 0.028–0.033；r_nearly_zero 0.0145 vs 0.0293；length_ratio 0.014–0.025 vs 0.029–0.035）。

**释义（已校正，见 §6/§7）**：上表 raw `ce` 与 `hex/dec_instr` 被**基底差异污染**，不能直接读作「HEX 算法有缺陷、可优化」：
- **I/O 字节**：DEC 十进制串对同整数比 HEX 十六进制串长约 1.2×（digits/bit：dec 0.3010 vs hex 0.25），故 DEC 的 raw `ce` 分母天然偏大，显得更「划算」——这部分差距是基底使然，非算法。
- **指令数**：HEX 内部 base-2⁶⁴ limb（u64），DEC 内部 base-2¹⁶ limb（u16）。前者 FFT 必须 `split_b2` 拆 32-bit + radix-5 顶层 + 较重频域点乘；后者 16-bit 点乘在 double 内精确、走 radix-4。同整数下 HEX FFT 元素数仅 DEC 一半却多花 1.4× 指令（每元素 ≈2.8× 贵）。这是**基底产物**，非算法缺陷。
- 因此 HEX/DEC 指令比 1.4–1.9× 主要源于内部基底不同，**在零风险微调下无法缩小**（实证见 §7）。

小 B / 退化档（max sd0 A=B、burnikel sd2 B=1、medium sd1）HEX 反而更省或持平——这些不是优化重点。

## 6. 零风险调优结论（实证，2026-08-14 下午）

在「禁高风险杠杆、仅零风险档位/常数微调」约束下，对标杆规模 `length_ratio_integer` 顶档（lri2，≈253M 指令、A:B≈5:1）及多个大数点做假设-验证：

- **knuthD 并非热点**：perf 显示 lri2 上 `knuthD` 仅占 **1.36%**，长除法实际走 **BZ（Burnikel-Ziegler）+ FFT 乘法引擎**（FFT 占 HEX 总指令 **~89%**、DEC **~81%**）。原 §5「靠短除/档位缩小差距」的假设不成立——差距在 FFT 引擎，不在 Knuth D。
- 两个零风险旋钮实测均无可冻结改动（详见 §7）：禁用 radix-3/5 混合档回归 7–26%；`BZ_CUTOFF` 扫值在 lri2 无变化、large_1 仅 ±2.2% 且点间互相抵消。
- **结论**：HEX/DEC 指令比的主因是 base-2⁶⁴ vs base-2¹⁶ 内部基底差异导致的 FFT 每元素开销，**属基底产物，零风险微调无法缩小**。
- 唯一能实质缩小的路径是结构性改造（如 HEX 内部改 16-bit limb / 换 FFT 基底），但属用户禁令的高风险杠杆，本阶段**不做**；保持 `MAXC=110000` 修复（全 GEN `pair_ok=True`），HEX best 当前交付态不变。

## 7. 函数级指令归属与零风险实验明细

### 7.1 perf 函数占比（同整数 lri2，HEX vs DEC）

| 函数 | HEX % | DEC % | 说明 |
|---|---|---|---|
| FFT 蝶形/变换 (difRec/ditRec/difC4/iditC4/radix-3/5) | ~36 (difRec 19.4 + ditRec 8.5 + dif5StageR 5.4 + idit5StageR 2.7) | ~62 (difC4 13.5 + iditC4 15.8 + real_dot_binrev3 16.2 + dif3StageC4 3.5 + dif/iditDispatch 12.7) | DEC radix-4 + 16-bit；HEX 须拆 32-bit + radix-5 |
| 频域点乘 pointwise_mixed | 21.1 | (并入 real_dot) | HEX 32-bit 点乘较重 |
| fm_mul（FFT 驱动） | 16.3 | (dif/iditDispatch ~12.7) | 乘法调度 |
| split_b2（64→32 拆分） | 7.5 | copyU16ToF64 8.1（无拆） | HEX 独有拆分开销 |
| mulg（学校法小乘） | 7.8 | absMul1 1.5 | BZ 内部小乘 |
| knuthD | 1.4 | absDivBasicCore 1.5 | 非热点 |
| put_big / writeTo（输出） | 2.7 | 3.1 | HEX 十六进制输出更省 |
| parse（解析） | (main 内含) | 3.1 | DEC 十进制解析更贵 |

### 7.2 实验 A：`-DV16_NOMIX`（关 radix-3/5 混合档，强制纯 2 幂）

指令数（M，小=好）：

| 点 | cur | nomix | Δ |
|---|---|---|---|
| length_ratio_integer_2 | 358.3 | 450.6 | +25.8% |
| a_max_b_random_1 | 302.5 | 367.9 | +21.7% |
| large_1 | 338.7 | 367.2 | +8.4% |
| r_nearly_zero_2 | 317.4 | 348.9 | +9.9% |
| length_ratio_integer_5 | 267.4 | 267.8 | −0.2% |
| burnikel_ziegler_bound_3 | 168.4 | 179.9 | +6.8% |

8 点 `pair_ok=True`。**结论：当前 `fft_ceil_tiers`（radix-5 省 37.5% FFT 长度）更优，禁用即回归，不冻。**

### 7.3 实验 B：`BZ_CUTOFF` 扫值（48/80/96/128，当前 64）

指令数（M）：lri2 全部 ≈358.3（无变化）；a_max_b_random_1 cur 302.5 / bz48 308.5(+2%) / 80·96·128 ≈302.5；large_1 cur 338.7 / bz48 331.2(−2.2%) / bz80 338.5 / bz96 341.8(+1%) / bz128 364.0(+7.5%)。8 点 `pair_ok=True`。

**结论：无统一胜者（lri2 免疫、large_1 在 48 时 −2.2% 但 a_max_b_random_1 +2%、128 时 large_1 +7.5% 回归），64 已均衡，不冻。**

### 7.4 收口

两项零风险实验均验证 HEX best 当前交付态（`MAXC=110000`、radix-5 tier、`BZ_CUTOFF=64`、`FFT_LEAF_LOG=9`）在其题目域内与同整数对标下**无回归且已近最优**；HEX/DEC 指令比差距是 base-2⁶⁴ vs base-2¹⁶ 基底产物，非可调缺陷。
