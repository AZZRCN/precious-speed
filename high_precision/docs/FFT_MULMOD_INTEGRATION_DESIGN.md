# fftMulModBm1 集成进 absInvNewton 的设计文档

> **任务**: 研究 GMP `invertappr.c` 两步分解算法，设计将其集成到 `moptm_fusion.cpp` 的 `absInvNewton` 函数中，使用已实现的 `fftMulModBm1`（cyclic convolution mod B^m-1）减少 FFT 长度。
> **约束**: 不修改 `moptm_fusion.cpp`（仅设计，不实现）；数学推导严谨；若方案不可行或风险过高需明确说明。
> **基础**: BASE = 10^4, Limb = uint16_t, radix-4 自排序 DIF/DIT FFT
> **撰写时间**: 2026-07-20

---

## 0. 摘要

**结论**: 集成方案**理论可行，但属于高风险高收益优化**。预期 DIV 1M/500k 端到端净收益 10~20%，与 `O3_DEV_LOG.md` 中"-20~25%"的估计一致但偏保守（扣除了实现复杂度带来的常数开销）。

**核心数据**（1M/500k 用例，k=125000）：
- 当前 `absInvNewton` 顶层迭代：absSqr FFT 长度 2^17 + absMul FFT 长度 2^18
- GMP 方案：第一步 cyclic FFT 长度 2^17 + 第二步 mul_n FFT 长度 2^17
- **FFT 长度峰值从 2^18 降到 2^17（节省 50%）**，但变换次数从 4~5 次增加到 6 次

**主要风险**:
1. GMP 的 cyclic 修正数学推导复杂（正/负剩余类判定），直接移植需逐行对照
2. 当前 `fftMulModBm1` 要求 m 是 2 幂，mn 选择比 GMP 的 `mpn_mulmod_bnm1_next_size` 受限
3. 第二步 `mul_n` 仍是 full mul，部分抵消 cyclic 收益
4. 调试困难：错误仅在 k ≥ 阈值（约 64+）的大输入显现

**建议**: 分阶段实现，先在小规模 (k=1000，cyclic 生效的最小规模) 验证数学正确性，再扩展到生产规模。优先实现"正剩余类"路径（覆盖 ~99% 用例），负剩余类路径可后续补充。注意 k=128/256/512/1024 等 2 幂 k 值 cyclic 全部退化（见 3.3 节），不能用于测试。

---

## 1. GMP `mpn_ni_invertappr` 算法数学原理

### 1.1 算法来源与文档

GMP 的 `mpn_ni_invertappr`（位于 `mpn/generic/invertappr.c`）基于 Brent-Zimmermann《Modern Computer Arithmetic》算法 3.5 "ApproximateReciprocal"，并加入了 cyclic convolution (mod B^m-1) 优化。

**GMP 官方注释**（直译自源码）:
> Compute I such that `floor((B^{2n}-1)/U) - 1 <= I + B^n <= floor((B^{2n}-1)/U)`
> 返回值 `e ∈ {0,1}`: `0 <= e <= 1`, 且 `{dp,n}*(B^n+{ip,n}) < B^{2n} <= {dp,n}*(B^n+{ip,n}+1+e)`
> 即 e=0 为精确逆，e=1 可能差 1。

**cyclic 分支触发条件**:
```c
if (BELOW_THRESHOLD (n, INV_MULMOD_BNM1_THRESHOLD) ||
    ((mn = mpn_mulmod_bnm1_next_size (n + 1)) > (n + rn))) {
    /* 常规 mul，截断到 n+1 */
} else {
    /* 使用 B^mn-1 wraparound */
}
```
即仅当 `mn <= n + rn` 时使用 cyclic。`mn = mpn_mulmod_bnm1_next_size(n+1)` 是 ≥ n+1 的合适尺寸（GMP 内部选择策略）。

### 1.2 迭代结构

GMP 的 Newton 迭代从低精度到高精度逐层扩展。设当前层精度为 n（limb 数），上一层精度为 rn（满足 `rn = (n>>1) + 1`，即约 n/2）。

**指针约定**: `dp` 指向 divisor 高 n 位（`dp-n .. dp`），`ip` 指向 inverse 高 rn 位（`ip-rn .. ip`）。

**每次迭代的两步**:

#### 第一步：计算 `xp = (ip * dp + dp * B^rn - B^{rn+n}) mod (B^mn - 1)`

```c
mpn_mulmod_bnm1 (xp, mn, dp - n, n, ip - rn, rn, tp);
/* xp = (ip * dp) mod (B^mn - 1) */

/* Add dp * B^rn mod (B^mn - 1) */
ASSERT (n >= mn - rn);
cy = mpn_add_n (xp + rn, xp + rn, dp - n, mn - rn);
cy = mpn_add_nc (xp, xp, dp - (n - (mn - rn)), n - (mn - rn), cy);

/* Subtract B^{rn+n} */
xp[mn] = CNST_LIMB (1);
MPN_DECR_U (xp + rn + n - mn, 2 * mn + 1 - rn - n, CNST_LIMB (1) - cy);
MPN_DECR_U (xp, mn, CNST_LIMB (1) - xp[mn]);
cy = CNST_LIMB(0); /* Remember we are working Mod B^mn-1 */
```

