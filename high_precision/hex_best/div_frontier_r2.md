# HEX DIV 出头点 `length_ratio_integer_2` 前沿判定（Round 2 侦察收口）

> 实测铁律：oracle 26 例字节相等 = 正确性硬判；`perf instructions:u` = 跨架构有效真值；LC 取单点 MAX。
> 本文件是**纯侦察 + 根因**，未改任何生产码；原件 `div_base16_r2_base.cpp` 留存备查。

## 1. 调度链路（实测确认该点走哪条路径）

`length_ratio_integer_2`：A=1.33M hex ≈ 332.5k limb，B=266666 hex ≈ 66.7k limb，`blocks ≈ 5`。

```
absDivRem
  └─ len1 >= len2*2  (332.5k >= 133.3k)  →  else 分支 (行 6585)
       └─ absDivMu(dividend, divisor, quot, in_used, allow_cyclic=false)   // 行 6716 硬编码 allow_cyclic=false
```

`absDivNewtonCore2`（行 6412）此点**不命中**（仅 `len1 < len2*2` 时走 Core1/Core2）。

## 2. 逆元已吃 cyclic 红利（这部分已兑现，不是头）

`absInvNewtonGMP`（行 4896）内：
```cpp
// 行 5021
bool use_cyclic = (mn >= k+1) && (mn <= k+rn) && (k >= CYCLIC_MIN_K) && !inv0_degenerate;
// mn = f_ceil_mn(k+1)
```
顶层 `k ≈ 66.7k` → `mn ≈ 98k ≤ k+rn(100k)` → **逆元内部乘积已用缩减 FFT（~98k 而非 ~133k）**。
→ 逆元这条链路的 FFT 缩减红利**已经兑现**。

## 3. 块循环乘积 = 未缩减的主成本（真正的头，但被锁死）

`absDivMu` 块循环两次 `fftMulPre`（行 5963 / ~5990）：
```cpp
inv_float_len     = fft_ceil_lin(2*in+1);     // ~131k~196k
divisor_float_len = fft_ceil_lin(len2+in);    // ~131k~196k
```
因 `allow_cyclic=false` → `use_cyclic=false` → **走全尺寸线性 FFT**。
`blocks≈5` → 约 **10 次全尺寸 FFT** = 该点不可削减的主成本。

### 为什么不能裸开缩减 cyclic

`absDivMu` 内 `use_cyclic` 的判定逻辑（行 5804 / 5810-5813）：
```cpp
use_cyclic = (cyclic_m_gate < in+len2) && allow_cyclic;     // 5810: 需 allow_cyclic 才为真
if (allow_cyclic) { cyclic_m = fft_ceil_cycm(in+len2+1); use_cyclic = true; }  // 5810-5813: 强制全尺寸!
```
→ **缩减 cyclic（小 FFT）在当前代码不可达**：`allow_cyclic=false` 时 `use_cyclic=false`（线性）；`allow_cyclic=true` 时 `cyclic_m` 被强制拉到全尺寸（无缩减）。

注释 6709–6711 明确这是**有意为之的正确性封锁**：
> "cyclic 路径：VM 真 FFT 下大档（gen large seed=34，商第22207位起错）及多块场景会偏离精确 product（mod B^m-1 折叠在多块累积误差）。与 DEC verified 一致强制禁用，走精确线性卷积，牺牲约 22% FFT 缩减换取正确性。"

**即：short-product（中乘积）在这套除法里 = 缩减 cyclic FFT，正是被封存的已知陷阱。不能"随便试"地裸开——oracle 必炸。**

## 4. FFT 内核已是 cache-oblivious（4-step 边际小、风险高）

`rdif`（行 2677）对 2 幂长度走递归 radix-4 `dif`/`difC4`（行 1415 自递归），**深层自动下沉到 L1/L2 尺寸**，仅最外 1~2 级跨距超 L2。这正是 Round 1 预取实测"污染"（非真正缺失）的根因：工作集超 L2 但被 L2/L3 吸收，HW 预取跟不住远流却搬动的线是污染。

→ 4-step/6-step 重写 FFT 内核：边际收益小（递归已缓解大部分），且**动到 mul/sqr/div 共享内核**，影响面大、前代 6-step 曾挂死。属高风险、低边际。

## 5. 结论：该出头点已到前沿

| 候选杠杆 | 状态 | 风险/收益 |
|---|---|---|
| 顶层蝶形 SW 预取 (Round 1) | 实测失败（污染 +0.181%） | — |
| 缩减 cyclic 块乘积 / short-product | **已知正确性陷阱（多块折叠误差）** | 高风险的"必炸"，非可裸开 |
| 4-step/6-step 重写 FFT 内核 | 边际小、影响面广 | 高风险、低边际 |
| **解多块 cyclic 折叠误差** | **未做，唯一真头** | 高收益（reclaim ~22% 块乘积 FFT），高风险 |

## 6. 下一步（需你拍板方向，二者皆需 oracle 26 例门控 + 隔离变体 + VM instructions:u 收口）

1. **解多块 cyclic 折叠误差**（让 reduced cyclic 在 `absDivMu` 合法化）→ 直接 reclaim 块乘积 ~22% FFT 缩减，是文档里就规划好的"intended"优化。难点=修 unwrap 跨块累积误差（已知难点，但最值钱）。
2. **4-step/6-step 重写 FFT 内核** → 边际、风险高、影响面大。

**我的建议**：走 (1)。它是除法本身"该吃没吃到的红利"，且只动 `absDivMu` 的 cyclic 组合逻辑（不动共享 FFT 内核），oracle 门控下若仍炸可精确拿到失败签名（哪些 case、哪些位错），反向指导 unwrap 修复。

> 注：`div_base16_r2_base.cpp` 为 Round 2 原件快照（360,295 B），未折回 canonical。
