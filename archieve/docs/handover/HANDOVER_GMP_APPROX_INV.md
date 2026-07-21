# 交接文档：GMP 近似逆除法优化

## 1. 项目概述

**项目**：precious-speed，冲击 [Library Checker](https://judge.yosupo.jp/) 大整数运算第一名
**GitHub**：https://github.com/AZZRCN/precious-speed
**当前阶段**：Phase 3 深度优化（hint 库统一融合版）
**交接目标**：实现 GMP 风格的"近似逆 + Möller's 分块除法"，预期 DIV 性能再提升 ~30-44%

---

## 2. 当前状态

### 2.1 性能基线（2026-07-17 复跑，10 次中位数）

| case | GMP | best | moptm | moptm/best |
|------|-----|------|-------|-----------|
| 100k/10k | 14.1 | 10.5 | 10.5 | 1.01x |
| 200k/20k | 21.9 | 13.2 | 12.4 | 0.94x |
| 500k/50k | 54.1 | 18.5 | 17.8 | 0.96x |
| 500k/100k | 55.4 | 22.8 | 20.5 | 0.90x |
| 1M/100k | 116.3 | 29.9 | 27.5 | 0.92x |
| **1M/500k** | 125.4 | **38.9** | **37.4** | **0.96x** |
| 1M/900k | 142.8 | 28.8 | 26.7 | 0.93x |
| 1M/999k | 143.8 | 23.4 | 22.4 | 0.96x |
| **LC score** | | **38.9** | **37.4** | **0.96x** |

**结论**：moptm 已超越 best（0.96x），但 GMP 端到端仍有差距（GMP 的字符串转换开销大，实际除法更快）。

### 2.2 当前 moptm.cpp 架构

- **底座**：hint 库（BASE=10^4, uint16_t, radix-4 auto-vec FFT, Core2 分块除法）
- **I/O**：cin 读 + oBuffer 零拷贝写
- **三模式**：`-DHINT_OP_ADD/MUL/DIV` 切换 main
- **编译**：`g++ -O3 -mavx2 -mfma -funroll-loops -DHINT_OP_DIV -o moptm_DIV.exe moptm.cpp`
- **规模**：约 2550 行

---

## 3. GMP 除法算法深度分析

### 3.1 GMP 的除法层级（Skylake x86_64 阈值）

| 算法 | 阈值 | 适用规模 |
|------|------|---------|
| sbpi1_div_qr (schoolbook) | n/a | 小规模 |
| dcpi1_div_qr (divide & conquer) | DC_DIV_QR_THRESHOLD=55 | 中规模 |
| **mu_div_qr (Möller's)** | **MU_DIV_QR_THRESHOLD=1528** | **大规模** |

1M/500k digits 规模（GMP 的 64-bit limb 下约 26k limbs）远超 1528，使用 Möller's 除法。

### 3.2 GMP mu_div_qr 的三大优势

#### 优势 1：近似逆（in < dn）

GMP 只计算 `in` 位近似逆，而非完整 `dn` 位精确逆（[mpn_mu_div_qr.c:175](file:///E:/gmp-6.3.0/mpn/generic/mu_div_qr.c#L175)）：

```c
static mp_size_t mpn_mu_div_qr_choose_in(mp_size_t qn, mp_size_t dn, int k) {
    if (qn > dn) {
        b = (qn - 1) / dn + 1;        // ceil(qn/dn), 块数
        in = (qn - 1) / b + 1;        // ceil(qn/b)
    } else if (3 * qn > dn) {
        in = (qn - 1) / 2 + 1;        // b = 2
    } else {
        in = qn;                       // b = 1
    }
}
```

对 1M/500k（qn = dn = 500k）：`in = ceil(500k/2) = 250k`，**Newton 迭代规模减半**。

#### 优势 2：mulmod B^m-1 环形乘法

GMP 在 Newton 迭代和除法分块中大量使用 `mpn_mulmod_bnm1`（[mulmod_bnm1.c](file:///E:/gmp-6.3.0/mpn/generic/mulmod_bnm1.c)）：

```c
// mulmod B^rn-1 的本质：完整乘法 + 高低半相加
mpn_mul_n(tp, ap, bp, rn);              // 完整乘法
cy = mpn_add_n(rp, tp, tp + rn, rn);    // 高半 + 低半
MPN_INCR_U(rp, rn, cy);                 // 处理进位
```

**FFT 层面的节省**：
- 普通乘法 a(n) * b(n)：FFT 长度 = ceil2(2n)
- mulmod B^n-1：FFT 长度 = ceil2(n)（环形卷积）
- **变换长度减半，每次变换操作量减半**

阈值：`INV_MULMOD_BNM1_THRESHOLD = 50` limbs（Skylake）

#### 优势 3：64-bit limb

GMP BASE=2^64，1M digits ≈ 52k limbs；moptm BASE=10^4，1M digits = 250k limbs。GMP 的 FFT 规模小 4-8 倍。**此项无法借鉴**（改 BASE 需重写整个库）。

### 3.3 GMP Newton 迭代核心（[invertappr.c](file:///E:/gmp-6.3.0/mpn/generic/invertappr.c)）

```c
// mpn_ni_invertappr 的 Newton 步骤
while (1) {
    n = *--sizp;  // 当前目标精度

    // Step 1: xp = dp * inv0（可用 mulmod B^m-1）
    if (BELOW_THRESHOLD(n, INV_MULMOD_BNM1_THRESHOLD) || ...) {
        mpn_mul(xp, dp - n, n, ip - rn, rn);      // 完整乘法
        mpn_add_n(xp + rn, xp + rn, dp - n, n - rn + 1);
        cy = CNST_LIMB(1);
    } else {
        mpn_mulmod_bnm1(xp, mn, dp - n, n, ip - rn, rn, tp);  // 环形乘法
        // 修正环形卷积结果...
    }

    // Step 2: 修正 xp（正/负残类处理，复杂）
    if (xp[n] < CNST_LIMB(2)) { /* positive residue */ }
    else { /* negative residue */ }

    // Step 3: xp = xp_low * inv0（完整乘法，需要 2*rn 位精度）
    mpn_mul_n(xp, xp + 2*n - rn, ip - rn, rn);
    // 加法修正得到新的 inv
}
```

**关键差异**：GMP 的 Newton 拆分为 `inv0 * m`（可用 mulmod）+ `xp_low * inv0`（完整乘法），而 moptm 当前拆分为 `inv0²`（完整平方）+ `tprod * m`（完整乘法）。

### 3.4 GMP 除法主循环（[mu_div_qr.c:266](file:///E:/gmp-6.3.0/mpn/generic/mu_div_qr.c#L266)）

```c
while (qn > 0) {
    if (qn < in) { ip += in - qn; in = qn; }
    np -= in; qp -= in;

    // 用近似逆计算 qhat
    mpn_mul_n(tp, rp + dn - in, ip, in);       // mulhi: qhat = (R_high * inv) >> shift
    cy = mpn_add_n(qp, tp + in, rp + dn - in, in);

    qn -= in;

    // 计算 prod = divisor * qhat（可用 mulmod）
    if (BELOW_THRESHOLD(in, MUL_TO_MULMOD_BNM1_FOR_2NXN_THRESHOLD))
        mpn_mul(tp, dp, dn, qp, in);
    else {
        mpn_mulmod_bnm1(tp, tn, dp, dn, qp, in, scratch_out);
        // 修正...
    }

    // 修正余数和商
    r = rp[dn - in] - tp[dn];
    // 减法 + while 循环修正 qhat（69% 概率 0 次，31% 概率 1 次）
    while (r != 0) { mpn_incr_u(qp, 1); mpn_sub_n(rp, rp, dp, dn); r -= cy; }
    if (mpn_cmp(rp, dp, dn) >= 0) { mpn_incr_u(qp, 1); mpn_sub_n(rp, rp, dp, dn); }
}
```

---

## 4. 近似逆策略：理论收益与实现路径

### 4.1 理论收益估算

对 1M/500k（BASE=10^4，dn=125k limbs）：

| 策略 | Newton 变换量 | 分块变换量 | 总计 |
|------|------------|----------|------|
| moptm 当前（精确逆 dn） | ~24M | ~24M | **~48M** |
| 近似逆（in=dn/2） | ~12M | ~15M | **~27M** |

**理论收益 ~44%**，即使打折扣到 20-30%，DIV 也可能从 37ms 降到 26-30ms。

### 4.2 实现路径

#### 步骤 1：修改 absInvNewton 支持部分精度

当前 [moptm.cpp:1853](file:///d:/precious_speed/moptm.cpp#L1853)：
```cpp
static void absInvNewton(View m, Span inv,
                         const double *m_dft = nullptr, size_t m_dft_float_len = 0)
```

需要增加 `in` 参数（目标精度，in <= m.size）：
```cpp
static void absInvNewtonApprox(View m, Span inv, size_t in,
                               const double *m_dft = nullptr, size_t m_dft_float_len = 0)
```

**关键**：Newton 迭代的递归深度由 `in` 决定，而非 `m.size`。`s = (in - 1) / 2`，递归到 `m + (m.size - in)` 的高 in 位。

#### 步骤 2：实现 absDivMu（Möller's 风格分块除法）

参考 GMP 的 [mpn_preinv_mu_div_qr](file:///E:/gmp-6.3.0/mpn/generic/mu_div_qr.c#L233)，实现：
```cpp
static void absDivMu(Span dividend, View divisor, Span quotient, View inv, size_t in)
{
    // 分块计算商，每块 in 位
    // 每块：
    //   1. qhat = (remainder_high * inv) >> shift  [mulhi]
    //   2. prod = divisor * qhat                    [完整乘法或 mulmod]
    //   3. remainder -= prod
    //   4. while 修正 qhat（误差 ≤ 1）
}
```

#### 步骤 3：在 absDivNewtonCore2 中调用

```cpp
// absDivNewtonCore2 中
size_t in = (len2 + 1) / 2;  // 近似逆精度 = dn/2
absInvNewtonApprox(divisor, inv_span, in, divisor_dft_buf.data(), divisor_float_len);
absDivMu(dividend, divisor, quotient, inv_span, in);
```

### 4.3 风险评估

| 风险 | 等级 | 说明 |
|------|------|------|
| 精度不足 | 高 | BASE=10^4 下 double FFT 精度紧张，近似逆误差可能放大 |
| 修正逻辑复杂 | 中 | GMP 的正/负残类处理复杂，需仔细移植 |
| mulmod 实现 | 中 | 需新增环形卷积函数，FFT 长度减半 |
| 端到端收益打折 | 中 | 理论 44%，实际可能 20-30%（I/O、缓存等开销） |

### 4.4 精度分析（关键）

**当前 moptm 精度**：
- BASE=10^4，limb 最大值 9999
- double FFT：尾数 52 位，精度 ~9×10^15
- 卷积值上限：limb_max² × conv_len = 9999² × 250k ≈ 2.5×10^13（安全）

**近似逆精度**：
- 近似逆误差 ≤ 1（GMP 保证）
- qhat 误差 ≤ 1（由修正逻辑处理）
- 不影响最终精度（修正后精确）

**mulmod 精度**：
- 环形卷积有混叠，中间值可能更大
- 需验证：环形卷积后 limb 值是否仍在 double 精度范围内

---

## 5. 关键代码位置（moptm.cpp，2026-07-17 版本）

### 5.1 FFT 核心结构

| 组件 | 行号 | 说明 |
|------|------|------|
| FFT class | 542 | radix-4 Complex2 SoA auto-vectorize |
| FFT::dif | 556 | decimation in frequency |
| FFT::idit | 585 | decimation in time (inverse) |
| dot_rfft | 754 | base case 点积+bitrev（A 方案障碍） |
| dot_rfftX2 | 791-825 | 高度优化蝶形（4 个 Complex2） |
| real_dot_binrev | 828-896 | base case 点积+bitrev |
| real_dot_binrev2 | 899-917 | 主循环点积（A 方案核心障碍） |
| getSharedFFT | 919-925 | 共享 FFT 对象（已优化） |
| real_conv | 927-941 | 标准流程 dif→dot→idit |

### 5.2 乘除法函数

| 函数 | 行号 | 说明 |
|------|------|------|
| fftMul | 1601 | FFT 乘法（完整卷积） |
| fftSqr | 1639 | FFT 平方（完整卷积） |
| absMul | 1690 | 绝对值乘法（带 carry） |
| absSqr | 1678 | 绝对值平方（带 carry） |
| prepareDFT | 1709 | 预计算 DFT |
| fftMulPre | 1721-1749 | 带预计算 DFT 的乘法（B-1 优化） |
| absInvNewton | 1853-1918 | Newton 迭代求逆（B-1 优化） |
| absDivNewtonWithInv | 1919 | 慢路径除法（用 absMul） |
| absDivNewtonWithInvFast | 1961 | 快路径除法（用 fftMulPre） |
| absDivNewtonCore2 | 2064 | Core2 分块除法主循环 |
| DIV main | 2492-2508 | cin 读 + oBuffer 写 |

### 5.3 已应用的优化

1. **B-1 优化**（已验证生效）：预计算 divisor_dft 传入 absInvNewton 最外层，省 1 次 DFT(m)
2. **共享 FFT**（已验证生效）：3 个独立 static FFT 对象通过 getSharedFFT 共享旋转因子表
3. **blocks>=3 阈值**（已验证）：blocks=2 时快路径预分配缓冲区页错误开销超收益

### 5.4 已尝试但失败的优化

1. **A 方案（3-way 乘法合并）**：不可行。跳过 carry chain 会导致 DFT 中间值指数级增长（10^8→10^17→10^26），IDFT 输出远超 double 精度
2. **B-2 优化（省 carry chain）**：不可行。carry chain 是非线性变换（取模+除法），DFT(原始double) ≠ DFT(carry后)

---

## 6. 测试与验证方法

### 6.1 编译与运行

```bash
cd d:\precious_speed

# 编译工具
g++ -O2 -o gen_data.exe gen_data.cpp
g++ -O2 -o bench.exe bench.cpp

# 一键基准测试（自动编译 moptm/best/gmp + 生成数据 + 3 模式基准）
.\ai.bat div          # 仅 DIV
.\ai.bat all          # ADD+MUL+DIV
.\ai.bat div --rebuild  # 强制重编
.\ai.bat div --runs 15  # 自定义运行次数
```

### 6.2 正确性验证

```bash
# cmp 模式：moptm vs best 逐字节比对 stdout
bench cmp div
bench cmp mul
bench cmp add
```

### 6.3 测试用例

```cpp
// gen_data.cpp 生成的 DIV 用例（覆盖各种长度比）
test_div_100k_10k.txt    // blocks=10
test_div_200k_20k.txt    // blocks=10
test_div_500k_50k.txt    // blocks=10
test_div_500k_100k.txt   // blocks=5
test_div_1M_100k.txt     // blocks=10
test_div_1M_500k.txt     // blocks=2（LC 最慢点）
test_div_1M_900k.txt     // blocks=2
test_div_1M_999k.txt     // blocks=2
```

### 6.4 验证检查清单

实现近似逆后，必须通过以下验证：
1. `bench cmp div` 8/8 用例 PASS（逐字节比对 moptm vs best）
2. `bench div` 性能不退化（目标：1M/500k < 37ms）
3. 无 assert 失败（退出码 3 = assert 失败）

---

## 7. GMP 源码位置（E:盘）

| 文件 | 说明 |
|------|------|
| [E:\gmp-6.3.0\mpn\generic\invertappr.c](file:///E:/gmp-6.3.0/mpn/generic/invertappr.c) | Newton 迭代近似逆核心 |
| [E:\gmp-6.3.0\mpn\generic\invert.c](file:///E:/gmp-6.3.0/mpn/generic/invert.c) | 精确逆（调用 invertappr + 修正） |
| [E:\gmp-6.3.0\mpn\generic\mu_div_qr.c](file:///E:/gmp-6.3.0/mpn/generic/mu_div_qr.c) | Möller's 除法主循环 |
| [E:\gmp-6.3.0\mpn\generic\mulmod_bnm1.c](file:///E:/gmp-6.3.0/mpn/generic/mulmod_bnm1.c) | mulmod B^m-1 环形乘法 |
| [E:\gmp-6.3.0\mpn\x86_64\skylake\gmp-mparam.h](file:///E:/gmp-6.3.0/mpn/x86_64/skylake/gmp-mparam.h) | Skylake 阈值参数 |

---

## 8. 实现建议（给 Claude 模型）

### 8.1 推荐实现顺序

1. **先实现 absInvNewtonApprox**（最小改动）
   - 修改 absInvNewton 增加 `in` 参数
   - 递归深度由 `in` 决定：`s = (in - 1) / 2`，递归处理 `m + (m.size - in)` 的高 in 位
   - 验证：`bench cmp div` 通过

2. **实现 absDivMu**（Möller's 分块）
   - 参考 [mpn_preinv_mu_div_qr](file:///E:/gmp-6.3.0/mpn/generic/mu_div_qr.c#L233)
   - 分块计算商，每块 in 位
   - 修正逻辑：while 循环 ±1 调整 qhat
   - 验证：`bench cmp div` 通过

3. **性能测试**
   - `bench div` 确认 1M/500k 性能提升
   - 若提升 < 10%，检查 in 的选择是否最优

4. **（可选）实现 mulmod B^m-1**
   - 新增 `real_conv_cyclic` 函数
   - 在 absDivMu 的第二次乘法（divisor * qhat）中使用
   - 验证精度后启用

### 8.2 关键注意事项

1. **不要改 BASE**：BASE=10^4 是设计选择，double FFT 精度上限
2. **不要省 carry chain**：A 方案和 B-2 已证明不可行（精度爆炸）
3. **保持 B-1 优化**：最外层 absInvNewton 仍用预计算 divisor_dft
4. **blocks>=3 阈值**：不要降到 blocks>=2（页错误开销）
5. **共享 FFT**：所有新函数用 getSharedFFT<Float>()
6. **thread_local 缓冲区**：所有临时 vector 用 thread_local 避免重复分配
7. **代码注释**：按中文写

### 8.3 精度安全检查

实现后，在所有 assert 处添加精度检查：
```cpp
// 卷积后检查最大值是否在安全范围
double max_val = 0;
for (size_t i = 0; i < conv_len; i++) {
    max_val = std::max(max_val, std::abs(v[i]));
}
// BASE=10^4, double 精度 9e15, 安全上限 ~1e13
assert(max_val < 1e14);
```

---

## 9. 文件结构

```
d:\precious_speed\
├── moptm.cpp                      # 主文件（Phase 3，hint 库统一，~2550 行）
├── bench.cpp                      # C++ 计时基准测试器（v3，~613 行）
├── gen_data.cpp                   # 测试数据生成器（~78 行）
├── ai.bat                         # 一键编译+测试脚本
├── best/                          # LC 第一名提交文件（参考）
│   ├── add.cpp (18ms)
│   ├── mul.cpp (36ms)
│   └── div.cpp (108ms #1)
├── docs/handover/                 # 交接文档
│   └── HANDOVER_GMP_APPROX_INV.md # 本文档
├── gmp_bench.cpp                  # GMP 基准测试
├── hint_all.cpp                   # Phase 2 产物（保留参考）
├── OM.cpp                         # Phase 1 产物（保留参考）
└── HANDOVER.md                    # 旧交接文档（Phase 2 时期，已过时）
```

---

## 10. Git 历史

- `9e79658` Add best LC submissions and hybrid big-integer library
- `173e3c8` Add development files
- `de30b7d` Phase 3: DIV 优化 + C++ 工具链重构（当前 HEAD）

---

*交接文档生成时间：2026-07-17*
*作者：AZZRCN*
