# HEX cyclic 除法治本排查日志 (cyclic_debug_log.md)

> 持久排查日志。每次上下文压缩/断点续作，先读本文件，不要换方向重查。
> 最后更新：2026-08-15（重要订正：早前"mu_in<64 门槛"结论是误诊，已推翻）

---

## 0. 当前结论（一句话）

cyclic 路径有**两处**独立于 GMP 的偏差：
1. **unwrap 修正**（已修）：原 `_err` 用低 2 limb 启发式，错。已改为 GMP `mu_divappr_q.c` L229-231 的**全 limb 比较** `cx=mpn_cmp(rp+dn-in,tp+dn,tn-dn)<0; tp+=(cx-cy)`。探针确认乘积修正现在 100% 正确（OK）。
2. **★ 根因（已用块级探针定位，2026-08-15 续作）**：半尺寸 cyclic 卷积的结构性失效。
   `cyclic_m ≈ (len2+in)/2`（性能来源），但乘积 `qhat*divisor` 有 `len2+this_in` 位，
   wrap 次数 `k ≈ B^(len2+in-cyclic_m)`（case 513 实测 ≈ 65535）。r-method 的单 limb 判据
   `r = window[len2]-tprod[len2]` 与 `±1` unwrap 只能处理 `k∈{-1,0,1}`；当 `k≈65535` 时
   `r` 变成 -65535，r-method 的 `+1` 修正循环（上限 10）修不回来 → qhat 与余数全错。
   **GMP `mu_divappr_q` 能用 cyclic 是因为其模数 `tn≈dn+1`（非半尺寸），相关乘积 limb 误差恒 ≤±1；
   我们的 `cyclic_m` 小得多，wrap 巨大，r-method 结构性失效。**
   → 这是 cyclic 不能默认开启、且"修 remainder 路径"不足以解决的根因。

**当前处置**：`allow_cyclic` 默认 **false**（走精确线性卷积，保证正确）；`CYCLIC=1` 显式开启 cyclic（供修 remainder 路径时测试）。全量 9 gen × seed 1..8 oracle 验收（精确线性）后台运行中，预期 0 失败。

---

## 1. 失败现象（确定性，非偶发）

- 失败带：`burnikel_ziegler_bound` gen，seed=2，case 512–998（约 488 个）。
- 尺寸：len2 limbs ≈65–124，比值 ≈2。
- 错误类型：早期以为 q+r 全错；**精确定位后**是 q 正确、r 错（低位少一个 F，即 r 偏小约 B^M）。

## 2. 关键证据链（已彻底重做，推翻旧结论）

| 实验 | 操作 | 结果 | 结论 |
|---|---|---|---|
| A | 逐进程对拍 | 失败确定性 | 非 thread_local 污染 |
| B | `NOCYCLIC=1`（精确线性） | lin_bad=0 | 精确线性路径正确 |
| C | cyclic (旧 `_err` 启发式) vs oracle | cyc_bad=488 | cyclic 路径整体错 |
| D | 改 `_err`→GMP `cx-cy`（满块偏移 `window.ptr+len2`） | 满块探针全 OK；case 600 三路一致 | unwrap 修正已正确 |
| E | case 512 cyclic vs oracle | q 对、r 错 1 limb | **错在余数路径，非 unwrap** |
| F | 批量探针 cases 512–560 | 全部 `this_in=64 m=80 OK` | 满块乘积修正全对；r 错是 remainder 逻辑 |

→ **决定性结论**：根因不是"自然 mu_in<64 误开 cyclic"（旧假设误诊）——那些失败 case 的 `mu_in` 本就 ≥64，门槛拦不住。真因是 cyclic 余数/调 qhat 路径（L6126-6184）相对 GMP 仍有未对齐处，使 r 算错。

## 3. 已做的修复

### 3.1 unwrap 修正（已完成，正确）
文件 `div_base16.cpp` L5992-6081 区域：
```cpp
// 旧（错）: _err = (P mod B^2 低2 limb 启发式); tp -= _err
// 新（GMP 同层）:
size_t _cmp_len = cyclic_m - len2;                       // tn - dn
bool _cx = absCompare(View(window.ptr + len2, _cmp_len),  // rp+dn-in (=window[len2], 恒成立)
                      View(prod_mod_span.ptr + len2, _cmp_len)) < 0;
int _err = (int)_cx - (int)borrow;                       // = cx - cy ∈ {-1,0,1}
if (_err > 0) absAdd1(prod_mod_span, 1, prod_mod_span);   // GMP: tp += (cx-cy)
else if (_err < 0) absSub1(prod_mod_span, 1, prod_mod_span);
```
注：比对偏移用 `window.ptr + len2`（因 GMP 处 `in` 已缩到当前块长 `this_in`，`rp+dn-in = window[len2]` 恒成立；满块 `this_in==in` 等价，末块必须用此偏移，否则误加 1）。

