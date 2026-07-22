# O3 开发日志

## 2026-07-21 (续3): VM 独占下 GMP cyclic 重新评估 + bug 发现

### VM 独占下重新 benchmark (15 runs × 50 loops)

VM 独占后 (GCC 改造已暂停) 重新验证 absInvNewtonGMP cyclic 路径性能:

| Test | BASE (absInvNewton) | GMP (cyclic) | diff |
|------|---------------------|--------------|------|
| 50k/25k | 1.632 ms | 1.532 ms | **-6.14%** |
| 100k/50k | 2.691 ms | 2.402 ms | **-10.74%** |
| 200k/100k | 4.917 ms | 4.662 ms | **-5.19%** |
| 500k/250k | 10.218 ms | 9.201 ms | **-9.96%** |
| 1M/500k | 19.016 ms | 17.335 ms | **-8.84%** |

**结论**: 之前"无提升"结论是 VM 不稳定时的噪声。VM 独占下 GMP cyclic **确实快 8-10%**。

### Bug 发现: cyclic 精度不足导致错误输出

启用 GMP cyclic 为默认后, 200 组 fuzz 测试发现 3 个 FAIL:

| FAIL # | a 位数 | b 位数 | 现象 |
|--------|--------|--------|------|
| #37 | 50000 | 10000 | default 输出 57991 字节, disable 50003 字节 |
| #106 | 2000 | 664 | default 输出 2891 字节, disable 2002 字节 |
| #164 | 500000 | 250000 | default 输出 500004, disable 500003 (差 1) |

