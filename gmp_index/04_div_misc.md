# GMP 索引 04: 通用除法 (div_q + tdiv_qr + divrem + bdiv_q + divexact)

## 源文件信息
- div_q.c: 313 行
- tdiv_qr.c: 386 行
- divrem.c: 103 行
- bdiv_q.c: 76 行
- divexact.c: 296 行

## 函数索引 TREE

### div_q.c

#### `mpn_div_q` (L99-L313)
- **签名**: `void mpn_div_q(mp_ptr qp, mp_srcptr np, mp_size_t nn, mp_srcptr dp, mp_size_t dn, mp_ptr scratch)`
- **功能**: 通用除法（截断商），Q = N/D 不带余数；输入不被破坏，支持 `np==scratch` 复用以省一次拷贝。
- **关键宏** (L87-L97):
  - `FUDGE = 5`：判定何时用高段近似商调整；要求 `>= 2` 才正确。
  - `DC_DIV_Q_THRESHOLD = DC_DIVAPPR_Q_THRESHOLD`
  - `MU_DIV_Q_THRESHOLD = MU_DIVAPPR_Q_THRESHOLD`
  - `MUPI_DIV_Q_THRESHOLD = MUPI_DIVAPPR_Q_THRESHOLD`（缺省回落到 `MUPI_DIV_QR_THRESHOLD`）
- **入断言** (L112-L119): `nn>=dn`, `dn>0`, `dp[dn-1]!=0`，`qp` 与 `np/dp` 不重叠，`np` 与 `scratch` 同址或分离。
- **dn==1 快速通道** (L122-L126): 直接 `mpn_divrem_1`。
- **主调度分支**: `qn = nn-dn+1` (L128)，根据 `qn + FUDGE >= dn` 二选一。

### tdiv_qr.c

#### `mpn_tdiv_qr` (L46-L386)
- **签名**: `void mpn_tdiv_qr(mp_ptr qp, mp_ptr rp, mp_size_t qxn, mp_srcptr np, mp_size_t nn, mp_srcptr dp, mp_size_t dn)`
- **功能**: 标准截断除法带余数；输出 `nn-dn+1` 商 + `dn` 余数。`qxn` 当前必须为 0（L50 断言）。
- **入断言** (L52-L56): `nn>=0`, `dn>=0`, `dp[dn-1]!=0`，无重叠。
- **switch(dn) 三分支**:
  - `dn==0`: 触发 `DIVIDE_BY_ZERO` (L60-L61)
  - `dn==1`: 调 `mpn_divrem_1` (L63-L67)
  - `dn==2`: 局部归一化后调 `mpn_divrem_2`，恢复余数 (L69-L104)
  - `default`: 大分支 (L106-L384)，再按 `nn+adjust >= 2*dn` 分两路

### divrem.c

#### `mpn_divrem` (L35-L103)
- **签名**: `mp_limb_t mpn_divrem(mp_ptr qp, mp_size_t qxn, mp_ptr np, mp_size_t nn, mp_srcptr dp, mp_size_t dn)`
- **功能**: 中间层，本质调用 `mpn_tdiv_qr`；返回最高商 limb。要求 `dp[dn-1] & GMP_NUMB_HIGHBIT`（已归一化）。
- **三分支**:
  - `dn==1` (L50-L67): `mpn_divrem_1` + 临时商区拷回
  - `dn==2` (L68-L71): 直接 `mpn_divrem_2`
  - `dn>=3` (L72-L102): `qxn!=0` 时在 `np` 前 `qxn` 个 0 拼接；调 `mpn_tdiv_qr`，拷回商，返回 `qhl`

### bdiv_q.c

#### `mpn_bdiv_q` (L42-L67)
- **签名**: `void mpn_bdiv_q(mp_ptr qp, mp_srcptr np, mp_size_t nn, mp_srcptr dp, mp_size_t dn, mp_ptr tp)`
- **功能**: Hensel（2-adic）除法，Q = N/D mod B^nn，仅商。`tp` 为 scratch。
- **调度**:
  - `BELOW_THRESHOLD(dn, DC_BDIV_Q_THRESHOLD)` → `mpn_sbpi1_bdiv_q` (L50-L55)
  - `BELOW_THRESHOLD(dn, MU_BDIV_Q_THRESHOLD)` → `mpn_dcpi1_bdiv_q` (L56-L61)
  - 否则 → `mpn_mu_bdiv_q` (L62-L65)