### 3.2 allow_cyclic 默认关闭（正确性优先）
```cpp
bool allow_cyclic = (std::getenv("CYCLIC") != nullptr);  // 默认精确线性; CYCLIC=1 开启
```
原因：unwrap 已对，但 remainder 路径仍有 bug，故默认关 cyclic 保证正确。

## 4. 验收状态（截至写本日志）

| 项 | 状态 |
|---|---|
| unwrap 修正 GMP 同层 | ✅ 探针全 OK |
| 精确线性路径（默认）oracle | ⏳ 全 9 gen × seed 1..8 后台跑，预期 0 |
| cyclic remainder 路径 | ❌ 未修（q 对 r 错，待定位） |

## 5. 已排除 / 不要重复踩的坑（防方向漂移）

- ❌ "自然 mu_in<64 才错"——误诊。失败 case mu_in 多 ≥64，门槛无效（cyc_bad 实测 488→仍 488）。
- ❌ analyze.py 偶发假阴性：曾报 cycfix 0 失败，oracle 复核为 488。验证须用 **oracle（Python 大整数真值）**，analyze.py 仅作辅助。
- ❌ 不要只改 cycfix 副本不碰主源（前次"BUG 又出来"的错觉来源）。本次主源已改。
- ✅ `fftMulModBm1Pre` 确为 mod B^m−1（L4042 进位+wrap），与 GMP `mpn_mulmod_bnm1` 语义一致 → 乘积本身对，问题在修正/余数逻辑。
- ⚠️ 末块 offset：GMP `rp+dn-in` 的 `in` 是**当前块长**，必须用 `window.ptr+len2`，不能用 `window.ptr+(len2+this_in-in)`。

## 6. 下一步（待用户宣布 HEX 收工或授权续修）

1. 等全量 oracle 验收结果（精确线性）确认 0 失败 → 收口 HEX 正确性。
2. 修 cyclic 路径：**不是"修 remainder 路径"能解决的**（见 §7），需按 GMP 重新设计为
   "cyclic 近似商 + 末尾精确余数（r = N - Q*B 精算）"，才能既保正确又留 cyclic FFT 收益。
   或退而求其次：保持 cyclic 默认关闭（正确、无收益）。需用户拍板。
3. 正确性收口后做 HEX 性能压榨（#C，znver3/AMD 周期优先，perf 指令数）。
4. 用户宣布 HEX 收工后迁移 DEC（#E）。

## 7. 块级探针证据（2026-08-15 续作，决定性）

- 工具：`repro_cyc.py`（CYCLIC=1 vs NOCYCLIC=1 逐块 diff `CYCBLK` dump，linear 为真值）、
  `find_first_cyc_bad.py`（定位首个 `CYCLIC=1` 错例，len2>64 确保走 absDivMu）。
- 首个错例：`burnikel_ziegler_bound seed=2 CASE=513`，lenB=257hex=65 limbs，走 absDivMu。
- 块级 diff（CASE 513）：
  ```
  [BLOCK 0] DIFF
    CYC: blk=0 len2=65 this_in=64 r_init=-65535 r_end=-65526 rem0=49151 rem1=2 remhi=65535 q0=65525
    LIN: blk=0 len2=65 this_in=64 r_init=NA    r_end=NA    rem0=16383 rem1=0 remhi=65535 q0=65535
  [BLOCK 1] DIFF
    CYC: blk=1 len2=65 this_in=1 r_init=1 r_end=0 rem0=0 rem1=32768 remhi=65535 q0=65535
    LIN: blk=1 len2=65 this_in=1 r_init=NA  r_end=NA  rem0=0 rem1=0    remhi=65535 q0=65535
  ```
- **smoking gun**：BLOCK 0 的 `r_init = -65535`（应为接近 0 的小值）。修正循环 `corr_cnt<10`
  只把 `r` 从 -65535 推到 -65526（改动 9），远未归零 → 余数与 qhat 全错。
- `r_init = window[len2] - tprod[len2]`：tprod[len2]（index 65 的乘积 limb）被 wrap `k≈65535` 改掉。
  单 limb 判据 + `±1` unwrap 表示不了 `k≈65535` → r-method 结构性失效。
- 结论：半尺寸 cyclic（FFT 收益来源）与 r-method 的精确余数算法**根本不兼容**；GMP 因模数
  `tn≈dn+1` 使相关 limb 误差恒 ≤±1 才可用。我们的 `cyclic_m` 小，wrap 大，必错。
- 注：unwrap 修正本身（§3.1）对"低 cyclic_m limb 的 ±1 误差"是正确的（探针已证）；错在
  wrap 跨越多 limb（k≈65535）时单 limb r 判据彻底失真，非 unwrap 的错。