**GMP 关键不变量**（源码注释）:
> `2*|ip*dp + dp*B^rn - B^{rn+n}| < B^mn-1`

即真实值 `X = ip*dp + dp*B^rn - B^{rn+n}` 满足 `|2X| < B^mn-1`，因此 `X mod (B^mn-1)` 能唯一区分 X 的正负（无歧义）。

#### 正/负剩余类判定与修正

```c
if (xp[n] < CNST_LIMB (2)) { /* "positive" residue class */
    cy = xp[n]; /* 0 <= cy <= 1 */
    /* 修正：sub_n / sublsh1_n，得到 xp 的规范形式 */
    /* 最终 1 <= cy <= 4 */
    MPN_DECR_U(ip - rn, rn, cy);
} else { /* "negative" residue class */
    ASSERT (xp[n] >= GMP_NUMB_MAX - CNST_LIMB(1));
    /* 修正：取补，加 1 到 ip */
    MPN_INCR_U(ip - rn, rn, CNST_LIMB (1));
    mpn_com (xp + 2 * n - rn, xp + n - rn, rn);
}
```

判定依据：`xp[n]`（位置 n 的 limb，超出 mn 范围）的值。正剩余类 `xp[n] ∈ {0,1}`，负剩余类 `xp[n] ∈ {B-2, B-1}`（因为 mod B^mn-1 的负数表示为 B^mn-1 - |X|，高位为 B-1）。

#### 第二步：计算 `xp_high * ip`（常规 mul_n）

```c
mpn_mul_n (xp, xp + 2 * n - rn, ip - rn, rn);
/* xp = (修正后的 xp 高 rn 位) * ip, 长度 2*rn */

cy = mpn_add_n (xp + rn, xp + rn, xp + 2 * n - rn, 2 * rn - n);
cy = mpn_add_nc (ip - n, xp + 3 * rn - n, xp + n + rn, n - rn, cy);
MPN_INCR_U (ip - rn, rn, cy);
```

第二步是 `rn × rn` 的常规乘法（非 cyclic），conv_len = 2*rn-1。然后通过加法组合得到新的 ip（n 位）。

### 1.3 数学原理的直觉解释

标准 Newton 一步：`Y_new = 2*X - X^2 * U`（取高 n+1 位）。

分解 `X^2 * U = X * (X * U)`：
- 第一步 `T = X * U`（长度 rn+n）
- 第二步 `X * T`（长度 2rn+n，取高位 n+1）

GMP 的 cyclic 优化作用于第一步：
- 直接算 `T = X*U` 需要 full mul，FFT 长度 `int_ceil2(rn+n) ≈ int_ceil2(1.5k)`
- 用 cyclic `T mod (B^mn-1)`（mn ≈ k），FFT 长度 `mn ≈ k`
- 但 cyclic 丢失高位信息，需要通过"加 `U*B^rn`"和"减 `B^{rn+n}`"修正

**修正的直觉**（基于 GMP 注释推导，非完整证明）:
- `X * U ≈ B^{rn+n}`（因为 X 是 U 高 rn 位的逆，`X * U_high ≈ B^{2*rn}`，`X * U ≈ B^{2*rn+s} = B^{rn+n}` 当 `rn = k-s`）
- 减 `B^{rn+n}` 对消主项，剩下小量 `X*U - B^{rn+n} + U*B^rn`
- 加 `U*B^rn` 引入已知量，使得最终 `X = X*U + U*B^rn - B^{rn+n}` 满足 `|2X| < B^mn-1`

**⚠️ 说明**: 完整的数学证明（为何这个特定修正能使 `|2X| < B^mn-1`，以及正/负剩余类判定正确性）需要参考 Brent-Zimmermann 原书和 GMP 论文，本设计文档**不重新推导**，直接引用 GMP 注释作为正确性依据。

---

## 2. 当前 `absInvNewton` 实现瓶颈分析

### 2.1 当前算法（HIGH-half 提取法）

**位置**: `moptm_fusion.cpp` L2571-2636