- **预处理**: 拷 `np`→`tp`，`binvert_limb(di, dp[0]); di = -di;` 提供低位逆元。

#### `mpn_bdiv_q_itch` (L69-L76)
- **签名**: `mp_size_t mpn_bdiv_q_itch(mp_size_t nn, mp_size_t dn)`
- **功能**: 返回 scratch 大小；`MU` 阈值之下返回 `nn`，之上返回 `mpn_mu_bdiv_q_itch(nn,dn)`。

### divexact.c

文件含两套实现，由 `#if 1` (L43) / `#else` (L106) 切换；当前激活的是第一套。

#### `mpn_divexact`（激活版，L44-L104）
- **签名**: `void mpn_divexact(mp_ptr qp, mp_srcptr np, mp_size_t nn, mp_srcptr dp, mp_size_t dn)`
- **功能**: 已知整除的精确除法。等价于 `bdiv_q` + 取负。
- **步骤**:
  1. (L58-L65) 跳过低位为 0 的 `dp[0]`（同步前移 `np/dp`）
  2. (L67-L71) `dn==1` 走 `MPN_DIVREM_OR_DIVEXACT_1`
  3. (L75-L93) `count_trailing_zeros(shift, dp[0])`，若 `shift>0` 右移 `dp` 与 `np` 的低位段（仅 `ss = min(dn, qn+1)` 长度，以省时间）
  4. (L95-L96) `if (dn > qn) dn = qn` 截断除数到商长
  5. (L98-L99) `mpn_bdiv_q(qp, np, qn, dp, dn, tp)`
  6. (L103) `mpn_neg(qp, qp, qn)` —— 因为 `bdiv_q` 返回 `-N/D mod B^qn`

#### `mpn_divexact_itch`（仅 else 分支，L127-L131）
- **签名**: `mp_size_t mpn_divexact_itch(mp_size_t nn, mp_size_t dn)`
- **功能**: 注释明确说 "FIXME this is not right"，返回 `nn + dn`。

#### `mpn_divexact`（Jebelean 双向版，#else，L133-L295）
- **签名**: `void mpn_divexact(mp_ptr qp, mp_srcptr np, mp_size_t nn, mp_srcptr dp, mp_size_t dn, mp_ptr scratch)`
- **功能**: Jebelean 双向精确除法：低位 2-adic + 高位 truncating。
- **关键阈值**: `DIVEXACT_JEB_THRESHOLD`, `DC_BDIV_Q_THRESHOLD`, `MU_BDIV_Q_THRESHOLD`, `DC_DIVAPPR_Q_THRESHOLD`, `MU_DIVAPPR_Q_THRESHOLD`。
- **结构**:
  - 小除数或小商：直接 `mpn_sbpi1_bdiv_q` (L157-L166)
  - `qn0 > dn` 时退回单边 bdiv (L172-L195)
  - 否则分两段 (L168-L215)：`qn0 = ((nn-dn)>>1)+1`，`nn1/qn1` 计算高段
  - 高段归一化 (L219-L251)：`count_leading_zeros(cnt, dp[dn-1])` 后 `mpn_lshift` 除数和被除数
  - 高段近似商 (L253-L272)：sbpi1/dcpi1/mu_divappr_q 三选一，mu 分支前先 `mpn_cmp`+条件减处理 qh
  - 低段 bdiv (L274-L289)：sbpi1/dcpi1/mu_bdiv_q 三选一
  - 衔接修正 (L291-L292)：`if (qml < qp[qn0-1]) mpn_decr_u(qp+qn0, 1)`

## 关键算法步骤详解

### div_q 的调度逻辑（主路径一：qn+FUDGE >= dn，L130-L207）

适用：除数与商大小相当（"长除数"情形）。流程：