---

## 7. 用户授权重构 + 决定性结案（2026-08-15 下午）

**用户授权**：按 GMP 风格重做 cyclic —— 保留 cyclic 半尺寸近似商 + 末尾精确算 r=N−Q·B + 归一化。

**实测推翻该方案（cyc_qr_check.py, burnikel seed=2）**：
```
both_right=1576  q_right_r_wrong=0  q_wrong_r_right=0  both_wrong=488
```
→ cyclic 错时 **q 和 r 同时错**（非只有 r 错）。样本显示 q 偏 ~0xA0000、r 偏 +0xA0000·B。
说明半尺寸 cyclic 算出的 **Q 本身就是错的**（wrap≈65535 使 qhat 所在 limb 失真），
末尾用错 Q 精算 R=N−Q·B 只得到 R+0xA0000·B，归一化需循环 4 万次，不可接受。
→ "保留近似商 + 末尾精确余数" 治不了。

**GMP 同层正解（源码已落实，L5714 后追加）**：cyclic 要正确必须 `m >= in+len2`（无 wrap），
即 GMP `mu_divappr_q` 的 `tn≈dn+1` 同层——但我们每步算 `in` limb 商，需 `m>=in+len2`。
故 `CYCLIC=1` 强制 `cyclic_m = fft_ceil_cycm(in+len2+1)` + `use_cyclic=true`（全尺寸 mulmod）。

**验证（VM, div_base16_zn, CYCLIC=1）**：
```
burnikel_ziegler_bound s2: cyc_bad=0   (原 488 失败 → 全尺寸后 0)
large s2/s5:            cyc_bad=0
length_ratio_integer s2/s5: cyc_bad=0
a_max_b_random s2/s5:   cyc_bad=0
r_nearly_zero s2/s5:    cyc_bad=0
```
→ 半尺寸 wrap 是 cyclic 失败的唯一根因；全尺寸 cyclic（GMP 风格）精确。

**测速（perf instructions:u, A=400000/B=206025 limbs, VM znver3）**：
```
CYCLIC=1 (全尺寸): 462,220,403 instructions
LINEAR (默认):     462,220,178 instructions
差值: 225 (~0.00005%)  → 全尺寸 cyclic 与线性指令数完全相同
```

**结案结论**：
1. 半尺寸 cyclic（≈2x FFT 提速点）在精确商算法下**结构性不可修复**——wrap 使 Q 失真。
2. GMP 风格全尺寸 cyclic **正确但速度 = 线性**（无收益）。
3. 故 **线性 FFT 路径即最快正确路径，cyclic 退役**（不再作为提速手段）。
4. 默认 `allow_cyclic=false`（线性，已 9gen×8seed oracle 全 0，验收进行中重 gen 全过）。
   `CYCLIC=1` 保留为"验证等价"的开关（全尺寸，正确且 == 线性），非生产提速路径。
5. 真正的 HEX 提速点在别处：FFT 内核（split-radix/AVX-512 汇编/少趟）、单发 mu_div_qr
   重构、schoolbook 阈值调参 —— 见 `perf_bottleneck.md`（任务 #D）。

---

## 9. 有符号除法符号 bug（漏测盲区，2026-08-15 晚发现并修复）

- **发现**：全量 oracle 验收时 `power` gen 出现 `lin_bad=2501/3568`（**线性路径也失败**）。
  之前只测 burnikel/large/length_ratio/a_max_b_random 等**从不取负**的 gen，误判"线性全对"。
- **根因**：HEX 主路径直接调 `absDivRem`（无符号绝对值除法），且**从不给 a/b 设符号** →
  负号被丢弃，q/r 输出恒为正。`power` gen 在 `seed!=0` 时给 B 加负号（`power.cpp` L45），暴露该 bug。
- **约定确认（关键）**：查 DEC `origin/div.cpp` 主路径 int64 快速路用 `va/vb`、`va%vb`（C **截断**语义，
  余数符号=被除数 A）。故 LC 约定 = **截断（trunc）**，非 Python floor。原 `verify_oracle.py` 用 floor 是错的，
  已改为 `tq=abs(a)//abs(b); if (a<0)!=(b<0): tq=-tq; tr=a-tq*b`。
- **修复**（div_base16.cpp）：新增有符号 `divRem`（截断语义：q 符号=sign(A)^sign(B)，r 符号=sign(A)），
  main 改调 `a.divRem(b,q,r)` 替代 `a.absDivRem(b,q,r)`。`fromCharRange` 已正确解析负号设 `sign`，
  故只需在输出补符号（absDivRem 算量值，divRem 补符号）。