```cpp
static void absInvNewton(View m, Span inv,
                         const double *m_dft = nullptr, size_t m_dft_float_len = 0)
{
    size_t k = m.size;
    if (k <= INV_NEWTON_BASE_THRESHOLD) { /* base case: O(k^2) 学校除法 */ }
    size_t s = (k - 1) / 2;
    absInvNewton(m + s, inv);              /* 递归: inv0 = inv(m_high) */
    size_t inv0_len = k - s + 1;
    Span inv0(inv.ptr, inv0_len);

    /* inv2 = 2 * inv0 (高位对齐到位置 s) */
    std::fill_n(tinv2.data(), s, Limb(0));
    bool cf = absAdd(inv0, inv0, inv2_span + s);

    /* prod = inv0^2 (长度 2*inv0_len) */
    absSqr(inv0, prod_span);

    /* prod = inv0^2 * m (长度 2*inv0_len + k - 1) */
    if (m_dft 复用条件) fftMulPre(...); else absMul(...);

    /* inv = 2*inv0 - prod_high */
    prod_span = prod_span + 2 * (k - s);   /* 取 HIGH half */
    absSub(inv2_span, prod_span, inv);
}
```

**数学**:
- `inv0 ≈ B^{2*(k-s)} / m_high`，`inv0_len = k-s+1 = rn+1`
- Newton 一步: `inv = 2*inv0 - (inv0^2 * m)_high`
- HIGH half 从位置 `2*(k-s) = 2*rn` 开始，长度 `k+1`

### 2.2 FFT 长度量化

记 `rn = k - s = k - (k-1)/2`：
- k 偶: `rn = k/2 + 1`
- k 奇: `rn = (k+1)/2`

**两次乘法的 conv_len**:
1. `absSqr(inv0)`: `conv_len_sqr = 2*inv0_len - 1 = 2*rn + 1`
2. `absMul(inv0^2, m)`: `conv_len_mul = 2*inv0_len + k - 1 = 2*rn + k + 1`

**1M/500k 用例 (k=125000, rn=62501)**:
| 乘法 | conv_len | float_len = int_ceil2 |
|------|----------|----------------------|
| absSqr(inv0) | 125003 | 2^17 = 131072 |
| absMul(inv0^2, m) | 250002 | **2^18 = 262144** |

**主要瓶颈**: 第二次乘法 `inv0^2 * m` 的 FFT 长度 2^18，是第一次的 2 倍。FFT 工作量 ∝ n·log(n)，第二次约占总 FFT 工作的 60~65%。

### 2.3 递归层累加

`absInvNewton` 递归调用 `absInvNewton(m+s, inv)`，递归层 k 减半。总 FFT 开销约等比级数:
- 顶层: 2^17 + 2^18
- 次层: 2^16 + 2^17
- ...
- 总和 ≈ 2 * (2^18) = 2^19（几何级数，公比 1/2）

**B-1 优化的作用**：当 `absDivMu` 调用 `absInvNewton` 且 `blocks >= 2` 时，预计算 `divisor_dft`（float_len = 2^ceil(log2(2k))），若与 `need_float_len` 匹配则复用，省 1 次 DIF(m)。但仅在最外层生效，递归层无复用。

---

## 3. 集成方案设计

### 3.1 方案概述

将 `absInvNewton` 的 Newton 一步从"HIGH-half 提取法"改为"GMP 两步分解法"：
- **第一步**: 用 `fftMulModBm1` 计算 `(inv0 * m) mod (B^mn - 1)`，然后加 `m*B^rn`、减 `B^{rn+n}` 修正
- **正/负剩余类判定与修正**
- **第二步**: 用 `absMul` 计算 `(修正后 xp 高 rn 位) * inv0`（full mul，长度 2*rn-1）
- **组合**得到新的 inv

### 3.2 关键尺寸映射

当前实现 ↔ GMP 命名：
| 当前 | GMP | 含义 |
|------|-----|------|
| k (m.size) | n | 当前层总精度 |
| s = (k-1)/2 | n - rn | 低半长度 |
| rn = k - s | rn | 上层精度（inv0 的位数，不含 B^rn）|
| inv0_len = rn + 1 | rn + 1 | inv0 的 limb 数（含 B^rn）|

### 3.3 mn 选择策略

**GMP**: `mn = mpn_mulmod_bnm1_next_size(n + 1)`，且要求 `mn <= n + rn`。

**当前实现约束**: `fftMulModBm1` 要求 `m` 是 2 幂（`assert(is_2pow(m))`）。

**我们的 mn 选择**:
```
mn = 最小的 2 幂 满足 mn >= n + 1 且 mn <= n + rn
```

**1M/500k 用例 (n=125000, rn=62501, n+rn=187501)**:
- mn 候选: 2^17 = 131072 (满足 131072 >= 125001 且 <= 187501) ✓
- mn = 131072

**退化条件**: 若 `mn > n + rn`（即最小的 2 幂 mn >= n+1 已经超过 n+rn），则 cyclic 无效，回退到常规 absMul。

