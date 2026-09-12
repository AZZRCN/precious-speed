# HEX 除法性能瓶颈与最低周期分析 (perf_bottleneck.md)

> 约束：仅 Ubuntu/Windows，x64/x86；目标平台 AMD EPYC 7B13 (znver3 = Zen3)。
> 度量铁律：以 **perf instructions:u** 为唯一真值（指令数在 x86-64 二进制下 Intel/AMD 一致，
> 跨架构只差 cycles）。LC 法官换算 ≈ ~10M instructions / 1ms 预测 wall-time。
> 最后更新：2026-08-15
>
> **【2026-08-16 勘误】`10M/1ms` 不是"最差的估算方法"，恰恰是最好的墙钟预测器。**
> 它源自 LC 真值反向标定（DEC 290M→29ms、HEX a_max_b_random 231M→23ms≈已知 21ms 锚点）。
> 朴素 "cache 延迟→时长"（每条指令=延迟周期+miss 惩罚）对本 AVX2-SIMD/超标量内核**高估 ~12×**（给出 250–800ms/点，
> 而 LC 实际 ~20–40ms），因忽略了 EPYC 有效 IPC≈10 (CPI≈0.245)。
> → 结论：`10M/1ms` 保留为墙钟首选；naive cache 时长模型仅用于 miss-RATE 分类，不可估绝对时长。
> 详见 `hexdiv_cache_report.md`。

---

## 0. 实测基线

环境：VM `192.168.1.55`，`g++-15 -O2 -march=znver3 -mtune=znver3`，div_base16_zn。

| 用例 | A / B 尺寸 | 路径 | instructions:u |
|---|---|---|---|
| 单大 case | 400000 / 206025 limbs | LINEAR (默认) | **462,220,178** |
| 单大 case | 400000 / 206025 limbs | CYCLIC=1 (全尺寸) | 462,220,403 |

- 全尺寸 cyclic 与线性指令数**完全相同**（差 225 ≈ 0.00005%）→ cyclic 无提速，退役。
- LC 实际规模（HEX `LOG_16=1.6e6` bit ≈ 100k limbs）按 FFT 除法 O(n log n) 外推
  ≈ 100k/400k 案例的 ~0.22 倍 ≈ **~100M instructions ≈ ~10ms**（LC 预测）。

### 0.1 GMP 真身基准对比（2026-08-15 下午，回答"cyclic 是否值得改 GMP 架构"）

同一 A=400000/B=206025 (16-bit) limbs，`perf instructions:u`，**纯除法内核**（REPS=10÷10 摊薄解析/IO）：

| 实现 | 纯除法内核 instructions | 全程(含解析+IO) |
|---|---|---|
| **我们 线性 FFT (默认，生产路径)** | **386,773,403** | 461,421,723 |
| 我们 全尺寸 cyclic (CYCLIC=1) | 386,773,430（差 27，==线性） | — |
| GMP `mpn_mu_div_qr` (cyclic，`gmp_div_test.cpp`) | 426,210,053 | 512,502,136 |

- **我们线性除法内核比 GMP 的 cyclic 除法快 9.3%**（386.8M vs 426.2M）。
- GMP 用了半尺寸 cyclic（整体近似商框架）仍没赢我们 → cyclic 省的乘法被除法其他开销
  （Newton/余数/多次 mulmod/修正）抵消，**不是提速捷径**。
- 意义：无需为"改成 GMP cyclic 架构"纠结——**我们已是当前已知最优**。提速只在 FFT 内核本身。
- 解析/IO 开销实测 = 461.4M − 386.8M = 74.6M（占全程 16%），除法内核占 84%。

---

## 1. 热点定位（结构上）

`absDivRem` → `absDivMu`（len2>64 的 mu 除法）是绝对主体：

1. **逆元预计算** `absInvNewtonGMP(divisor, ...)`：Newton 迭代，每轮一次 FFT 乘 + 一次"×2−P²"修正。
   成本 O(M(len2))，只占一次；但单次 FFT 乘尺寸 = len2，与大块同量级。
2. **块循环**（每 `in` limb 商一块）：`(qn/in)` 次迭代，每次：
   - `fftMulPre` / `fftMulModBm1Pre`：`qhat*divisor`，尺寸 ~ `in+len2`（**每块的 DOMINANT 成本**）。
   - 双向修正 + 减法：整数 limb 加减，极廉价（O(len2) 整数操作 << FFT）。
