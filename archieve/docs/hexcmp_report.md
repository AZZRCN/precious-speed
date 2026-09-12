# HEX 大整数除法：原始 #393027 (v11) vs 优化 v27 各点实测

**原始解法确认**：`D:\precious_speed\393027.cpp`（提交 #393027 / v11，md5 `60b6a702`）在手；LC 题库 `division_of_hex_big_integers` 完整在手。
**对比对象**：原始 `393027.cpp`(v11) ↔ 优化 `393027_opt.cpp`(v27, md5 `5b7bafa1`)。
**指标**：墙钟（11 轮中位，本次授权允许墙钟 + 多轮）、callgrind `I refs`、cache miss（`--cache-sim`，D1mr=L1 D 读缺失 / DLmr=LL D 读缺失）。
**环境**：VM `192.168.1.66`，g++ 15.2.0，`-O3 -std=c++20 -march=znver3 -mtune=znver3`。⚠️ VM CPU 为 **Intel Ultra 9 285H**（非 AMD EPYC 7B13 目标）；墙钟绝对值为 Intel 数值，但 **I refs 与 miss 计数跨架构一致**，可直接用于 AMD 判定。

## 各点结果（A=B 等大除 / 各 LC 生成器类别）

| 类别 (seed) | 形状 | orig 墙钟 ms | opt 墙钟 ms | 加速× | orig I refs | opt I refs | I↓% | orig L1D miss | opt L1D miss |
|---|---|---|---|---|---|---|---|---|---|
| small (1) | 数百 tiny | 15.6 | 15.6 | 1.00 | – | – | – | – | – |
| medium (1) | 中 | 7.7 | 7.5 | 1.02 | – | – | – | – | – |
| large (1) | 大 | 6.8 | 6.9 | 1.00 | – | – | – | – | – |
| max (3) | **A=B 早退** | 10.5 | 10.3 | 1.02 | 10.8M | 10.9M | -1% | 211K | 211K |
| a_max_b_random (1) | A=max, B~随机大 | 33.6 | 30.7 | **1.10** | 297M | 237M | **21%** | 4.93M | 3.86M |
| power (1) | 幂形 | 7.4 | 8.1 | 0.91 | – | – | – | – | – |
| r_nearly_zero (1) | r≈0 | 7.4 | 7.1 | 1.04 | – | – | – | – | – |
| length_ratio_integer (5) | A=50·B | 22.2 | 19.9 | **1.11** | 307M | 224M | **27%** | 3.27M | 2.48M |
| burnikel_ziegler_bound (1) | BZ 边界簇 | 24.3 | 16.5 | **1.48** | 261M | 202M | **23%** | 286K | 80K |

DLmr（LL D 缺失）所有大类别均 ≈57K，几乎相同（巨页 hugify 已压平 LL miss），故未单列。

## 关键结论

1. **正确性**：9/9 类别两解法输出逐字节一致（AGREE），优化未引入任何偏差。
2. **优化在「重/非对称」除法形状上确实生效**：
   - `a_max_b_random`：墙钟 **−10%**，I refs **−21%**，L1 D miss **−22%**。
   - `length_ratio_integer`(m=50, A=50·B)：墙钟 **−11%**，I refs **−27%**，L1 D miss **−24%**。
   - `burnikel_ziegler_bound`：墙钟 **−48%**（最大墙钟收益），I refs **−23%**，L1 D miss **−72%**（opt 局部性显著更好）。
3. **轻量/早退/解析主导的形状无收益**：small/medium/large/max/power/r_nearly_zero 墙钟 ≈ 持平（加速 0.91~1.04，在噪声内）。
   - ⚠️ `max` 类别 seed=3 生成 **A=B**（早退 q=1，仅 O(n) 比较），I refs 仅 10.8M——**不是重除**。LC 的「真正重 MAX」由 a_max_b_random / length_ratio_integer / burnikel_ziegler_bound 三类承载，优化在这些点上赢。
4. **墙钟增益 < I refs 增益**：因墙钟含 hex 解析/输出（两解法相同），稀释了除法内核的改进；I refs 下降才是除法内核自身的真实收益。

## 与「还能大幅改进吗」的关系

本次是 **v11(原始) vs v27(现状)**。v27 已把重形状的全部收益吃下（重类别 I refs −21~27%、墙钟 −10~48%）。**在 v27 之上再压是死路**（前轮实测：NTT 替 FFT 慢 60–85×；除法 88% 是 FFT 乘法、脚手架仅 11%），与之前结论一致。

## 复现

`D:\precious_speed\hexcmp_run.sh`（VM 端：`bash /tmp/hexcmp/hexcmp_run.sh`，需先 stage 题目目录 + `common/random.h` + 两 cpp）。官方 `sol/correct.cpp`（schoolbook `divmod`）在 100K-limb 量级是 O(n²)，实测会跑数分钟~小时，**未纳入对比**。