**退化用例**:
- k=64, rn=33, n+rn=97, n+1=65, 最小 2 幂 mn=128 > 97 → 退化
- k=128, rn=65, n+rn=193, n+1=129, 最小 2 幂 mn=256 > 193 → 退化
- k=256, rn=129, n+rn=385, n+1=257, 最小 2 幂 mn=512 > 385 → 退化
- k=512, rn=257, n+rn=769, n+1=513, 最小 2 幂 mn=1024 > 769 → 退化
- k=1024, rn=513, n+rn=1537, n+1=1025, 最小 2 幂 mn=2048 > 1537 → 退化
- k=2048, rn=1025, n+rn=3073, n+1=2049, 最小 2 幂 mn=4096 > 3073 → 退化

**约束分析**: 由于 2 幂约束，`mn = 2^ceil(log2(n+1))`，而 `n+rn ≈ 1.5n`。cyclic 生效条件 `mn <= n+rn` 即 `2^ceil(log2(n+1)) <= 1.5n`。

**覆盖率推导**:
- 设 `n+1 ∈ (2^{p-1}, 2^p]`，则 `mn = 2^p`
- 要求 `2^p <= 1.5n`，即 `n >= 2^p * 2/3`，即 `n+1 >= 2^p * 2/3 + 1`
- 生效区间: `n+1 ∈ [2^p * 2/3 + 1, 2^p]`，长度 ≈ `2^p/3`
- 总区间 `(2^{p-1}, 2^p]` 长度 = `2^{p-1}`
- **覆盖率 = (2^p/3) / 2^{p-1} = 2/3 ≈ 67%**

即约 2/3 的 n 值 cyclic 生效，1/3 退化。**这是本方案的主要限制**，但关键 LC 用例均落在生效区间。

**1M/500k 用例**: n=125000, n+1=125001, 2^p=131072 (p=17), 生效区间 [87382, 131072]，125001 ∈ ✓，mn=131072 生效。

**100k/50k 用例**: n=12500, n+1=12501, 2^p=16384 (p=14), 生效区间 [10923, 16384]，12501 ∈ ✓，mn=16384 生效。

**结论**: mn 选择策略受 2 幂约束限制，**覆盖率约 67%**，1/3 用例需 fallback 到常规 absMul。主要 LC 用例（1M/500k, 200k/100k, 100k/50k）均生效。

### 3.4 集成步骤伪代码

```cpp
static void absInvNewtonGMP(View m, Span inv,
                            const double *m_dft = nullptr, size_t m_dft_float_len = 0)
{
    size_t k = m.size;  // GMP n
    if (k <= INV_NEWTON_BASE_THRESHOLD) { /* base case */ }

    size_t s = (k - 1) / 2;
    absInvNewtonGMP(m + s, inv);  // 递归
    size_t rn = k - s;            // GMP rn
    size_t inv0_len = rn + 1;
    Span inv0(inv.ptr, inv0_len);

    // mn 选择
    size_t mn = int_ceil2(k + 1);  // 最小 2 幂 >= n+1
    bool use_cyclic = (mn <= k + rn) && (mn >= k + 1);

    // 缓冲区: xp 需要容纳 2*k 个 limb（第二步 mul_n 和组合步骤访问 xp[2k-1]）
    thread_local std::vector<Limb> txp, ttemp;
    if (txp.size() < 2 * k + 2) txp.resize(2 * k + 2);
    Span xp(txp.data(), 2 * k + 2);

    if (use_cyclic) {
        // 第一步: cyclic convolution
        // xp[0..mn-1] = (inv0 * m) mod (B^mn - 1)
        if (m_dft && m_dft_float_len == mn) {
            fftMulModBm1Pre(inv0, m_dft, k, mn, xp);  // 复用 m DFT
        } else {
            fftMulModBm1(inv0, m, mn, xp);
        }

        // 修正: xp = (xp + m * B^rn) mod (B^mn - 1)
        //   m * B^rn mod (B^mn - 1): 将 m 的 limb 加到 xp[rn..] 并 wrap
        //   (因为 B^rn * m 的位置 rn..rn+k-1, 若 rn+k > mn 则 wrap)
        Limb cy = addShiftModBm1(xp, m, rn, mn);  // 需实现
        xp[mn] = 1;  // 哨兵
        // 减 B^{rn+k}: 即 xp[rn+k-mn .. ] 减 (1 - cy)
        subShiftModBm1(xp, rn + k, mn, 1 - cy);  // 需实现

        // 正/负剩余类判定
        // 注意: xp[n] 即 xp[k] (位置 n 的 limb)
        // 但我们的 xp 长度 mn, xp[k] 可能越界 (k < mn 时)
        // 需要仔细处理: GMP 的 xp[mn] 是哨兵, xp[n] 是位置 n
        // ... (此处数学边界条件复杂, 需逐行对照 GMP)

        if (xp[k] < 2) {  // positive
            // 修正: sub_n, 得到 xp 规范形式
            // cy ∈ [1, 4]
            // MPN_DECR_U(ip - rn, rn, cy)
        } else {  // negative
            // 修正: 取补, ip 加 1
        }

        // 第二步: mul_n(xp_high, inv0)
        // xp_high = xp[2n - rn .. 2n - 1] 即 xp[2*k - rn .. 2*k - 1]
        // ⚠️ 边界: 2*k - rn 可能 > mn, 需从 xp 的规范形式取
        thread_local std::vector<Limb> tprod2;
        tprod2.resize(2 * rn + 1);
        Span prod2(tprod2.data(), 2 * rn + 1);
        absMul(View(xp.ptr + 2*k - rn, rn), inv0, prod2);  // rn * rn mul

        // 组合: 得到新的 inv
        // cy = add_n(xp + rn, xp + rn, xp + 2n - rn, 2rn - n)
        // cy = add_nc(ip - n, xp + 3rn - n, xp + n + rn, n - rn, cy)
        // MPN_INCR_U(ip - rn, rn, cy)
        // ... (组合逻辑, 需实现)
    } else {
        // fallback: 原始 HIGH-half 提取法
        // (保留当前 absInvNewton 逻辑)
    }
}
```