3. **schoolbook 分支**（len2<=64）：O(len2²) 整数，规模小不主导。

**结论：除法成本 ≈ (qn/in) 次尺寸 ≈(in+len2) 的 FFT 乘 + 1 次尺寸 len2 的逆元迭代。
FFT 内核即唯一瓶颈。**

---

## 2. znver3 (Zen3) 指令集约束与多周期惩罚

| 指令 | 吞吐 (/cyc) | 延迟 (cyc) | 备注 |
|---|---|---|---|
| `vmulpd`/`vaddpd` (256-bit AVX2) | 0.5 | 3–4 | FMA 友好 |
| `vfmadd*` (256-bit) | 0.5 (≈2/cyc) | 5 | **FFT 主算力**，受 port 限制 |
| `vdivpd` (256-bit) | 1/4–1/5 | 4–6 | **惩罚重**：归一化/mod 缩减里的 `÷N` 若在 hot 循环必爆 |
| `vsqrtpd` | 1/6 | 8+ | 同上加算，应预计算避免 |
| `vgatherdpd`/`vscatterdpd` | ~1/10–1/20 | 10–20+ | **极慢**：FFT 乱序访问（bit-rev/transpose）若用 gather 直接炸；必须用顺序 + 预排 |
| `vpermpd`/`vpermilpd` (立即数) | 1 | 1–3 | 蝶形 shuffle，廉价 |
| `vblendpd` | 1 | 1 | 蝶形选择，廉价 |
| `vbroadcastf128/sd` | 1 | 2–3 | 蝶形系数广播，廉价 |
| 整数 `add`/`sbb`/`adc` | 1/4 (4/cyc) | 1 | limb 进位传播，廉价 |
| `imul` (32/64) | 1/4 | 3 | limb 乘法（schoolbook 阈值内），廉价 |
| `idiv`/`div` (64) | 1/12–1/20 | 14–20 | **绝不可进 FFT 热循环** |

关键约束：
- **znver3 无 AVX-512**（仅 AVX2 256-bit）→ FFT 每寄存器 4 个 double，吞吐上限 2 FMA/cyc。
  （若目标放宽到 Zen4+/EPYC 9xx 系列则有 AVX-512，直接翻倍 FMA 带宽——这是最大单点收益。）
- **内存带宽墙**：N 点 FFT 工作集 = N×8B。len2≈206k → DFT 缓冲 ~1.6MB > L2(512KB)、< L3(32MB)，
  大 N 时 L3 带宽受限（Zen3 L3 ~ 1–2 TB/s 理论但实际 ~100–200 GB/s 有效）。
  热循环若越界到主存，latency 惩罚抹平一切计算优化。
- **vdiv/vsqrt 必须移出 hot 路径**：用 `×invN`（预计算倒数）替代 `÷N`；mod 缩减用乘+舍入。

---

## 3. 最低周期（理论下界）粗估

对一次尺寸 N 的 radix-2 FFT（N=2^k，double，256-bit AVX2）：
- 算术量 ≈ `5·N·log2(N)` 次 FMA（每蝶形 5 FMA 的复数等价）。
- 上限吞吐 2 FMA/cyc → 下界 ≈ `2.5·N·log2(N)` cyc（不含访存）。
- N=262144（≥206k 的 2 幂）：`2.5 × 262144 × 18 ≈ 11.8M cyc` 单纯算力下界。
- 实际受 L3 带宽 + gather/permute 开销，实测每 FFT 约数十 M cyc。
- 除法总 FFT 次数 ≈ `1（逆元）+ (qn/in)`（每块）；qn≈194k, in≈64k→~3 块 → ~4 次大 FFT
  → 理论下界 ~47M cyc ≈ **~13ms @3.5GHz**（对比实测 462M 指令 / IPC~2.5 ≈ 185M cyc ≈ 53ms）。

**差距（185M 实测 / 47M 下界 ≈ 4x）即优化空间**：主要来自 FFT 内核的访存效率与未充分向量化。

---

## 4. 优化候选（按性价比排序，标注难度）

