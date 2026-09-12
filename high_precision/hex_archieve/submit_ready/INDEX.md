# HEX 三题 —— 提交就绪清单

> 状态：**三题全部击败当前榜一，等待原主人决策提交**（铁律 #2：绝不自交）
> 验收：全部在 VM（`10.144.33.157`，g++ 15.2.0）跑；编译参数 `-O2 -std=c++23 -DEVAL -DONLINE_JUDGE -march=native`
> 测速权威：`perf stat -e instructions:u`，取**最长测试点**（= LC 计分口径）

## 1. 战果

| 题 | 文件 | 我方 maxIr | 榜一 maxIr | **倍率** | 我方 maxCyc | 榜一 maxCyc | 官方点 |
|---|---|---|---|---|---|---|---|
| addition_of_hex_big_integers | `add.cpp` | 26.1M @small_00 | 86.9M @small_00 | **3.33×** | 19.5M (6.7ms) | 41.7M (11.1ms) | 24/24 OK |
| multiplication_of_hex_big_integers | `mul.cpp` | **131.3M** @max_max_05 | 243.2M @small_00 | **1.85×** | 74.9M (25.2ms) | 115.1M (35.7ms) | 22/22 OK |
| division_of_hex_big_integers | `div.cpp` | 634.2M @length_ratio_integer_02 | 1395.0M @small_00 | **2.20×** | 211.8M (59.8ms) | 409.0M (102.3ms) | 27/27 OK |

## 2. 合规检查（铁律 #3）

- 三份源码均 **不含** `#pragma GCC optimize` ✅
- 仅使用 `#pragma GCC target("avx2,...")`（LC = AMD EPYC Milan / Zen3，AVX2 可用）✅
- ⚠️ 历史坑：`work/add/BEST_v2.cpp` 曾带 `#pragma GCC optimize("O3,unroll-loops")`，会 CE。已移除，代价仅 **+1.7% Ir**（25.7M → 26.1M），仍 3.33×。

## 3. 正确性

- 官方 gen 数据逐字节比对：add 24/24、mul 22/22、div 27/27 全 OK。
- **div 额外 100 轮随机+边界暴力对拍**（`tools/stress_div.py`，oracle = `python int(s,16)`）：`rounds=100 bad=0 ALL OK`。
  覆盖 BZ 路径 / Barrett 路径 / Knuth D 短商路径 / nb 边界（BZ_CUTOFF、BZ_MIN、128/256/512/1024）。
  → 满足 INVITATION 天坑 #4「除法先对拍满 100 轮再谈速度」。
- **mul 额外 100 轮随机+边界暴力对拍**（`tools/stress_mul.py`）：`rounds=100 bad=0 ALL OK`。
  覆盖 128×128 快路径 / `mul_bf`（mb≤48）/ `mul_fft`、`pick_k` 全部档位边界（304/1152/4352/16384）、
  FFT_LEAF_LOG=11 递归边界、radix-2² 残层、`split_b2` gather 尾巴、正负号 / 零 / 前导零。
- **mul 满规模精度压测**（`tools/maxprec_mul.py`，6 组 3.2MB 极端模式：全 F / 交替 / 随机 / 稀疏 / 平方）：
  `MAXPREC: ALL OK`。→ 证明 double FFT 在 LC 上限尺寸（ts = 2^19 复点，k=14）舍入余量充足。

## 4. div 算法演进（本轮主战场）