1. **归一化判定** (L136)：`if (LIKELY((dh & GMP_NUMB_HIGHBIT) == 0))` —— 若除数最高位未置 1，进入归一化分支。
2. **归一化** (L137-L146):
   - `count_leading_zeros(cnt, dh)` 统计 `dh` 前导 0
   - `mpn_lshift(new_np, np, nn, cnt)`，捕获 `cy`，`new_np[nn] = cy`，`new_nn = nn + (cy!=0)`
   - `mpn_lshift(new_dp, dp, dn, cnt)`（新分配 `dn` limb）
3. **子算法选择**（已归一化分支 L147-L175 / 未归一化分支 L181-L205 完全对称）：
   - `dn==2` → `mpn_divrem_2`
   - `BELOW_THRESHOLD(dn, DC_DIV_Q_THRESHOLD) || BELOW_THRESHOLD(new_nn-dn, DC_DIV_Q_THRESHOLD)` → `mpn_sbpi1_div_q`（schoolbook + 预逆 `invert_pi1`）
   - **慢条件组合** (L157-L161 / L191-L194)：
     - `BELOW_THRESHOLD(dn, MUPI_DIV_Q_THRESHOLD)` 快条件
     - `BELOW_THRESHOLD(nn, 2*MU_DIV_Q_THRESHOLD)` 快条件
     - `2*(MU_DIV_Q_THRESHOLD - MUPI_DIV_Q_THRESHOLD)*dn + MUPI_DIV_Q_THRESHOLD*nn > dn*nn` 慢条件（避免 MU 调度不划算的边界）
     - 满足任一 → `mpn_dcpi1_div_q`（divide & conquer + pi1 逆元）
   - 否则 → `mpn_mu_div_q`（Möller 算法），分配 `mpn_mu_div_q_itch(new_nn, dn, 0)` scratch
4. **carry 修正** (L171-L174): 归一化分支里 `if (cy==0) qp[qn-1]=qh; else ASSERT(qh==0)`。因为左移产生 carry 时多出一位，`new_nn` 比 `nn` 大 1，此时商高位已被填到 `qp[qn-2]`，`qh` 必为 0。

### div_q 的调度逻辑（主路径二：qn+FUDGE < dn，L208-L310）

适用：除数远大于商（"短商"情形）。仅取被除数高 `2*qn+1` 段做近似除，最后用乘法验证修正。

1. **scratch 分配** (L212-L219)：`tp = qn+1`；`new_np = scratch`，若 `scratch==np` 则改重新分配 `new_nn+1`，避免破坏 `np`。
2. **`new_nn = 2*qn+1`** (L215)。
3. **归一化分支** (L222-L267):
   - `count_leading_zeros(cnt, dh)`，`mpn_lshift(new_np, np+nn-new_nn, new_nn, cnt)`
   - 除数取 `qn+1` 段并左移，并把 `dp[dn-(qn+1)-1]` 的低位右移拼入 `new_dp[0]`（L233）以保留一位 overlap
   - 子算法走 `divappr_q`（近似商，非精确）：
     - `qn+1==2` → `mpn_divrem_2`
     - `BELOW_THRESHOLD(qn, DC_DIVAPPR_Q_THRESHOLD-1)` → `mpn_sbpi1_divappr_q`
     - `BELOW_THRESHOLD(qn, MU_DIVAPPR_Q_THRESHOLD-1)` → `mpn_dcpi1_divappr_q`
     - 否则 → `mpn_mu_divappr_q`
   - **carry 修正** (L255-L266):
     - `cy==0` 时 `tp[qn]=qh`
     - `cy!=0 && qh!=0` 罕见：意味着近似商返回 `B^n`，此时把 `tp[0..n-1]` 全置 `GMP_NUMB_MAX`，`qh=0`（注释 L259-L261）
4. **未归一化分支** (L268-L295): 与上对称，但 `new_dp = dp + dn - (qn+1)`（指针直指原 `dp`），不需要左移。
5. **拷贝商** (L297): `MPN_COPY(qp, tp+1, qn)` —— `tp[0]` 是低 carry，丢弃。
6. **修正** (L298-L309):
   - 仅当 `tp[0] <= 4` 才修正（启发式：误差不会超过 4）
   - `mpn_mul(rp, dp, dn, tp+1, qn)` 计算 `D*Q`
   - 若 `rn > nn` 或 `mpn_cmp(np, rp, nn) < 0` → `MPN_DECR_U(qp, qn, 1)` 商减 1