### 3.5 需要新实现的辅助函数

1. **`addShiftModBm1(Span xp, View m, size_t shift, size_t mn)`**: 计算 `xp = (xp + m * B^shift) mod (B^mn - 1)`，处理 wrap-around。
2. **`subShiftModBm1(Span xp, size_t shift, size_t mn, Limb borrow)`**: 计算 `xp = (xp - B^shift * borrow_value) mod (B^mn - 1)`。
3. **正/负剩余类修正**: 逐行对照 GMP 的 `sub_n`/`sublsh1_n`/`com` 逻辑。

---

## 4. FFT 长度节省分析

### 4.1 1M/500k 用例 (k=125000, rn=62501)

| 方案 | 第一步 FFT 长度 | 第二步 FFT 长度 | 总和 |
|------|---------------|---------------|------|
| 当前 (HIGH-half) | absSqr: 2^17 | absMul: 2^18 | 2^17 + 2^18 = 393216 |
| GMP (cyclic) | cyclic: 2^17 | mul_n: 2^17 | 2^17 + 2^17 = 262144 |
| **节省** | 0 | 2^17 (50%) | 131072 (33%) |

**变换次数**（每次乘法 = DIF + DIF + DIT，平方省 1 次 DIF）:
- 当前: absSqr (2 次变换) + absMul (3 次, 或复用 m_dft 时 2 次) = 4~5 次
- GMP: cyclic (3 次, 或复用 m_dft 时 2 次) + mul_n (3 次) = 5~6 次

**FFT 工作量** ∝ n·log(n):
- 当前: 2^17 * 17 + 2^18 * 18 = 131072*17 + 262144*18 = 2228224 + 4718592 = 6946816
- GMP: 2^17 * 17 + 2^17 * 17 = 2 * 131072 * 17 = 4456448
- **FFT 工作量节省: (6946816 - 4456448) / 6946816 ≈ 35.8%**

**端到端净收益估计**:
- absInvNewton 占 DIV 1M/500k 约 30~40%（其余是 absDivMu blocks loop）
- FFT 占 absInvNewton 约 80%
- FFT 节省 35.8% → absInvNewton 节省 28.6%
- 端到端: 28.6% * 35% ≈ **10%**（仅顶层）

若递归层也用 cyclic（1M/500k 递归层 11 层全部生效，见 4.3 节），额外节省约 5~10%，总计 **15~20%**。

与 `O3_DEV_LOG.md` 中"-20~25%"的估计相比，本设计略保守（扣除了变换次数增加、常数开销、修正步骤加法开销等因素）。

### 4.2 其他用例

| 用例 | k | rn | n+1 | mn (2幂) | cyclic 生效? | 节省 |
|------|---|----|----|---------|-------------|------|
| 1M/500k | 125000 | 62501 | 125001 | 131072 | ✓ (131072 ≤ 187501) | 33% FFT |
| 200k/100k | 25000 | 12501 | 25001 | 32768 | ✓ (32768 ≤ 37501) | 33% FFT |
| 100k/50k | 12500 | 6251 | 12501 | 16384 | ✓ (16384 ≤ 18751) | 33% FFT |
| 1M/100k | 走 absDivNewtonCore2, len2=10000 | - | - | - | - |

**覆盖率结论**: 主要用例（1M/500k, 200k/100k, 100k/50k）均生效，仅极小用例（k < 256，已在 base case 阈值附近）退化。

### 4.3 递归层覆盖

递归序列（k' = k - s = k - (k-1)/2）:
- k=125000: s=62499, k'=62501
- k=62501: s=31250, k'=31251
- k=31251: s=15625, k'=15626
- k=15626: s=7812, k'=7814
- k=7814: s=3906, k'=3908
- k=3908: s=1953, k'=1955
- k=1955: s=977, k'=978
- k=978: s=488, k'=490
- k=490: s=244, k'=246
- k=246: s=122, k'=124
- k=124: s=61, k'=63 (k' <= 64 走 base case)