**根因分析**:
- cyclic convolution mod B^mn-1 的精度条件 `|2X| < B^mn-1` 在某些尺寸下不满足
- mn = int_ceil2(k+1) 可能 < k+rn, 导致 cyclic wrap 后精度不足
- xp_mod[k] 落在正剩余类区域 (< 2) 但实际值不正确, 产生错误输出
- 之前的 assert (异常剩余类) 只捕获了部分情况 (#75), 其他情况 (正剩余类但值错误) 未被捕获

**GMP 的 mn 选择**: GMP 用 `mpn_mulmod_bnm1_next_size(n+1)`, 返回 2/4/8 对齐值 (非 2 幂), 比 int_ceil2 更接近 n+1, 更容易满足 mn <= n+rn。本项目用 int_ceil2(k+1) (2 幂), mn 偏大, 但仍可能 < k+rn。

### 修复: 异常剩余类 goto fallback

将 `assert(false)` 改为 `goto gmp_newton_fallback` (安全回退):
```cpp
else
{
    // 异常: 既不是正也不是负剩余类
    is_positive = false;
    goto gmp_newton_fallback;
}
```

但此修复不够: 正剩余类但值错误的情况仍产生错误输出。

### 最终决定: 默认禁用 GMP cyclic

```cpp
#ifdef USE_GMP_NEWTON
    absInvNewtonGMP(divisor + (len2 - in), inv_span);
#else
    absInvNewton(divisor + (len2 - in), inv_span);  // 默认
#endif
```

**保留代码** (USE_GMP_NEWTON 条件编译, 默认关闭), 待修复 cyclic 精度问题后启用。

### 下一步: 修复 cyclic 精度

1. **分析 fftMulModBm1 的精度**: 检查 cyclic wrap 后的进位传播是否正确
2. **改进 mn 选择**: 参考 GMP 的 `mpn_mulmod_bnm1_next_size`, 选择更小的 mn (非 2 幂)
3. **添加正剩余类验证**: cyclic 结果与 fallback 结果对比, 验证正确性
4. **大尺寸阈值**: 只在 k >= 某阈值 (如 50000) 时启用 cyclic, 小尺寸用 absInvNewton

---

## 2026-07-20: absInvNewtonGMP 函数实现 (fallback only)

### 任务
在 `moptm_fusion.cpp` 中实现 `absInvNewtonGMP` 函数 (GMP 风格两步分解 + cyclic convolution)，并添加测试入口。

设计文档: `FFT_MULMOD_INTEGRATION_DESIGN.md`

### 完成内容

#### 1. absInvNewtonGMP 函数 (fallback only)
- **位置**: `moptm_fusion.cpp` L2642-2724 (紧跟 absInvNewton 之后)
- **当前状态**: 仅实现 fallback 路径 (与 absInvNewton 等价的 HIGH-half 提取法)
- **cyclic 路径**: 已计算 `mn = int_ceil2(k+1)` 和 `use_cyclic` 标志，但强制 `use_cyclic = false`，cyclic 分支用 `assert(false)` 捕获并标注 TODO
- **递归**: 调用自身 `absInvNewtonGMP(m + s, inv)` (非 absInvNewton)
- **接口**: `static void absInvNewtonGMP(View m, Span inv, const double *m_dft = nullptr, size_t m_dft_float_len = 0)` (与 absInvNewton 一致)
- **B-1 优化**: 保留 m_dft 复用接口 (float_len 不匹配时不复用，与 absInvNewton 一致)

#### 2. HINT_OP_TESTNEWTON 测试入口
- **位置**: `moptm_fusion.cpp` L3767-3827
- **功能**: 对比 absInvNewton (基线) 和 absInvNewtonGMP (待测) 的输出
- **输入格式**: 第一行 t (组数)，然后每组 2 行 (a 和 b 的十进制字符串)
- **输出**: 每组 PASS/FAIL + 末尾汇总
- **b 归一化要求**: b 的最高 limb >= 5000 (HALF_BASE)，否则 absDivBasicCore 触发 assert

#### 3. 测试数据生成脚本
- **文件**: `gen_testnewton_data.py`
- **固定种子**: seed=42 (可复现)
- **b 归一化**: `rand_digits_normalized()` 确保 b 位数是 4 的倍数且最高位 5-9 (最高 limb >= 5000)
- **直接写文件**: 避免 PowerShell 管道吞 `\n` 问题

#### 4. 测试数据
- **文件**: `testnewton_data.txt` (6 组测试用例)

| 组 | a 位数 | b 位数 | k (limbs) | Newton 层级 | cyclic 生效? | 结果 |
|----|--------|--------|-----------|------------|-------------|------|
| 1  | 100    | 52     | 13        | base case  | -           | PASS |
| 2  | 600    | 300    | 75        | 1 层       | 否 (k<682)  | PASS |
| 3  | 1000   | 500    | 125       | 1 层       | 否          | PASS |
| 4  | 2000   | 1000   | 250       | 2 层       | 是 (k=250)  | PASS |
| 5  | 4000   | 2000   | 500       | 3 层       | 是          | PASS |
| 6  | 10000  | 5000   | 1250      | 4 层       | 是 (k=1250) | PASS |

#### 5. ai.bat
- **文件**: `ai.bat`
- **命令**: 生成数据 → 编译 → 运行测试
- **编译命令**: `g++ -O2 -std=gnu++20 -DHINT_OP_TESTNEWTON moptm_fusion.cpp -o testnewton.exe`
- **编译器**: MinGW g++ 15.2.0 (Windows)

### 编译与测试结果

```
编译: g++ -O2 -std=gnu++20 -DHINT_OP_TESTNEWTON moptm_fusion.cpp -o testnewton.exe
结果: 编译成功 (exit code 0, 无 warning/error)

测试: testnewton.exe < testnewton_data.txt
结果:
PASS k=13 inv_len=14 (a_digits=100 b_digits=52)
PASS k=75 inv_len=76 (a_digits=600 b_digits=300)
PASS k=125 inv_len=126 (a_digits=1000 b_digits=500)
PASS k=250 inv_len=251 (a_digits=2000 b_digits=1000)
PASS k=500 inv_len=501 (a_digits=4000 b_digits=2000)
PASS k=1250 inv_len=1251 (a_digits=10000 b_digits=5000)
=== 6 PASS, 0 FAIL ===
```

### 结论
- absInvNewtonGMP (fallback only) 与 absInvNewton 输出完全一致 (6/6 PASS)
- 覆盖 base case + Newton 1~4 层递归 + cyclic 生效的 k 值
- 编译通过，无 warning

### 下一步计划 (cyclic 路径实现)
1. **实现辅助函数**: `addShiftModBm1` (xp += m*B^shift mod B^mn-1)、`subShiftModBm1` (xp -= B^shift mod B^mn-1)
2. **实现正剩余类路径**:
   - 第一步: `fftMulModBm1(inv0, m, mn, xp)` 计算 (inv0*m) mod (B^mn-1)
   - 修正: xp = (xp + m*B^rn - B^{rn+k}) mod (B^mn-1)
   - 判定: xp[k] < 2 为正剩余类
   - 修正: sub_n/sublsh1_n (对照 GMP 附录 A)
   - 第二步: `absMul(xp_high, inv0, prod2)` (rn × rn full mul)
   - 组合: add_n + add_nc + INCR_U
3. **负剩余类路径**: 用 `assert(false)` 暂时捕获 (约 1% 概率)，后续补充
4. **小规模验证**: k=1000 (mn=1024, cyclic 生效)，对比 fallback 结果
5. **生产规模验证**: k=125000 (1M/500k 用例)

### 风险点 (对照设计文档 5.1 节)
1. 正/负剩余类判定边界: xp[k] 的精确值需仔细核对
2. 修正步骤 wrap-around: m*B^rn mod (B^mn-1) 涉及 cyclic 加法
3. 第二步 xp_high 位置: 2k-rn > mn，需从修正后的 xp 规范形式正确取高位
4. GMP 的 mpn_com (取补): 负剩余类分支需要

---

## 2026-07-20 (续): cyclic 路径阶段 A 验证完成

### 任务
实现 absInvNewtonGMP 的 cyclic 路径 (阶段 A: 验证版本)，验证 fftMulModBm1 + GMP 修正的正确性。

### 关键 bug 修复
**bug**: 首次实现时误将 GMP 源码 L176-L182 的 "加 m*B^rn" 步骤直接翻译，导致 assert "invalid residue class" 触发。

**根因分析**:
- GMP 的 `ip` 是 rn 位小数部分 (不含整数位 B^rn)
- GMP 的 `inv0 = ip + B^rn`，所以 `inv0 * dp = ip*dp + dp*B^rn`
- GMP 源码 `mpn_mulmod_bnm1(xp, mn, dp, n, ip, rn, tp)` 计算 `ip*dp mod B^mn-1`，需要加 `dp*B^rn` 凑成 `inv0*dp`
- **本项目 `inv0` 已是 rn+1 位 (含整数位 B^rn)**，`fftMulModBm1(inv0, m, mn, xp)` 直接计算 `inv0*m mod B^mn-1`
- 因此本项目的修正只需 "减 B^{rn+k}"，**不需要 "加 m*B^rn"**

**修复**: 删除 "加 m*B^rn mod (B^mn-1)" 步骤，保留 "减 B^{rn+k} mod (B^mn-1)"。

### 阶段 A 测试结果 (8 组)

扩展测试数据到 8 组，覆盖 cyclic 生效的 p=7..12 区间:

| 组 | a 位数 | b 位数 | k (limbs) | cyclic 生效? | 结果 |
|----|--------|--------|-----------|-------------|------|
| 1  | 100    | 52     | 13        | - (base)    | PASS |
| 2  | 600    | 300    | 75        | 否 (退化)   | PASS |
| 3  | 1000   | 500    | 125       | 是 (p=7)    | PASS |
| 4  | 2000   | 1000   | 250       | 是 (p=8)    | PASS |
| 5  | 4000   | 2000   | 500       | 是 (p=9)    | PASS |
| 6  | 8000   | 4000   | 1000      | 是 (p=10)   | PASS |
| 7  | 16000  | 8000   | 2000      | 是 (p=11)   | PASS |
| 8  | 32000  | 16000  | 4000      | 是 (p=12)   | PASS |

**结论**: cyclic + 修正 (减 B^{rn+k}) + 正/负剩余类判定逻辑完全正确 (8/8 PASS)。

### 阶段 B (GMP 组合逻辑) 可行性分析

**目标**: 用 cyclic 结果替代 full mul，获得 FFT 长度减少的性能提升。

**GMP 组合逻辑** (invertappr.c L228-L231):
```c
mpn_mul_n (xp, xp + 2*n - rn, ip - rn, rn);       // rn × rn mul
cy = mpn_add_n (xp + rn, xp + rn, xp + 2*n - rn, 2 * rn - n);
cy = mpn_add_nc (ip - n, xp + 3*rn - n, xp + n + rn, n - rn, cy);
MPN_INCR_U (ip - rn, rn, cy);
```

**无法直接翻译的根本原因**:
1. GMP 的 xp 在修正后是 2n 位布局 (位置 0 到 2n-1)，`xp + 2n - rn` 是高 rn 位
2. 本项目 cyclic 结果是 mn 位 (mod B^mn-1)，修正后仍是 mn 位，没有 2n 位布局
3. GMP 的组合逻辑深度依赖 2n 位 xp 布局，本项目无法套用

**替代方案分析 (重构 T + absMul(inv0, T_high))**:
- Newton: `inv_new = 2*inv0 - (inv0 * T) / B^{2*rn}` (高 k+1 位)
- `inv0 * T = inv0 * T_low + (inv0 * T_high) * B^k`
- 近似 `(inv0 * T) / B^{2*rn} ≈ (inv0 * T_high) / B^{2*rn-k}` 忽略 `inv0 * T_low` 高位进位
- **问题**: `(inv0 * T_high) >> (2*rn-k)` 长度仅 k 或 k-1 位，缺少 1-2 位高位 (来自 T_low 进位)
- **修正**: 需计算 `inv0 * T_low` 高 s 位 (s = k-rn)，需 full mul (FFT 长度 ceil2(rn+k) ≈ 1.5k)

**FFT 开销对比** (1M/500k, k=125000):
| 方案 | 第一步 | 第二步 | 第三步 | 总和 |
|------|--------|--------|--------|------|
| 原方案 (absSqr + absMul) | absSqr: 2^17 | absMul: 2^18 | - | 3 * 2^17 (近似) |
| 替代方案 (cyclic + T_high + T_low) | cyclic: 2^17 | absMul(inv0,T_high): 2^17 | absMul(inv0,T_low): 2^17 | 3 * 2^17 |

替代方案 FFT 长度总和与原方案相近，但**变换次数增加** (cyclic 3 次 + absMul 3 次 + absMul 3 次 = 9 次 vs 原 4-5 次)，**实际性能反而下降**。

### 阶段 B 结论 (2026-07-20, 已过时)

~~**不实现阶段 B**。原因:~~
~~1. GMP 组合逻辑无法直接翻译 (布局根本不同)~~
~~2. 替代方案 (重构 T) 需要额外计算 inv0*T_low，抵消 cyclic 收益~~
~~3. 进位误差处理复杂，风险高~~

**注**: 此结论是早期分析, 分析对象是"替代方案(重构 T)", 而非 GMP 原版组合逻辑。代码 L2762-2870 后来实际实现了 GMP 原版组合逻辑 (mul_n + 三步组合), 与 GMP invertappr.c L225-L272 完全一致。

### 阶段 B 重新分析 (2026-07-21)

**search 子代理逐行审查结论**: absInvNewtonGMP cyclic 路径 + GMP 两步分解组合逻辑 (L2678-2870) 与 GMP invertappr.c L225-L272 完全一致, 8 个审查点全部通过, 无 bug。

**FFT 开销重新分析** (k=62500, 1M/500k):
- absInvNewton fallback:
  - absSqr(inv0): FFT len = 65536 (1 forward + 1 IFFT)
  - absMul(inv0^2, m): FFT len = 131072 (2 forward + 1 IFFT)
  - 总: 2×65536 + 3×131072 = 524288
- absInvNewtonGMP cyclic:
  - fftMulModBm1(inv0, m, mn=65536): FFT len = 65536 (2 forward + 1 IFFT)
  - absMul(xp_high, inv0_low): FFT len = 65536 (2 forward + 1 IFFT)
  - 总: 5×65536 = 327680
- **理论节省 37.5%**

**安全性**:
- copy_backward 在 if(is_positive) 块内, 负剩余类回退 fallback 时 inv.ptr 未被修改
- 负剩余类概率 ~1%, 回退 fallback 与 absInvNewton 一致
- use_cyclic=false 时直接走 fallback, 与 absInvNewton 一致

**集成方案**: absDivMu L3153 用 `#ifdef USE_GMP_NEWTON` 条件编译控制 (默认关闭)
- in == len2 路径 (L3149) 暂不修改 (保留 B-1 优化)
- absDivNewtonCore2 路径暂不修改

### 下一步

1. 等 SSH 恢复 (VM 重启后 SSH 服务未启动)
2. VM 测试 HINT_OP_TESTNEWTON (大规模数据 k=62500)
3. PASS 后启用 USE_GMP_NEWTON 编译, benchmark 对比

### VM 测试与 Benchmark 结果 (2026-07-21)

**SSH 修复**: ssh_exec.py 的 HOST 从旧 IP（已移除）更新为 `192.168.1.55`。

**HINT_OP_TESTNEWTON 大规模测试** (3 组, k=25000/50000/62500):
- k=25000 PASS
- k=50000 **FAIL** (INV1=1828 vs INV2=1827, 差 1) — GMP 近似逆固有"少 1"特性
- k=62500 PASS

**absInvNewtonGMP 返回近似逆, 可能比真实逆少 1**。但 absDivMu 的双向修正循环 (L3240 向下 + L3254 向上) 能处理此误差。

**DIV 正确性验证** (USE_GMP_NEWTON 版本 vs 基线):
- 1M/500k: MD5 完全匹配 ✓
- 200k/100k: MD5 完全匹配 ✓

**Benchmark 结果** (50 loops, 5 runs, 中位数, ms):

| 测试 | baseline | GMP_cyclic | 差异 |
|------|----------|------------|------|
| DIV 1M/500k | 14.98 | 14.96 | -0.1% (噪声) |
| DIV 200k/100k | 3.08 | 3.04 | -1.3% (噪声) |

**结论: GMP cyclic 路径无性能提升**。

**理论分析盲点**:
1. absInvNewton 是递归的, 最外层 cyclic 节省被递归内部开销抵消
2. cyclic 路径变换次数 = 6 次 (fftMulModBm1 3 + absMul 3), fallback = 5 次 (absSqr 2 + absMul 3)
   - absSqr 只需 1 forward (平方省 1 次), cyclic 的 fftMulModBm1 需要 2 forward
   - cyclic 多 1 次变换, 虽然 FFT 长度减半, 但 setup 开销增加
3. fftMulModBm1 的 cyclic wrap-around (mod B^mn-1 进位传播) 有额外开销
4. cyclic 路径的组合逻辑 (copy_backward, GMP 修正, mul_n, 三步组合) 有额外开销

**O3_DEV_LOG 早期"阶段 B 不可行"结论的结果是正确的 (虽然理由错误)**。

**保留 USE_GMP_NEWTON 条件编译** (默认关闭), 代码已验证正确但无性能收益。

---

## 2026-07-20 (续): absDivMu profile 测量与优化实验

### 任务
基于 PROFILE_DIV 基础设施, 测量 absDivMu 各阶段准确耗时, 实施并验证优化。

### PROFILE_DIV 基础设施修复

**问题**: 之前用 `fprintf(stderr, ...)` 输出 profile, 在 PowerShell `Start-Process -RedirectStandardError` 下导致 35ms 测量误差 (同步写入开销)。

**修复**: 改用全局 `FILE *_prof_fp` 写入 `prof_detail.log` 文件:
```cpp
#ifdef PROFILE_DIV
#include <cstdio>
static FILE *_prof_fp = nullptr;
static inline void _prof_init() { if (!_prof_fp) _prof_fp = std::fopen("prof_detail.log", "w"); }
#define PROF_PRINT(...) do { _prof_init(); if (_prof_fp) std::fprintf(_prof_fp, __VA_ARGS__); } while(0)
#else
#define PROF_PRINT(...) ((void)0)
#endif
```
全局替换 11 处 `fprintf(stderr,` → `PROF_PRINT(`。误差从 34.6ms 降到 0.6ms。

### 基线 profile 数据 (1M/500k, in=62500=len2/2)

```
absDivMu: absInvNewton+prepareDFT: 10.592 ms (49%)  (in=62500, inv_fl=131072, div_fl=262144)
absDivMu: gap1 (fprintf+resize):      0.732 ms ( 3%)
absDivMu: blocks loop:                10.264 ms (48%)  (qn=125000, in=62500, blocks=2)
absDivMu: total:                      21.588 ms
```

两个方向各占一半: absInvNewton+prepareDFT 49%, blocks loop 48%。

### 实验 1: 预计算 divisor_high DFT 传给 absInvNewton (失败)

**思路**: 当 in < len2 时, 预计算 divisor_high = divisor + (len2 - in) 的 DFT, 传给 absInvNewton 作为 m_dft (B-1 优化), 省 1 次 DFT(divisor_high)。

**float_len 匹配分析**:
- absInvNewton 内部 absMul 的 need_float_len = int_ceil2(inv0_len*2 + k - 1)
- 偶数 in: need_float_len == inv_float_len (永匹配)
- 奇数 in = 2^p-1 (如 63, 127, 255...): 不匹配, absInvNewton fallback

**结果**:
- absInvNewton+prepareDFT: 10.592 → 10.354 ms (仅省 0.24ms, 噪声范围)
- 正确性: MD5 MATCH

**失败原因**: B-1 优化在 in == len2 时有效, 是因为 divisor_dft 被 absInvNewton 和 blocks loop 共享。in < len2 时 divisor_high ≠ divisor, prepareDFT(divisor_high) 只被 absInvNewton 用一次, blocks loop 仍需 prepareDFT(divisor)。**净收益 = 0** (只是搬移 DFT 位置)。

**回退**: 已回退。

### 实验 2: 减小 mu_in 到 len2/4 (失败)

**思路**: 减小 mu_in, absInvNewton(k) FFT 长度减半, prepareDFT 长度减半。预期 absInvNewton+prepareDFT 减半, blocks loop 基本不变 (基于错误理论 "blocks loop FFT 与 in 无关")。

**实施**: 在 absDivRem 中用 `#ifdef DIV_MU_IN_QUARTER` 控制 mu_in 选择:
```cpp
#ifdef DIV_MU_IN_QUARTER
    mu_in = (qn_mu - 1) / 4 + 1;  // len2/4
#else
    mu_in = (qn_mu - 1) / 2 + 1;  // len2/2 (GMP 默认)
#endif
```

**结果** (in=31250, blocks=4):
```
absDivMu: absInvNewton+prepareDFT:  5.990 ms (节省 4.6ms, 符合预期)
absDivMu: blocks loop:             20.201 ms (增加 9.9ms, 远超预期!)
absDivMu: total:                   26.903 ms (变慢 5.3ms)
```

**失败原因**: 理论分析 "blocks loop FFT 与 in 无关" 是错误的。正确分析:
- fftMulPre #1: float_len = int_ceil2(2*in+1), 与 in 正相关
- fftMulPre #2: float_len = int_ceil2(len2+in), 与 in 正相关
- blocks = qn / in, 与 in 反相关
- 总 FFT 开销 = (qn/in) × (float_len1 + float_len2)

| in | blocks | per_block (float_len1+float_len2) | 总 FFT | 比值 |
|----|--------|-----------------------------------|--------|------|
| len2/2=62500 | 2 | 131072+262144=393216 | 786432 | 1.0x |
| len2/4=31250 | 4 | 65536+262144=327680 | 1310720 | 1.67x |
| len2/8=15625 | 8 | 32768+131072=163840 | 1310720 | 1.67x |
| len2=125000  | 1 | 262144+262144=524288 | 524288 | 0.67x (但走 Core2) |

减小 mu_in 会导致 blocks 增加且每块 FFT 开销降幅不足, 总 FFT 开销增加。

**结论**: 当前 mu_in = len2/2 是最优的 (GMP 理论正确)。减小 mu_in 变慢, 增大 mu_in 走 absDivNewtonCore2 也变慢 (absInvNewton 开销翻倍)。

**回退**: 保留 `#ifdef DIV_MU_IN_QUARTER` 条件编译 (默认关闭), 记录此实验结果。

### 下一步方向

两个失败的实验表明, absDivMu 的 mu_in 和 DFT 复用已到极限。需要从其他角度优化:
1. **SIMD 优化 Barrett 进位传播**: fftMulPre 内部的 divBASE (整数除法) 是 carry chain 串行瓶颈, 用 SIMD 并行化无依赖部分
2. **absInvNewton 内部 cyclic convolution**: 重新审视 GMP cyclic 路径 (阶段 A 已验证), 评估是否能用于 absSqr/absMul
3. **降低 FFT 常数因子**: 优化 transform::fft 的实现 (radix-4 auto-vec 已有, 可尝试 radix-8)
4. **float FFT + split**: float 2x throughput, 但需 split 处理精度 (风险高)

---

## 2026-07-21: absInvNewton 内部 PROFILE_DIV 分解 + O2 极限分析

### PROFILE_DIV 实测 (Windows, 1M/500k, O2)

absInvNewton 递归内部分解:

| k | inv0_len | absSqr (ms) | absMul (ms) | absSqr fl | absMul fl |
|---|----------|-------------|-------------|-----------|-----------|
| 62500 | 31252 | 0.708 | 2.510 | 65536 | 131072 |
| 31251 | 15627 | 0.406 | 1.454 | 32768 | 65536 |
| 15626 | 7815 | 0.189 | 0.584 | 16384 | 32768 |
| 7814 | 3909 | 0.097 | 0.278 | 8192 | 16384 |
| 3908 | 1956 | 0.048 | 0.138 | 4096 | 8192 |
| 1955 | 979 | 0.020 | 0.071 | 2048 | 4096 |
| 978 | 491 | 0.011 | 0.042 | 1024 | 2048 |
| 490 | 247 | 0.006 | 0.019 | 512 | 1024 |
| 246 | 125 | 0.006 | 0.010 | 256 | 512 |
| 124 | 64 | 0.007 | 0.260 | - | 256 (base case) |

- absInvNewton 递归总耗时: absSqr=1.498ms, absMul=5.366ms, 总=6.864ms
- absInvNewton+prepareDFT 总耗时: 11.289ms
- absDivMu 总耗时: 22.080ms
  - absInvNewton+prepareDFT: 11.289ms (51%)
  - blocks loop: 10.679ms (48%)
    - #1 (fl=131072): 3.357ms
    - #2 (fl=262144): 6.814ms
    - correct+sub: 0.467ms

### 关键发现

1. **absMul 占 absInvNewton 的 78%** (5.366/6.864)，是最大瓶颈
2. **absSqr 仅占 22%** (1.498/6.864)，已用自平方优化
3. **#2 占 blocks loop 的 64%** (6.814/10.679)，是 blocks loop 最大瓶颈
4. **最外层 (k=62500) 占 absInvNewton 递归的 47%** (3.218/6.864)

### 路径 A (融合 absSqr+absMul) 精度分析

**结论**: 不可行，double 精度不足。

- V = DFT(inv0), |V[k]| ≤ inv0_len * BASE = 3.125e8
- V*V (pointwise sqr): |V*V[k]| ≤ 9.77e16，超 double 2^53 = 9.007e15 约 10 倍
- V*V*M (三重卷积): |V*V*M[k]| ≤ 6.1e25，超 2^53 约 7e9 倍
- real_dot_binrev2 的 inv=1/L 是事后缩放，对中间乘法溢出无能为力
- dot_rfftX2 的 inv 应用顺序是 (a*b)*inv，中间 a*b 会先溢出
- 代码无 split FFT / long double / 三-FFT 实现可绕开此瓶颈

### GMP cyclic 路径理论收益重新计算

之前 O3_DEV_LOG L179-182 估算 "理论节省 37.5%" 是错误的。

正确计算 (k=62500, mn=65536, L_sqr=65536, L_mul=131072):
- fallback: absSqr(2 pass @ 65536) + absMul(3 pass @ 131072) = 2 + 6 = 8 pass @ 65536 equivalent
- cyclic: fftMulModBm1(3 pass @ 65536) + absMul(3 pass @ 65536) = 6 pass @ 65536
- 理论节省: (8-6)/8 = 25%

但 cyclic 路径有额外开销:
1. fftMulModBm1 的 cyclic wrap-around 进位传播 (串行，O(mn))
2. GMP 修正: 减 B^{rn+k} mod (B^mn-1) (串行借位传播，O(mn))
3. copy_backward + xp_full 分配 (O(mn))
4. GMP 组合逻辑: 三步 add/sub (串行进位传播，O(k))

额外开销估算 (最外层): ≈ 1ms
cyclic 节省 (最外层): 2 pass @ 65536 ≈ 0.7ms
净: -0.3ms (变慢)

递归所有层总: 节省 ≈ 1.4ms, 额外开销 ≈ 2ms, 净 -0.6ms

这与实测 "GMP cyclic 无提升 (14.98 vs 14.96)" 一致。cyclic 路径的额外开销主要是串行进位传播，无法 SIMD 并行化。

### GMP 源码深度分析结论

search 子代理系统分析 E:\gmp-6.3.0 后确认:
1. **GMP 大尺寸下无 truncated FFT / middle product via FFT** — 只有 full FFT+丢弃 或 cyclic mod B^m-1 (需代数不变量)
2. absDivMu #1/#2 无代数不变量，cyclic 不可行
3. 用户 mu_in 公式与 GMP 完全一致
4. udiv_qr_3by2 是单 limb 原语，不适用多 limb 块
5. BZ 在用户尺寸下不如 mu_div_qr
6. GMP 无 Hensel 整数逆

### O2 优化方向穷尽清单

| 方向 | 结论 | 原因 |
|------|------|------|
| 路径 A (融合 absSqr+absMul) | 不可行 | double 精度不足 (V*V*M 超 2^53) |
| 路径 C (blocks loop #1 high-product) | 不可行 | GMP 无 truncated FFT |
| GMP cyclic (absInvNewtonGMP) | 无提升 | 理论节省 25% 但额外开销抵消 |
| DIV_MU_IN_QUARTER | 不可行 | blocks 数翻倍 |
| udiv_qr_3by2 | 不适用 | 单 limb 原语 |
| BZ 除法 | 不如 mu_div_qr | 用户尺寸下 mu_div_qr 更快 |
| I/O 优化 | 已最优 | mmap + oBuffer 已实现 |
| radix-8 FFT | 不可行 | SRFFT 已最优 |
| Karatsuba Complex2::mul | 不可行 | FMA CPU 无收益 |
| INV_NEWTON_BASE_THRESHOLD | 64 最优 | 128/256/512/1024 均更慢 |
| -funroll-loops | 无收益 | 噪声范围 |
| DFT 复用 | 无法复用 | 不同数组/不同 float_len |
| cyclic 额外开销 SIMD 优化 | 不可行 | 串行进位传播无法并行 |

**结论: O2 已到极限。转向 O3 优化。**

---

## 2026-07-21 (突破): 方向 1 — 2NXN 循环卷积 (推翻之前结论)

### 重大发现

之前 search 子代理结论 "absDivMu #1/#2 无代数不变量, cyclic 不可行" 是**错误**的。

**实际**: GMP `mu_div_qr.c` L288-303 的 `mpn_mulmod_bnm1` + unwrap 逻辑正是 absDivMu #2 (`qhat * divisor`) 的代数不变量处理方法。

### GMP 2NXN 技巧原理 (mu_div_qr.c L288-303)

```c
tn = mpn_mulmod_bnm1_next_size (dn + 1);   // m = int_ceil2(dn+1)
mpn_mulmod_bnm1 (tp, tn, dp, dn, qp, in, scratch_out);  // C = (dp*qp) mod (B^m-1)
wn = dn + in - tn;                          // wrap 位数
if (wn > 0)
{
  cy = mpn_sub_n (tp, tp, rp + dn - wn, wn);   // unwrap: tp[0..wn-1] -= rp[dn-wn..dn-1]
  cy = mpn_sub_1 (tp + wn, tp + wn, tn - wn, cy);
  cx = mpn_cmp (rp + dn - in, tp + dn, tn - dn) < 0;
  ASSERT_ALWAYS (cx >= cy);
  mpn_incr_u (tp, cx - cy);
}
```

**关键**: `unwrap` 用 partial remainder `rp[dn-wn..dn-1]` (= 本项目 `window[cyclic_m..cyclic_m+wn-1]`) 替换 wrap 部分的线性卷积高位, 重建 prod, 误差由 absDivMu 既有双向修正循环处理。

### 项目实施 (moptm_fusion.cpp)

**条件编译**: `#ifndef DISABLE_2NXN_CYCLIC` (默认启用, 用 `-DDISABLE_2NXN_CYCLIC` 禁用)

**3 处编辑**:

1. **DFT 预计算区 (原 L3148-3178)**: 添加 `divisor_dft_mod_buf` (cyclic_m 长度) 和 `cyclic_m = int_ceil2(len2+1)` 计算
2. **fftMulPre #2 替换 (原 L3235-3247)**: 用 `fftMulModBm1Pre(qhat, divisor_dft_mod_buf, len2, cyclic_m, prod)` 替代 `fftMulPre(qhat, divisor_dft_buf, len2, divisor_float_len, prod)`
3. **buffer 溢出修复 (原 L3192-3195)**: `prod_len_max = max(prod_len_max, cyclic_m)`

**FFT 大小对比** (1M/500k, len2=125000, in=62500):
- 旧: `int_ceil2(len2 + in) = int_ceil2(187500) = 262144`
- 新: `cyclic_m = int_ceil2(len2 + 1) = int_ceil2(125001) = 131072`
- **FFT 长度减半**

### 验证结果

**正确性** (MD5 全部匹配):
- 1M/500k: MD5 = `3CBCBDEA21E67FCB955D9AB26B61C232` (cyclic) vs `3CBCBDEA21E67FCB955D9AB26B61C232` (baseline) ✓
- 200k/100k: MD5 匹配 ✓
- 12 组定向 fuzz: 全部 MD5 匹配 ✓
- 100 组随机 fuzz (尺寸 5000~50000 limbs): 全部 MD5 匹配 ✓ (修复 Case 20 buffer 溢出后)

**性能** (VM 独占, 192.168.1.55, 15 runs × 50 loops, 中位数):

| 测试 | Baseline | Cyclic | Speedup |
|------|----------|--------|---------|
| 1M/500k | 25.805 ms | 21.753 ms | **-15.70%** |
| 200k/100k | 6.072 ms | 5.260 ms | **-13.37%** |

**PROFILE_DIV 分解** (Windows 本地, 1M/500k, O2):
```
absInvNewton+prepareDFT: 10.156 ms (vs baseline 11.289 ms)
blocks loop:
  block 0: #1=1.890 ms, #2(cyclic)=1.938 ms (vs baseline 3.462 ms, -44%)
  block 1: #1=1.646 ms, #2(cyclic)=1.816 ms (vs baseline 3.352 ms, -46%)
  correct+sub: ~0.5 ms
  total: 7.831 ms (vs baseline 10.679 ms)
total: 18.107 ms (vs baseline 22.080 ms, -18%)
```

### 关键 Bug: buffer 溢出

**现象**: 100 组 fuzz 中 Case 20 (a=20000, b=10000) 堆损坏崩溃 (rc=3221226356)

**根因**: `tprod.size() = prod_len_max = len2 + in + 1`, 但 `fftMulModBm1Pre` 写入 `cyclic_m` 长度, 当 `cyclic_m > prod_len_max` 时溢出

**修复**:
```cpp
size_t prod_len_max = len2 + in + 1;
#ifndef DISABLE_2NXN_CYCLIC
prod_len_max = std::max(prod_len_max, cyclic_m);  // cyclic_m 可能 > prod_len_max
#endif
```

### 结论

方向 1 突破成功, **O2 优化空间仍未穷尽**。之前"O2 已到极限"结论错误, 原因是 search 子代理对 GMP mu_div_qr.c L288-303 的 2NXN unwrap 技巧理解不足。

下一步: 实施方向 3 (absAdd1/absSub1 carry 早终止), 评估方向 2+7 (修正循环 borrow 检测替代 absCompare)。

---

## 2026-07-21 (续): 方向 3 + 方向 2+7 实施

### 方向 3: absAdd1/absSub1 carry 早终止 (免费午餐)

**位置**: `moptm_fusion.cpp` L1810-1845

**优化**: 当 carry/borrow == 0 时早终止循环:
- in-place (in1.ptr == out.ptr): 直接 return (后续 out[i] 已等于 in1[i])
- 非 in-place: `std::copy` 剩余部分 (memcpy 比 for-loop 快)

所有调用点均为 in-place, 直接受益。

### 方向 2+7: 修正循环优化

**位置**: `moptm_fusion.cpp` L3320-3346

#### 修正 1 快速检查
```cpp
// 原: while (absCompare(prod_span, window) > 0) { ... }
// 新: while (prod_span[prod_span.size - 1] != 0 || absCompare(prod_span, window) > 0) { ... }
```
**原理**: `prod_span.size = len2 + this_in + 1 > window.size = len2 + this_in`。若 prod 最高位非零, prod 位数 > window 位数, prod > window 必然, 跳过 absCompare 的 O(n) count_true_length 扫描。

#### 修正 2 while→if
```cpp
// 原: while (absCompare(window, divisor) >= 0) { absSub(window, divisor, window); absAdd1(qhat_span, 1, qhat_span); }
// 新: if (absCompare(window, divisor) >= 0) { absSub(window, divisor, window); absAdd1(qhat_span, 1, qhat_span); }
```
**原理**: absInvNewton 是精确逆 (floor(D/B^k)), 精度足够高, 修正 2 最多迭代 1 次 (对照 GMP mu_div_qr.c L335-341 的 `if (mpn_cmp(rp, dp, dn) >= 0)` 单次检查)。

### VM 验证结果 (192.168.1.55, VM 独占)

**正确性** (100 组随机 fuzz + 标准测试, MD5 全部匹配):
- 100 组 fuzz (尺寸 5000~50000 limbs × 4 digits): MD5 = `b01ed0b2dd2cbab08117876e625a2020` (baseline) = `b01ed0b2dd2cbab08117876e625a2020` (default) ✓
- 1M/500k: MD5 匹配 ✓
- 200k/100k: MD5 匹配 ✓

**性能** (15 runs × 50 loops, 中位数, ms/loop):

| 测试 | baseline (DISABLE_2NXN_CYCLIC) | default (cyclic+dir3+dir2+7) | Speedup |
|------|----------|--------|---------|
| 1M/500k | 22.870 | 19.633 | **-14.15%** |
| 200k/100k | 5.932 | 5.087 | **-14.24%** |

**注**: 方向 2+7 的贡献被噪声掩盖 (与方向 1 单独的 -16.80%/-14.82% 接近)。修正 2 的 while→if 无实质收益 (absInvNewton 精度高, 修正 2 本来就最多 1 次)。修正 1 的快速检查无实质收益 (prod 最高位通常为 0, 仍需 absCompare)。

### 结论

方向 3+2+7 均为免费午餐, 正确性验证通过, 性能无回退。保留这些优化。

剩余最大瓶颈: absInvNewton 的 absMul (5.3ms, 占 absInvNewton 78%)。absInvNewton 总耗时 10ms, 占 total 53%。

---

## 2026-07-21 (续2): VM 独占后 O2/O3 稳定 benchmark + 阈值实验

### 背景

用户暂停 GCC 修改版 AI，VM Ubuntu 独占，CPU 数据更稳定。重新 benchmark 确认 O2/O3 性能关系。

### 实验 1: O2 vs O3 vs O3-novec (15 runs × 50 loops, 中位数)

编译选项:
- `moptm_o2`: `-O2 -std=gnu++20 -DHINT_OP_DIV`
- `moptm_o3`: `-O3 -std=gnu++20 -DHINT_OP_DIV`
- `moptm_o3_novec`: `-O3 -fno-tree-loop-vectorize -std=gnu++20 -DHINT_OP_DIV`

| 测试 | O2 | O3 | O3-novec |
|------|----|----|----------|
| 1M/500k | 19.200 ms | 19.200 ms | 20.200 ms |
| 200k/100k | 4.900 ms | 4.900 ms | 5.600 ms |

**结论**:
1. O2 和 O3 性能**完全相同** (VM 独占后数据稳定，之前 O3 略慢是噪声)
2. O3 -fno-tree-loop-vectorize **变慢** 1ms (1M/500k) / 0.7ms (200k/100k)
3. 说明 O3 的循环向量化是有益的，禁用它反而变慢
4. O3 没有额外优化空间 (与 O2 相同)

### 实验 2: INV_NEWTON_BASE_THRESHOLD 阈值实验

编译 `-DINV_NEWTON_BASE_THRESHOLD=128/256`，测试提高 base case 阈值是否能减少递归层数提升性能。

| 测试 | thr=64 (默认) | thr=128 | thr=256 |
|------|---------------|---------|---------|
| 1M/500k | 19.900 ms | 20.600 ms | 20.500 ms |
| 200k/100k | 5.100 ms | 5.700 ms | 6.000 ms |

**结论**: 阈值 64 最优。提高阈值变慢 (absDivBasicCore O(k²) 比 FFT 慢)。与 O3_DEV_LOG 早期结论一致。

### O2 优化方向再次穷尽

| 方向 | 结论 | 原因 |
|------|------|------|
| O3 -fno-tree-loop-vectorize | 变慢 | 循环向量化有益 |
| INV_NEWTON_BASE_THRESHOLD=128/256 | 变慢 | absDivBasicCore O(k²) 比 FFT 慢 |
| absSqr 保留 DFT 给 absMul 复用 | 无净收益 | absSqr fl=65536, absMul fl=131072, 长度不匹配 |
| High Product / Middle Product (#1) | 不可行 | cyclic 无法重建高位 |
| Truncated FFT | 不可行 | 实现复杂, GMP 也无此实现 |
| float FFT (单精度) | 不可行 | BASE=10^4 精度不够 (N*10^8 > 2^24) |

### 最终结论

**O2 和 O3 均已到极限**。当前性能:
- 1M/500k: 19.2 ms (O2 = O3)
- 200k/100k: 4.9 ms (O2 = O3)

absInvNewton 的 absMul (FFT fl=131072) 是最大瓶颈，但 conv_len=125003 无法减小。blocks loop 的 #1 和 #2 (FFT fl=131072) 也无法减小。

**下一步**: 翻看 E:\盘根目录寻找新的优化方向，或转向其他任务。