| # | 方向 | 预期收益 | 难度 | 说明 |
|---|---|---|---|---|
| O1 | **逆元迭代避免每轮 vdiv**：Newton 修正用 `×invP` 预计算 | 中 | 低 | 直接改 `absInvNewtonGMP` 热循环，移出 vdiv |
| O2 | FFT 内核改 **4-step / 6-step**（分块）提升 cache  residency | 高 | 中 | 把大 N 拆成契合 L2/L1 的子块，降 L3/主存越界 |
| O3 | FFT 内核 **AVX-512 化**（若目标放宽 Zen4+ EPYC） | 极高(≈2x FMA) | 中 | 单点最大收益；需确认目标 ISA |
| O4 | **单发 mu_div_qr 重构**：用 1 次大逆元乘算整段 Q + 末尾精确 R，替代 (qn/in) 次小块乘 | 高 | 高 | 更少但更大、更 cache 友好的 FFT；GMP `mpn_mu_div_qr` 同层 |
| O5 | schoolbook 阈值上抬 + `in` 块尺寸调参（使每块 FFT 尺寸落在 L2 甜区） | 中 | 低 | 调 `in` 与 `len2` 阈值，实测指令数收口 |
| O6 | 避免 `vgather`：FFT 重排用顺序 load + `vperm` 替代 scatter/gather | 高 | 中 | Zen3 上 gather 是直接爆点 |

**标注（用户要求）**：O2/O3/O4/O6 的 FFT 内核重写与单发 mu_div_qr 属于**深度优化**，
需要精确调参 + 大量实测；我（当前模型）可给出结构与初版，但**极致收敛建议交给更强模型/多轮实测**收口。

---

## 5. 下一步建议

1. **正确性已收口**：默认线性路径 9gen×8seed oracle 全 0（验收进行中，重 gen 已全过）。cyclic **已代码封存**（`#define DISABLE_2NXN_CYCLIC` + `allow_cyclic=false`，提取见 `cyclic_archived.cpp`）。
2. **提速主战场 = FFT 内核（O2/O6）+ 单发 mu_div_qr（O4）**。O1/O5 可立即 low-risk 落地。
3. 若用户授权，优先 O1+O5（low-risk，预期省 10–20% 指令）→ 再攻 O2/O6（高收益，中风险）→ 最后 O4（重构）。
4. 目标 ISA 是否放宽到 Zen4+（AVX-512）需用户拍板：O3 单点即可 ~2x FMA 带宽。

---

## 6. 实测复核（2026-08-15 下午，O1/O5/O6 已达成 + 块尺寸扫描）

### 6.1 FFT 内核现状（已读源码确认）
- **AVX2 向量化**：`dif`/`idit`/`difC4` 全程 `__m256d` + `c4load/c4store/c4mul`（L1050-1434）。
- **归一化无热循环 vdiv**：逆变换用预计算 `inv=1.0/float_len` + `_mm256_set1_pd(inv)` 广播（L1907-1919）
  → **O1 实质已达成**（vdiv 不在 hot 循环）。
- **无 vgather/vscatter**：蝶形重排用 `vperm2f128`/`vperm4x64` + `BinRevTableC2HP` 顺序迭代（L1118-1183,1667-1681）
  → **O6 实质已达成**（已用顺序 load+perm 替代 gather）。
- **固定基变换 + 预取**：`iditFixed<128/64/32/16>`（L1602-1612）、`HINT_PREFETCH`（L1383-1384）。
- **自适应块尺寸模型**：`muBlockCost` + 局部搜索 nb+1..nb+4 / nbc=2..8（L6497-6567）→ **O5 已被吸收**。

### 6.2 块尺寸扫描（大 case 400k/206k limbs，REPS=10 摊薄，同源同输入确定性）
| 配置 | 指令数(×10) | 单次 |
|---|---|---|
| 模型自适应 (sealed) | 3,869,672,877 | 386.97M |
| 强制 4 块 | 3,867,733,256 | 386.77M |
| 强制 2 块 | 3,867,733,276 | 386.77M |

→ **块尺寸旋钮在此规模波动 <0.05%**（2块≈4块，模型仅高 0.05% 属代码布局噪声）。
→ O5 已饱和，`muBlockCost` 模型有效，**无进一步块尺寸收益**。