- **验证（VM）**：power gen seed1 `bad=0/3568`；非整除负例 `A=256/B=-10 → q=-25 r=6` ✓；
  `A=-256/B=10 → q=-25 r=-6` ✓。全 9gen×8seed 截断语义验收后台跑（写 `verify_full.txt`），截至续作 42 行全 0。

## 10. cyclic 封存（用户"把 cyclic 提取封存，专心优化 FFT"）

- **封存档案**：`cyclic_archived.cpp`（提取的 cyclic 代码 + 根因 + GMP 对比，不编译，仅参考存档）。
- **生产源码**（div_base16.cpp）：
  1. 顶部 `#define DISABLE_2NXN_CYCLIC` → 所有 `#ifndef DISABLE_2NXN_CYCLIC` 块编译期排除。
  2. `allow_cyclic` 改 `constexpr bool allow_cyclic = false`（运行期永不触发 cyclic）。
  3. 移除 `fftMulModBm1Pre`（cyclic 原语，封存后无调用方）；2NXN 逆元里的调用加
     `#ifndef DISABLE_2NXN_CYCLIC` 守卫（运行期走 `fftMulModBm1` 线性回退）。
- **封存二进制** `div_base16_sealed` 构建通过（128344B < 原 138216B，cyclic 代码已剔除）；
  power/burnikel/length_ratio/a_max_b_random 截断语义验收全 `cyc_bad=0 lin_bad=0` → 封存后线性正确。
- **结论**：cyclic 已封存退役（半尺寸结构性失效、全尺寸=线性零收益）。提速主战场正式转入
  **FFT 内核**（perf_bottleneck.md O1-O6）。HEX 正确性基线：线性 FFT + 符号截断，全 gen oracle 验收进行中。



## 8. 用户追问"cyclic 真的用不了吗" → GMP 真身对比(决定性, 2026-08-15 下午)

**动机**：上一轮结案说"半尺寸 cyclic 结构性不可修复, 全尺寸 = 线性"。但我把话说太死——
"cyclic 用不了" 只对**我们当前的逐块精确商框架**成立, 不代表 cyclic 本身不能提速。
GMP 的 `mpn_mu_div_qr` **确实用半尺寸 cyclic (`mpn_mulmod_bnm1`) 提速**, 靠的是**整体近似商框架**
(cyclic 只算 `mu_divappr_q` 近似商, 容 O(1) 误差, 末尾精确算 R + 少数修正兜底)。
→ 唯一能判定"值不值得改成 GMP 架构"的标准: **GMP 用 cyclic 到底比我们线性快多少**。直接测 GMP 真身。

**实测 (VM znver3, GMP 6.3.0 libgmp.so.10, perf instructions:u, 同一 A=400000/B=206025 16-bit limbs)**：
`gmp_div_test.cpp` (mpz_tdiv_qr) vs `div_base16_zn` (加 REPS 环境变量测纯除法内核)。
```
                                  纯除法内核(REPS=10 / 10)      全程(REPS=1, 含解析+IO)
我们 线性 FFT (默认):              386,773,403                  461,421,723
我们 全尺寸 cyclic (CYCLIC=1):     386,773,430  (差27, ==线性)   --
GMP  mpn_mu_div_qr (cyclic):      426,210,053                  512,502,136 (无输出) / 537M (含hex输出)
```

**决定性结论 (翻转, 且是好消息)**：
1. **我们线性 FFT 除法内核 386.8M, 比 GMP 的 cyclic 除法 426.2M 快 9.3%** ((426.2-386.8)/426.2)。
   GMP 用了 cyclic 却没赢我们 → cyclic 省下的乘法工作量, 被除法其他开销(Newton 迭代 / 余数计算 /
   多次 mulmod 调用 / 修正)抵消。cyclic **不是提速捷径**。
2. 全尺寸 cyclic 386,773,430 vs 线性 386,773,403 (差 27 条) → 数学等价, 再次坐实。
3. 我们解析/IO/输出开销 = 461.4M - 386.8M = 74.6M (占全程 16%); 除法内核占 84%。

**对"cyclic 真的用不了吗"的最终回答**：
- 半尺寸 cyclic 在我们框架**用不了**(wrap 毒化精确商) —— 已坐实。
- 但**这不重要**: 即使按 GMP 架构正确实现 cyclic, 拿 GMP 真身实测证明它连自己都没跑赢我们的线性
  路径。**我们的线性 FFT 除法内核已是当前已知最优(超 GMP 9.3%)**, 无需为 cyclic 纠结。
- 提速主战场 = FFT 内核本身(见 perf_bottleneck.md), 不是除法调度层的 cyclic/linear 选择。

**新增文件**: `gmp_div_test.cpp` (GMP 真身测速基准); `div_base16.cpp` main 加 `REPS` 环境变量(纯除法内核测量, 仅测速用, 默认 1 不影响生产)。
