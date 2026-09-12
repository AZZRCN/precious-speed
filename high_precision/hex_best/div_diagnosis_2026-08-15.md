# HEX / DEC 大档除法排查诊断报告（2026-08-15）

## 一、关键认知反转：本地 MinGW 真 FFT 不可信

此前所有"cyclic 路径失败"的结论，都建立在"本地 MinGW 编译的二进制能复现 VM 行为"的假设上。
本轮用一组**控制变量**测试推翻了这个假设：

| 编译/路径 | 200-limb | 300-limb | 400-limb |
|---|---|---|---|
| `DISABLE_FFT_ALL`(absMul 精确) + `DISABLE_2NXN_CYCLIC` | OK | OK | OK |
| **真 FFT** + `DISABLE_2NXN_CYCLIC`(走 `fftMulPre` 线性) | **FAIL** | **FAIL** | **FAIL** |
| 真 FFT + cyclic(走 `fftMulModBm1Pre`) | FAIL | FAIL | FAIL |

结论：**只要启用真 FFT，本地 MinGW 下大尺寸除法一律 FAIL**，与是否走 cyclic 无关。
这与 `dec_best/div_verifield` 头部注释完全一致：

> 验收/编译权威: 本机 VM (Linux g++ 15.2.0 = LC 部署编译器); 本地 MinGW/g++ 不可信。

因此：**本地无法验证任何真 FFT 路径（含 cyclic），所有"cyclic 失败"的本地结论都被本地 FFT 浮点误差污染，不能据此定论。**

## 二、对先前"cyclic 实现 bug"推断的纠正

先前基于本地 `UNWRAP_PROBE`（真 FFT）抓到的
`err ~ ±1e9`、`TRUE=99`（cyclic product 与精确 product 差 >1 limb），
曾推断 `fftMulModBm1` 有实现级 bug。

现在必须纠正：**该证据由本地不可信 FFT 产生，不能区分"FFT 浮点误差"与"fold 逻辑 bug"。
本地已无手段定论 cyclic 是否为真实现 bug**——只能在 VM 上验证。

## 三、DEC verifield 的修复到底是不是"治标"

`dec_best/div_verifield/div_verifield.cpp` 头部（L4-7）明确：

> 唯一算法改动: absDivMu 分派处 `allow_cyclic = false`
> (修复真实商错误: gen 'large' seed=34, a=81479位 / b=2531位, 商第22207位起错;
>  Python divmod 神谕确认 ref 正确、mine 违反 A=qB+r)

这是 **VM（LC 编译器，即部署环境）上实测** 的大档 cyclic bug，用 `use_cyclic=false` 消除。
**这不是敷衍**：它是针对 VM 实证的不可能可靠路径的、正确的正确性保障。
用户的"治标"直觉源于"关开关=敷衍"的联想，但技术上是消除不可靠路径。

HEX 的 `div_base16.cpp` 与 DEC verifield **同源**（仅 base 不同：HEX=2^16，DEC=2^64），
且 HEX 在 L6613 默认 `allow_cyclic = (mu_in >= 64)`（即默认走 cyclic），
故 HEX 在 VM 真 FFT 下会触发与 DEC verifield 同款的大档 bug。

## 四、已落地的独立修复：牛顿逆 off-by-one（与 cyclic 无关）

在"本地可验证逻辑层"的范围内（`DISABLE_FFT_ALL` 走 absMul，FFT 不参与），
我**独立确认并修复**了一个真实 bug：

- `absInvNewton` 的 HIGH-half 翻倍法 `x = 2·B^s·x0 − ⌊x0²·m / B^{2s}⌋`
  会**偏大 1**（limb 0 偏 1，高位全同）。
- 用 Python 大整数**精确真值**判定：正确 `⌊B^{2k}/m⌋` 与 dump 的 `REF` 逐 limb 一致，
  与翻倍法 `INV` 仅 limb 0 差 1 → 根因在 Newton 翻倍法缺末位校正，不在 `absDivBasicCore`。
- 修复：在 `absSub` 之后插入精确化校正（最多 4 次 `inv--`/偏大修正），
  覆盖 `absInvNewton` 主路径 + `gmp_newton_fallback` 路径。
- 本地逻辑层验证：`[invself] match=Y`（之前为 `match=N`）。

此修复**不依赖 FFT**，本地可验证，是独立于 cyclic 的真实改进，已保留。

## 五、HEX 当前动作：对齐 DEC verifield 禁用 cyclic

为"确保 HEX 在 VM 正确"这一硬需求，已将 `div_base16.cpp` L6613：
`bool allow_cyclic = (mu_in >= 64);` → `bool allow_cyclic = false;`
（备份：`div_base16_before_cyclicfix.cpp`）。

这是把 HEX 与 DEC verifield 对齐，使 VM 上大档除法走精确的线性卷积路径。

## 六、"治本"方向 —— 让 cyclic 路径真正可用（VM 专项）

若用户希望恢复 cyclic 的 ~22% FFT 缩减（而非永久禁用），属于独立治本任务，
**必须在 VM 上完成**（本地 FFT 不可信）：

1. 在 VM 上编译 `div_verifield.cpp`（`-O3`，**不**定义 `DISABLE_2NXN_CYCLIC`），
   跑 gen 'large' seed=34 复现大档 bug，确认是 cyclic 路径。
2. 用 `UNWRAP_PROBE`（代码已内置，仅 `cyclic_m<=8192` 时算 ground truth）在 VM 上抓取
   `err/TRUE`，区分：
   - (a) FFT 浮点误差（cyclic product 低 limb 微偏）→ 属数值精度，需调 `copyU16ToF64AndFill`/`carryPropSeg` 精度；
   - (b) `mod B^m-1` fold 逻辑 bug（L3979-4014）或 unwrap 常数偏差（L5966-6086）→ 属算法。
3. 重点怀疑对象：cyclic 卷积 `P mod (B^m-1)` 在 `m < len2+this_in` 时，
   wrap 次数 `k = P/(B^m-1)` 可达 `B^(len2+this_in-m)`，远超 unwrap 假定的 ±1；
   多块场景下误差沿余数传播雪崩。这正是注释里"unwrap 近似误差在多块累积"的签名。
4. 治本修正应使**每块** qhat 均可被精确校正（r-based 修正 + 精确 product 兜底），
   而非依赖近似 unwrap 恢复完整 product。

## 七、下一步

- HEX：`div_base16.cpp` 现已含（1）牛顿逆校正（2）对齐禁用 cyclic。
  **需推到 VM 跑 `launch_invchk.sh` 验证 repro 全 PASS**（本地只能验证逻辑层）。
- DEC verifield：当前 `use_cyclic=false` 已是 VM 实证的正确修复，无需改动；
  若要治本，按第六节在 VM 推进。
- cyclic 治本属 VM 专项，本地不可验证，建议作为独立任务排期。