| 版本 | 关键改动 | maxIr | vs 榜一 |
|---|---|---|---|
| v2 | Burnikel-Ziegler（修 t 公式 / 死代码 / WORK 容量） | 2066.7M | 慢 32.5% |
| v3 | `KD_QMAX` 短商截断走 Knuth D | 2066.7M | 慢 32.5%（短商点暴跌 190×） |
| v4 | **Barrett 分块**：预计算 V=floor((2^128n−1)/BS)，每块只 2 次 n×n 乘 | 1018.5M | 1.37× |
| v5 | **固定乘数 FFT 复用**：块内 q2/BS 恒定，正变换只做一次 | 955.7M | 1.46× |
| v6 | **Newton 倒数 `invertappr`** 取代 `div_2n_1n` 算 V（~9·M(n) → ~2.5·M(n)） | 945.7M | 1.48× |
| v7 | `BARRETT_T` 3→2（t=2 也走 Barrett） | 634.3M | 2.20× |
| **v8** | t=2 加块长门槛 `BARRETT_NMIN=512`，修 bz_bound 回归 | **634.2M** | **2.20×** |

### 决定性实测（不靠推理）
把 V 预计算故意跑两遍，测出 **V 占 v5 总开销 ~70%**（length_ratio_integer_01: 955.7M → 1619.7M）。
→ 锁定「V 预计算」为唯一瓶颈，直接上 Newton 倒数，而不是继续调参
（此前 `BZ_CUTOFF × MULBF_MAX` 20 组合扫描，最优仅 +1%，证明调参无效）。

### `invertappr` 关键设计（正确性要害）
- 递归精度取 `h = n/2 + 1`（**不是 n/2**）：保证 `2h ≥ n+1`，Newton 误差平方后 < 1 limb，否则误差会自乘发散。
- 递归返回值先 `−4` 再放大：保证 `E = B^(2n) − d·X ≥ 0`，全程无符号运算。
- 全部取 floor：数学上 Newton 倒数**恒从下方收敛**（`1/D − X_{k+1} = D(1/D − X_k)² ≥ 0`），
  故 V 必为**下估** → Barrett 的 qhat 只会偏小、绝不偏大 → 只需现有「减 BS」修正循环，不会算负。
  这正是 INVITATION 天坑 #4（qhat 偏小连锁错）的结构性免疫。

## 4b. mul 算法演进（本轮第二战场）

先用 `tools/probe_mul.py` 做**阶梯式 Ir 拆解**（不靠猜）：max_max_00 上
IO 8.4M(4.5%) / split+merge 25.8M(13.8%) / 正变换×2 96.5M(51.5%) / pointwise 14.5M(7.7%) / 逆变换×1 42.0M(22.4%)
→ **三次变换合计 138.5M = 74%**，锁定蝶形为主战场。

| 版本 | 关键改动 | maxIr | vs 榜一 |
|---|---|---|---|
| v1 | DEC 榜一 FFT 直接移植（radix-2 DIF/DIT + 实序列打包） | 187.2M | 1.30× |
| v2 | **radix-2²（两层融合蝶形）** 平层版：中间结果留寄存器，load/store 往返减半 | 154.2M | 1.58× |
| v3 | radix-2² 递归版（difRec/ditRec 改四路） | 148.1M | 1.64× |
| v4 | **`split_b2` 改 AVX2 gather**：`_mm256_i32gather_epi32` + 变量移位，替代标量位循环 | 137.7M | 1.77× |
| **v5** | **`pointwise` 改 AVX2**：`cmulv` 一次 2 组复数，低 lane +tw / 高 lane −tw | **131.3M** | **1.85×** |

### 无效尝试（已实测排除）
- `FFT_LEAF_LOG` 扫 9/10/12/13：对 Ir 无实质影响（只动 cache 局部性）。**调参无用，必须改算法。**
- 提高 `pick_k` 的 k：max_max 处 u=200000 limb → coeffs=914286，无论 k=14..17 都落在 lm=2^20，
  要降到 2^19 需 k≥25（double 尾数不够）。**FFT 长度已被锁死，此路不通。**

## 5. 提交须知

- 提交页：`https://judge.yosupo.jp/`，语言选 C++（`-std=c++23`）。
- **未提交**。等原主人下令。回执到手后按铁律 #7 建 `submit_history/<ID>_<版本>_<headline>ms[_BEST].md`。