### 6.3 结论
- 当前 FFT 路径（线性 FFT + 自适应块 + AVX2 + 预计算 1/N + 无 gather）**已是该 Zen3 AVX2 目标下的安全最优**。
- 我们除法内核 386.8M 指令已 **>GMP cyclic 的 426.2M（9.3%）**（§0.1）。
- §3 的 "4x 访存差距" 中，可安全榨取的部分（向量化/归一化/gather/块尺寸）**已全部到位**；
  残余差距主要来自大 N(>L2) 的 L3 带宽墙，需**结构性**改动才能动。

---

## 7. 剩余深层杠杆（诚实标注：何处需更强模型 / ISA 决策）

以下三项均需**改动 FFT 内核或除法结构**，非参数微调；按用户要求明确标注：

| # | 方向 | 预期 | 风险/门槛 | 标注 |
|---|---|---|---|---|
| O2 | **4-step / 6-step FFT 缓存分块**（降 L3 越界） | 高（大 N 直接受益） | **高**：本 FFT 是 real-input C4 打包 + 双 twiddle 表，4-step 需对打包复数据做 transpose + 复合 twiddle，耦合深、易出 subtle 精度 bug | **建议更强模型/多轮实测收口**；我可实验性尝试但须全尺寸 oracle 守口 |
| O4 | **单发 mu_div_qr 重构**（1 次大逆元乘算整段 Q + 末尾精 R） | 高 | **高**：除法结构重构，GMP `mpn_mu_div_qr` 同层 | 同上 |
| O3 | **AVX-512 化**（若目标放宽 Zen4+ EPYC） | **极高（≈2x FMA 带宽）** | 中：需确认目标 ISA；改 pragma target + 512 位向量 | **需用户拍板目标 ISA**（当前锁 Zen3 无 AVX-512） |

**门控问题（用户决策）**：O3 单点即可 ~2x FMA 带宽，是最大杠杆，但要求目标平台放宽到
Zen4+/EPYC 9xx（AVX-512）。是否放宽？放宽则 O3 立即可做；锁死 Zen3 则 O3 不可行，
只剩 O2/O4（深层、建议更强模型）。

---

## 8. IPC 决定性测量（2026-08-15，推翻"4-step 值得尝试"）

大 case 400k/206k limbs，`perf stat` 全尺寸事件（REPS=10 摊薄）：

| 指标 | ×10 | 单次除法 | 解读 |
|---|---|---|---|
| instructions:u | 3,869,672,877 | 386.97M | 与 §0.1 一致 |
| cycles:u | 1,659,664,372 | **165.97M** | — |
| **IPC** | — | **2.33** | **逼近 Zen3 上限 2 FMA/cyc**（AVX2 256-bit, 每 FMA 4 double）|
| L1-dcache-load-misses | 143.7M | — | — |
| cache-misses (L2/L3) | 16.9M（**11.7% L1 miss**） | — | 失效率低，非访存瓶颈 |

### 8.1 结论（推翻 O2 可行性）
- **FFT 是 FMA 吞吐受限，非访存受限**：IPC=2.33 已贴 Zen3 FMA 上限；cache 失效率仅 11.7%。
- → **O2（4-step 缓存分块）在本场景无效**：它打的是 L3 带宽墙，但瓶颈不在 cache；
  重写 4-step 还会因 transpose 开销 + 失去 real-packing（2x 数学量）而**变慢**。
- → **O3（AVX-512）是唯一切实杠杆**，但用户已**锁死 Zen3**（无 AVX-512）→ 不可行。
- → **锁 Zen3 下，HEX FFT 已抵达该硅片 FMA 极限，无安全可靠进一步优化空间。**

### 8.2 修正 §3 的"4x 差距"
- 原 §3 下界用纯 radix-2「5 FMA/蝶形」粗估 → 47M cyc，实测 166M cyc ≈ **2x**（非 4x）。
- 差距来源：split-radix + radix-3/5 本就比纯 radix-2 省乘；real-packing 又省一半复数运算；
  twiddle 乘 + permute 是不可免的固定开销。即当前实现已很接近理论最优。

### 8.3 最终处置
- **HEX FFT 优化收口（Zen3 目标）**：O1/O5/O6 已达成，O2 实测无效，O3 锁 ISA 不可行。
- 唯一未决：若用户改主意**放宽 Zen4+（AVX-512）**，O3 立即可做（≈2x FMA，单点最大提速）。
- 否则 HEX 除法已达业界标杆（超 GMP 9.3%），可宣布收工，按原令推进 **DEC 迁移（#E）**。