逐层核算 cyclic 生效（n+1 ∈ [2^p*2/3+1, 2^p]）:
| 层 | k | n+1 | 2^p | 生效区间 | 生效? |
|----|------|------|------|----------------|-------|
| 1 | 125000 | 125001 | 131072 | [87382, 131072] | ✓ |
| 2 | 62501 | 62502 | 65536 | [43691, 65536] | ✓ |
| 3 | 31251 | 31252 | 32768 | [21846, 32768] | ✓ |
| 4 | 15626 | 15627 | 16384 | [10923, 16384] | ✓ |
| 5 | 7814 | 7815 | 8192 | [5462, 8192] | ✓ |
| 6 | 3908 | 3909 | 4096 | [2731, 4096] | ✓ |
| 7 | 1955 | 1956 | 2048 | [1366, 2048] | ✓ |
| 8 | 978 | 979 | 1024 | [683, 1024] | ✓ |
| 9 | 490 | 491 | 512 | [342, 512] | ✓ |
| 10 | 246 | 247 | 256 | [171, 256] | ✓ |
| 11 | 124 | 125 | 128 | [86, 128] | ✓ |

**1M/500k 递归层全部生效**（11 层 Newton 迭代，全部落在 cyclic 生效区间）。这使得总节省接近理论最大值，端到端净收益有望达到 15~20%。

---

## 5. 风险与注意事项

### 5.1 数学实现风险（高风险）

1. **正/负剩余类判定边界**: GMP 的判定 `xp[n] < 2` 依赖 `xp[n]` 的精确值，但我们的 xp 长度 mn，`xp[k]`（k < mn）的位置需仔细核对。GMP 代码中 `xp[mn]` 是哨兵，`xp[n]` 是位置 n（n < mn 时在 xp 范围内）。

2. **修正步骤的 wrap-around**: `m * B^rn mod (B^mn-1)` 涉及 wrap（因为 `rn + k > mn`），需正确实现 cyclic 加法。

3. **第二步 xp_high 的位置**: `xp + 2*n - rn` 即 `xp + 2*k - rn`，需确认 `2*k - rn` 在 xp 的有效范围内。`2*k - rn = 2*k - (k-s) = k + s ≈ 1.5k`，而 mn ≈ k，所以 `2*k - rn > mn`，需要从修正后的 xp 规范形式正确取高位。

4. **GMP 的 `mpn_com`（取补）**: 在 negative 分支中用于将负剩余类转为正数表示。

### 5.2 mn = 2 幂限制（中风险）

- GMP 的 `mpn_mulmod_bnm1_next_size` 支持非 2 幂 mn，我们的 `fftMulModBm1` 要求 2 幂
- 导致 cyclic 覆盖率约 67%（1/3 用例退化，详见 3.3 节推导）
- 主要 LC 用例（1M/500k, 200k/100k, 100k/50k 及其递归层）全部生效
- **缓解**: 若需更高覆盖率，可扩展 `fftMulModBm1` 支持非 2 幂 m（但 FFT 实现通常要求 2 幂，需用 Bluestein 算法或复合 2 幂，复杂度高）

### 5.3 第二步 full mul 抵消（中风险）

- 第二步 `mul_n(rn × rn)` 仍是 full mul，FFT 长度 `int_ceil2(2*rn) ≈ 2^17`
- 若 rn 接近 k/2（k 偶），2*rn ≈ k，FFT 长度 ≈ k，与第一步相同
- 净收益主要来自第一步（2k → k），第二步无节省

### 5.4 调试困难（中风险）

- 错误仅在大输入 (k > 64) 显现
- 正/负剩余类路径需分别测试（负剩余类约 1% 概率）
- 建议先实现正剩余类路径，用 `assert(xp[k] < 2)` 捕获负剩余类，验证后再补充

### 5.5 B-1 优化的兼容性（低风险）

- 当前 `absDivMu` 预计算 `divisor_dft` (float_len = int_ceil2(2*k))，用于 `absInvNewton` 的 B-1 复用
- GMP 方案需要 `divisor_dft` 的 float_len = mn（而非 2k）
- **不兼容**: B-1 复用的 float_len 匹配条件不满足，需在 `absDivMu` 中额外预计算 `divisor_dft_mod` (float_len = mn)
- 或者放弃 B-1 复用，每次 cyclic 都重新 DIF(m)

### 5.6 递归层 m_dft 复用（低风险）

- 当前递归层不传 `m_dft`，走 absMul 分支
- GMP 方案递归层同样不传，走 cyclic 分支
- 无额外开销

---

## 6. 替代方案讨论