### tdiv_qr 的归一化与去归一化（dn>=3，大分支 L106-L384）

#### 子分支 A：`nn+adjust >= 2*dn`（L113-L163）

`adjust = np[nn-1] >= dp[dn-1]`（保守判断是否多一位商）。

- **L119**: `qp[nn-dn] = 0` 先把最高商位置 0。
- **L120-L130 归一化**: 若 `dp[dn-1] & GMP_NUMB_HIGHBIT == 0`：
  - `count_leading_zeros(cnt, dp[dn-1])`, `cnt -= GMP_NAIL_BITS`
  - `mpn_lshift(d2p, dp, dn, cnt)`
  - `mpn_lshift(n2p, np, nn, cnt)` 捕获 `cy` 写入 `n2p[nn]`
  - `nn += adjust` 把 adjust 体现到 nn
- **L131-L139 未归一化**: `cnt=0`，`d2p = dp`，拷贝 `np` 到 `n2p`，`n2p[nn]=0`，`nn += adjust`。
- **L141 子算法**:
  - `BELOW_THRESHOLD(dn, DC_DIV_QR_THRESHOLD)` → `mpn_sbpi1_div_qr`
  - 慢条件组合 (L144-L148): 同 div_q 的 MU 判定公式 → `mpn_dcpi1_div_qr`
  - 否则 → `mpn_mu_div_qr`，注意 `n2p = rp` 复用余数区
- **L157-L160 去归一化**: `if (cnt != 0) mpn_rshift(rp, n2p, dn, cnt); else MPN_COPY(rp, n2p, dn)`。

#### 子分支 B：`nn+adjust < 2*dn`（L165-L384，"短商" qn << dn 情形）

注释算法 (L169-L199)：取高 `2*qn` 段做近似商，逐次比对、必要时减去除数。

- **L209-L211**: `qn = nn-dn; qp[qn]=0; qn += adjust`。
- **L213-L218 qn==0 退化**: 直接 `MPN_COPY(rp, np, dn)` 返回。
- **L220-L257 归一化**:
  - `in = dn - qn`（被忽略的低位段长度）
  - `count_leading_zeros(cnt, dp[dn-1]); cnt -= GMP_NAIL_BITS`
  - `mpn_lshift(d2p, dp+in, qn, cnt)` + 把 `dp[in-1]` 的高位移入 `d2p[0]`（L231）
  - `mpn_lshift(n2p, np+nn-2*qn, 2*qn, cnt)`
  - `adjust` 时 `n2p[2*qn]=cy; n2p++`；否则把 `np[nn-2*qn-1]` 的位移入 `n2p[0]`
- **L260-L286 子算法**:
  - `qn==1` 直接 `udiv_qrnnd`
  - `qn==2` 走 `mpn_divrem_2`
  - 否则 sbpi1/dcpi1/mu_div_qr 三选一（与 A 对称）
- **L288-L325 第一道修正**（注释 L289-L293）:
  - 取 `dl = dp[in-2]`（若 `in-2<0` 取 0）
  - 构造 `x = (dp[in-1] << cnt) | (dl >> ...)`，`umul_ppmm(h, dummy, x, qp[qn-1])`
  - 若 `n2p[qn-1] < h` → `mpn_decr_u(qp, 1)` + `mpn_add_n(n2p, n2p, d2p, qn)`，carry 处理 `n2p[qn]=cy; ++rn`
- **L327-L351 carry 合并**（`cnt!=0` 时）:
  - `mpn_lshift(n2p, n2p, rn, GMP_NUMB_BITS - cnt)` 把 partial remainder 提回去
  - `n2p[0] |= np[in-1] & (GMP_NUMB_MASK >> cnt)` 补回低位
  - `mpn_submul_1(n2p, qp, qn, dp[in-1] & (GMP_NUMB_MASK >> cnt))` 减去部分积
  - carry 处理 (L338-L349)：`qn != rn` 时 `n2p[qn] -= cy2`；否则 `n2p[qn] = cy1 - cy2`，并设 `quotient_too_large = (cy1 < cy2)`，`--in`