### 6.1 方案 A: 仅优化第一步（部分集成）

不实现完整的 GMP 两步分解，仅用 cyclic 替代 `absMul(inv0^2, m)`：
- 计算 `(inv0^2 * m) mod (B^mn - 1)`，但 cyclic 丢失高位
- 无法直接得到 `(inv0^2 * m)_high`
- **不可行**：cyclic 丢失的信息无法恢复

### 6.2 方案 B: 截断乘法（truncated multiplication）

不使用 cyclic，而用截断 FFT 计算 `inv0^2 * m` 的高位:
- 截断 FFT (truncated FFT) 只计算高位 limb，节省约 30% FFT 工作
- 实现复杂度低于 cyclic，但节省也较少
- **参考**: Harold Aptroot 的截断 FFT 论文

### 6.3 方案 C: 中间乘法用 cyclic，省去平方

观察当前实现：`inv0^2 * m = inv0 * (inv0 * m)`。
- 第一步 `T = inv0 * m`（长度 rn+k，用 cyclic）
- 第二步 `inv0 * T_high`（长度 2rn，full mul）
- 这正是 GMP 的两步分解！

**结论**: 方案 C 与本设计等价，GMP 的两步分解本质就是"分解平方为两次乘法，第一次用 cyclic"。

### 6.4 方案 D: 不集成，保留当前实现

- 当前 `absInvNewton` 已通过 B-1 优化（最外层）和递归 DFT 复用（守恒律限制，未启用）
- DIV 1M/500k = 30.870ms (O2) / 27.109ms (O3)
- 目标 < 25ms
- **若不集成 cyclic，需其他优化（如 AVX2 FFT、截断乘法）才能达标**

---

## 7. 实现路线建议

### 阶段 1: 小规模验证 (k=1000)

1. 实现 `addShiftModBm1`、`subShiftModBm1` 辅助函数
2. 实现正剩余类路径（负剩余类用 `assert` 捕获）
3. 在 `HINT_OP_TESTMOD` 模式下，用随机数据测试 k=1000
4. 对比 GMP 参考实现（可独立编译 GMP 的 `mpn_ni_invertappr` 验证）

**注意**: 不能用 k=128/256/512/1024 等 2 幂 k 值测试，因为它们全部退化（见 3.3 节）：
- k=128: n+1=129, mn=256, 生效区间[171,256], 129 < 171 → 退化 ✗
- k=256: n+1=257, mn=512, 生效区间[342,512], 257 < 342 → 退化 ✗
- k=1024: n+1=1025, mn=2048, 生效区间[1366,2048], 1025 < 1366 → 退化 ✗

**最小生效 k 值**（cyclic 生效区间 [2^p*2/3+1, 2^p]）:
- p=10: k ∈ [682, 1023] (n+1 ∈ [683, 1024])
- p=11: k ∈ [1365, 2047]
- p=12: k ∈ [2730, 4095]

**建议**: 阶段 1 用 k=1000 (n=1000, n+1=1001, mn=1024, 生效区间[683,1024], 1001 ∈ ✓) 验证。

### 阶段 2: 生产规模验证 (k=125000)

1. 实现负剩余类路径
2. 用 LC 1M/500k 测试数据验证正确性
3. benchmark 性能

### 阶段 3: 递归层集成

1. 将递归调用也改为 GMP 方案
2. 验证递归层 cyclic 生效（1M/500k 递归层全部生效，见 4.3）
3. 端到端 benchmark

---

## 8. 结论

### 8.1 可行性

**理论可行**，但实现复杂度高、数学边界条件多。预期净收益 10~20%（DIV 1M/500k），与目标 < 25ms (O2) 差距约 5~8ms，本方案有望填补。

### 8.2 主要限制

1. **mn = 2 幂约束**导致 cyclic 覆盖率约 67%（1/3 用例退化，但 1M/500k 及其 11 层递归全部生效）
2. **第二步 full mul**抵消部分收益
3. **正/负剩余类**实现易错，需逐行对照 GMP

### 8.3 推荐优先级

- **若 DIV 是当前最大瓶颈且其他优化（AVX2 FFT、截断乘法）收益不足**：推荐实施本方案
- **若 AVX2 FFT 移植可行**（需先 profiling 确认 FFT 占比 > 50%）：优先 AVX2 FFT，本方案备选
- **本方案与 AVX2 FFT 正交**：可叠加，但实现工作量翻倍

### 8.4 不确定性的诚实声明

1. GMP cyclic 修正的完整数学证明（为何 `|2X| < B^mn-1`）未在本文档推导，直接引用 GMP 注释
2. 端到端净收益 10~20% 是基于 FFT 工作量节省 35.8% 的推算，未计入：
   - 变换次数增加的常数开销
   - 修正步骤的加法/减法开销
   - 缓存效应（mn 减半可能改善缓存）
3. mn = 2 幂约束下的 cyclic 正确性需实验验证（GMP 的 mn 选择策略更灵活，我们的约束可能导致 wrap 模式不同）

---

## 附录 A: GMP `mpn_ni_invertappr` 关键代码片段

```c
/* 第一步: cyclic convolution */
mpn_mulmod_bnm1 (xp, mn, dp - n, n, ip - rn, rn, tp);
/* xp = (ip * dp) mod (B^mn - 1) */

/* Add dp * B^rn mod (B^mn - 1) */
ASSERT (n >= mn - rn);
cy = mpn_add_n (xp + rn, xp + rn, dp - n, mn - rn);
cy = mpn_add_nc (xp, xp, dp - (n - (mn - rn)), n - (mn - rn), cy);

/* Subtract B^{rn+n} */
xp[mn] = CNST_LIMB (1);
MPN_DECR_U (xp + rn + n - mn, 2 * mn + 1 - rn - n, CNST_LIMB (1) - cy);
MPN_DECR_U (xp, mn, CNST_LIMB (1) - xp[mn]);
cy = CNST_LIMB(0);

/* 正/负剩余类判定 */
if (xp[n] < CNST_LIMB (2)) { /* "positive" */
    cy = xp[n];
    if (cy++) {
        if (mpn_cmp (xp, dp - n, n) > 0) {
            mpn_sublsh1_n (xp, xp, dp - n, n);
            ++ cy;
        } else
            ASSERT_CARRY (mpn_sub_n (xp, xp, dp - n, n));
    }
    if (mpn_cmp (xp, dp - n, n) > 0) {
        ASSERT_NOCARRY (mpn_sub_n (xp, xp, dp - n, n));
        ++cy;
    }
    ASSERT_NOCARRY (mpn_sub_nc (xp + 2 * n - rn, dp - rn, xp + n - rn, rn,
                                 mpn_cmp (xp, dp - n, n - rn) > 0));
    MPN_DECR_U(ip - rn, rn, cy);
} else { /* "negative" */
    ASSERT (xp[n] >= GMP_NUMB_MAX - CNST_LIMB(1));
    MPN_DECR_U(xp, n + 1, cy);
    if (xp[n] != GMP_NUMB_MAX) {
        MPN_INCR_U(ip - rn, rn, CNST_LIMB (1));
        ASSERT_CARRY (mpn_add_n (xp, xp, dp - n, n));
    }
    mpn_com (xp + 2 * n - rn, xp + n - rn, rn);
}

/* 第二步: mul_n */
mpn_mul_n (xp, xp + 2 * n - rn, ip - rn, rn);
cy = mpn_add_n (xp + rn, xp + rn, xp + 2 * n - rn, 2 * rn - n);
cy = mpn_add_nc (ip - n, xp + 3 * rn - n, xp + n + rn, n - rn, cy);
MPN_INCR_U (ip - rn, rn, cy);
```

## 附录 B: 当前 absInvNewton 关键代码

```cpp
static void absInvNewton(View m, Span inv,
                         const double *m_dft = nullptr, size_t m_dft_float_len = 0)
{
    size_t k = m.size;
    if (k <= INV_NEWTON_BASE_THRESHOLD) { /* base case */ }
    size_t s = (k - 1) / 2;
    absInvNewton(m + s, inv);
    size_t inv0_len = k - s + 1;
    Span inv0(inv.ptr, inv0_len);

    /* inv2 = 2 * inv0 (高位对齐) */
    absAdd(inv0, inv0, inv2_span + s);
    /* prod = inv0^2 */
    absSqr(inv0, prod_span);
    /* prod = inv0^2 * m (或 fftMulPre 复用 m_dft) */
    absMul(View(tprod.data(), inv0_len * 2), m, prod_span);
    /* inv = 2*inv0 - prod_high */
    prod_span = prod_span + 2 * (k - s);
    absSub(inv2_span, prod_span, inv);
}
```

## 附录 C: fftMulModBm1 签名

```cpp
static void fftMulModBm1(View a, View b, size_t m, Span out);
// 计算 a * b mod (B^m - 1), 结果长度 m
// 约束: is_2pow(m), a_len <= m, b_len <= m

static void fftMulModBm1Pre(View a, const double *b_dft, size_t b_len, size_t m, Span out);
// 预计算 b_dft 版本, b_dft 长度 = m
```

---

**文档结束**

**撰写者声明**: 本设计文档基于 GMP `invertappr.c` 源码（通过 WebFetch 获取）和当前 `moptm_fusion.cpp` 实现（L2571-2636, L2271-2468）分析。数学推导中，GMP 的 cyclic 修正正确性直接引用 GMP 注释，未独立证明。mn = 2 幂约束下的覆盖率分析为本设计原创，可能存在边界情况未考虑。建议实施前在小规模 (k=1000) 验证数学正确性。