- **L354-L374 完整余数计算**:
  - `tp = TMP_ALLOC_LIMBS(dn)`
  - 根据 `in < qn` 决定 `mpn_mul(tp, qp, qn, dp, in)` 或 `mpn_mul(tp, dp, in, qp, qn)`
  - `mpn_sub(n2p, n2p, rn, tp+in, qn)`
  - `MPN_COPY(rp+in, n2p, dn-in)`
  - `mpn_sub_n(rp, np, tp, in)` + `mpn_sub_1(rp+in, rp+in, rn, cy)`
  - `quotient_too_large |= cy`
- **L375-L380 最终修正**: 若 `quotient_too_large` → 商减 1、余数加 `dp`。

### bdiv_q 的调度（Hensel 除法）
- 仅依据 `dn` 阈值三选一，无归一化（Hensel 用低位逆元 `binvert_limb`）。
- scratch 在小分支里只需 `nn`（用作 `np` 的可变副本）；大分支交给 `mpn_mu_bdiv_q` 自管。

### divexact 激活版的关键点
- **去低位 0** (L58-L65)：同步前移 `np/dp`，因为低位 0 在 `bdiv_q` 中无法用 `binvert_limb` 求逆。
- **trailing zeros 右移** (L76-L93)：把 `dp[0]` 的 2-因子移除（同步移 `np` 的低位段），让 `bdiv_q` 的低位逆元计算可行。注意只右移 `ss = min(dn, qn+1)` 长度而非全长，节省时间。
- **dn 截断** (L95-L96)：`if (dn > qn) dn = qn` —— 因为 `qn` 位商只需 `qn` 位除数。
- **取负** (L103)：`bdiv_q` 实际算出 `-N/D mod B^qn`，故最后 `mpn_neg` 还原正商。

## 与 moptm_fusion.cpp 的对应关系

> 以下为概念对应；具体行号需以 `moptm_fusion.cpp` 当前版本为准。

- **通用调度对照 moptm `absDivMu` 的路径选择**：
  - div_q 的 `qn+FUDGE >= dn` 分支对应 moptm 中"长除数"路径（直接 div_qr 类）。
  - div_q 的 `qn+FUDGE < dn` 分支对应 moptm 中"短商 + 近似商 + 乘法修正"路径，即 `absDivMu` 里的 `divappr_q` + `mpn_mul` 验证逻辑。
  - 三档子算法 sbpi1 / dcpi1 / mu 对应 moptm 中按阈值切换 SB / DC / MU 的同一套阈值常量。

- **归一化对照 moptm `divisorNormalizeFactor`**：
  - div_q.c L136-L146 与 tdiv_qr.c L120-L130 中的 `count_leading_zeros(cnt, dh) + mpn_lshift` 序列，对应 moptm `divisorNormalizeFactor` 计算左移量并同步左移 N、D 的操作。
  - 去归一化 `mpn_rshift(rp, n2p, dn, cnt)`（tdiv_qr.c L158）对应 moptm 中余数的 `denormalizeRemainder`。

- **carry 处理对照**：
  - div_q L171-L174 `if (cy==0) qp[qn-1]=qh; else ASSERT(qh==0)` 对应 moptm 中 `normalizeAndDiv` 后的 `carry == 0 ? writeQHigh : assertZero` 模式。
  - tdiv_qr L255-L266 中 `qh != 0 && cy != 0` 的 "B^n 回卷" 处理对应 moptm 中 `handleDivapprOverflow` 的 `fill GMP_NUMB_MAX` 分支。

- **divexact 取负对照**：
  - divexact.c L103 `mpn_neg(qp, qp, qn)` 对应 moptm 中 `bdiv_q` 后的 `negateQuotient` 步骤；若 moptm 直接走 `bdiv_qr` 则无需取负（因为 `bdiv_qr` 内部已处理符号）。

- **bdiv_q 调度对照**：
  - bdiv_q.c L50-L65 的三档阈值（`DC_BDIV_Q_THRESHOLD`、`MU_BDIV_Q_THRESHOLD`）对应 moptm 中 `bdivDispatch` 的同一组阈值。
