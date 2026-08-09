# GMP 索引 02: mu-based 除法 (mu_divappr_q + mu_div_qr + mu_div_q)

## 源文件信息
- mu_divappr_q.c: 368 行
- mu_div_qr.c: 417 行
- mu_div_q.c: 184 行

> 三个文件共享同一算法框架（Möller-Granlund ICMS 2006），代码高度相似，注释明确指出
> "This code and the code in mu_divappr_q.c should be edited in sync."
> 因此 mu_divappr_q.c 是核心参考，其余两文件可视为其变种。

## 函数索引 TREE

### mu_divappr_q.c

#### `mpn_mu_divappr_q` (L77-L159)
- **签名**: `mp_limb_t mpn_mu_divappr_q (mp_ptr qp, mp_srcptr np, mp_size_t nn, mp_srcptr dp, mp_size_t dn, mp_ptr scratch)`
- **功能**: 计算 `Q = floor(N / D) + e`，N 为 nn limbs，D 为 dn limbs 且必须归一化，Q 为 nn-dn limbs，`0 <= e <= 4`（近似商，无余数保证）
- **算法**: 基于 mu 逆（`mpn_invertappr`）的 Newton-Barrett 除法；用比标准 Barrett 更小的逆（in limbs 而非 dn+1 limbs）节省 Newton 迭代开销，代价是商可能偏大最多 4
- **入口处理**:
  - L91: `qn = nn - dn`
  - L94-L100: 若 `qn + 1 < dn`，截断 np/np/dp 使 `dn = qn + 1`（保证 in <= dn）
  - L103: `in = mpn_mu_divappr_q_choose_in (qn, dn, 0)` 选择逆元大小
- **逆元计算（L106-L154，活动分支 #if 1）**:
  - L110-L111: `ip = scratch; tp = scratch + in + 1`
  - L114-L120 (`dn == in`): 构造 `{1, dp[0..in-1]}` 为 `in+1` 位数，调用 `mpn_invertappr (ip, tp, in+1, tp+in+1)`，然后 `MPN_COPY_INCR (ip, ip+1, in)`（丢掉最高位，因隐含为 1）
  - L121-L131 (`dn != in`): 对 `dp + dn - (in+1)` 的 `in+1` 位先 `+1`，再 `mpn_invertappr`；若 `+1` 溢出则 `ip = 0`（极端情况）
- **派发**: L156 `qh = mpn_preinv_mu_divappr_q (qp, np, nn, dp, dn, ip, in, scratch + in)`

#### `mpn_preinv_mu_divappr_q` (L161-L309) 【核心循环，static】
- **签名**: `static mp_limb_t mpn_preinv_mu_divappr_q (mp_ptr qp, mp_srcptr np, mp_size_t nn, mp_srcptr dp, mp_size_t dn, mp_srcptr ip, mp_size_t in, mp_ptr scratch)`
- **功能**: 给定预计算逆元 ip（in limbs），执行分块 mu 除法主循环，输出近似商
- **局部宏**: 
  - L176 `#define rp scratch`
  - L177 `#define tp (scratch + dn)`
  - L178 `#define scratch_out (scratch + dn + tn)`
- **初始化**:
  - L180: `qn = nn - dn`
  - L182-L183: `np += qn; qp += qn`（指针移到最高位端，循环从高到低）
  - L185-L189: `qh = mpn_cmp (np, dp, dn) >= 0`；若 np >= dp 则 `qh=1` 并 `mpn_sub_n (rp, np, dp, dn)`，否则 `MPN_COPY (rp, np, dn)` —— 初始 rp 为归一化的余数
  - L191-L192: 退化情况 `qn == 0` 直接返回

#### `mpn_mu_divappr_q_choose_in` (L317-L348) 【static】
- **签名**: `static mp_size_t mpn_mu_divappr_q_choose_in (mp_size_t qn, mp_size_t dn, int k)`
- **功能**: 选择逆元大小 `in`，保证 `in <= dn`
- **策略** (k=0 自动):
  - (a) `qn > dn`: `in = ceil(qn / ceil(qn/dn))`，分块数 = ceil(qn/dn)
  - (b) `dn/3 < qn <= dn`: `in = ceil(qn/2)`，2 块
  - (c) `qn <= dn/3`: `in = qn`，1 块

#### `mpn_mu_divappr_q_itch` (L350-L368)
- **签名**: `mp_size_t mpn_mu_divappr_q_itch (mp_size_t nn, mp_size_t dn, int mua_k)`
- **功能**: 计算 scratch 需求 = `in + MAX(dn + itch_local + itch_out, itch_invapp)`

### mu_div_qr.c

#### `mpn_mu_div_qr` (L96-L155) 【主入口】
- **签名**: `mp_limb_t mpn_mu_div_qr (mp_ptr qp, mp_ptr rp, mp_srcptr np, mp_size_t nn, mp_srcptr dp, mp_size_t dn, mp_ptr scratch)`
- **功能**: 计算 `Q = floor(N/D)` 和 `R = N - QD`（精确商和余数）
- **算法分派**:
  - L109: 若 `qn + MU_DIV_QR_SKEW_THRESHOLD < dn`（默认阈值 100，L82-L84），走 skew 路径
  - L121-L124 (skew 路径): 先用高位 `2*qn+1` 位除以 `qn+1` 位除数得初步 Q 和部分 R
  - L127-L130: `mpn_mul (scratch, dp 或 qp, ...)` 乘以被忽略的低位除数部分
  - L132-L136: 处理 qh 进位
  - L138-L147: 从 np 减去得到完整 R；若 underflow 则 `qh -= mpn_sub_1(qp, qp, qn, 1)` + `mpn_add_n (rp, rp, dp, dn)`
  - L151 (非 skew 路径): 直接 `mpn_mu_div_qr2 (qp, rp, np, nn, dp, dn, scratch)`

#### `mpn_mu_div_qr2` (L157-L231) 【static】
- **签名**: `static mp_limb_t mpn_mu_div_qr2 (mp_ptr qp, mp_ptr rp, mp_srcptr np, mp_size_t nn, mp_srcptr dp, mp_size_t dn, mp_ptr scratch)`
- **功能**: 计算逆元并派发到 `mpn_preinv_mu_div_qr`（与 `mpn_mu_divappr_q` 的逆元计算代码 L178-L226 完全一致）

#### `mpn_preinv_mu_div_qr` (L233-L358) 【核心循环，extern】
- **签名**: `mp_limb_t mpn_preinv_mu_div_qr (mp_ptr qp, mp_ptr rp, mp_srcptr np, mp_size_t nn, mp_srcptr dp, mp_size_t dn, mp_srcptr ip, mp_size_t in, mp_ptr scratch)`
- **功能**: 给定逆元 ip，执行分块 mu 除法主循环，输出**精确**商和余数
- **与 `mpn_preinv_mu_divappr_q` 的关键差异**:
  - rp 是函数参数（输出余数），而非 scratch 别名
  - L266 用 `while (qn > 0)`，而 mu_divappr_q 用 `for (;;)` + 内部 break
  - L261 用 `MPN_COPY_INCR (rp, np, dn)`，mu_divappr_q 用 `MPN_COPY`
  - **无末尾 `+3` 饱和加**（mu_divappr_q 的 L290-L306 在此处不存在），因为需要精确商
  - 其余主循环逻辑（mulhi、mul、sub、修正 1/2）与 mu_divappr_q **逐行对应**

#### `mpn_mu_div_qr_choose_in` (L366-L397) 【static】
- 与 `mpn_mu_divappr_q_choose_in` 完全相同的逻辑

#### `mpn_mu_div_qr_itch` (L399-L408)
- **签名**: `mp_size_t mpn_mu_div_qr_itch (mp_size_t nn, mp_size_t dn, int mua_k)`

#### `mpn_preinv_mu_div_qr_itch` (L410-L417)
- **签名**: `mp_size_t mpn_preinv_mu_div_qr_itch (mp_size_t nn, mp_size_t dn, mp_size_t in)`

### mu_div_q.c

#### `mpn_mu_div_q` (L65-L168)
- **签名**: `mp_limb_t mpn_mu_div_q (mp_ptr qp, mp_srcptr np, mp_size_t nn, mp_srcptr dp, mp_size_t dn, mp_ptr scratch)`
- **功能**: 计算 `Q = floor(N/D)`（精确商，不需要余数）
- **算法**: 调用 `mpn_mu_divappr_q` 得近似商，再用 mul + cmp 修正至精确
- **分支 A: `qn >= dn` (L82-L128)** —— 大商情况
  - L87-L89: `rp = TMP_BALLOC_LIMBS (nn+1); MPN_COPY (rp+1, np, nn); rp[0] = 0` —— 把 N 左移 1 位腾出空间
  - L91-L93: 预减：若 `rp+1+nn-dn >= dp` 则 `mpn_sub_n`
  - L95: `cy = mpn_mu_divappr_q (tp, rp, nn+1, dp, dn, scratch)`
  - L97-L105: 若 `cy != 0`（返回 `B^(qn-dn)+eps` 极端值），用 `GMP_NUMB_MAX` 填充 tp
  - L109: `if (tp[0] > 4)` —— 近似商最大误差 +4，若低位 > 4 则可信
    - L111: `MPN_COPY (qp, tp+1, qn)`
  - L113-L127 (不可信分支): mul + cmp 验证
    - L119: `mpn_mul (pp, tp+1, qn, dp, dn)` —— 计算 Q*D
    - L121: `cy = (qh != 0) ? mpn_add_n (pp+qn, pp+qn, dp, dn) : 0`
    - L123-L124: `if (cy || mpn_cmp (pp, np, nn) > 0)` 则 `qh -= mpn_sub_1 (qp, tp+1, qn, 1)`（最多差 1，无需循环）
    - L125-L126: 否则直接 copy
- **分支 B: `qn < dn` (L129-L164)** —— 小商情况
  - L139-L140: `mpn_mu_divappr_q (tp, np + nn - (2*qn+2), 2*qn+2, dp + dn - (qn+1), qn+1, scratch)` —— 截断除数到 `qn+1` 位
  - L144: `if (tp[0] > 6)` —— 额外误差 +2 来自除数截断，故阈值 6
  - L149-L163 (不可信分支): 类似 mul + cmp 修正

#### `mpn_mu_div_q_itch` (L170-L184)
- **签名**: `mp_size_t mpn_mu_div_q_itch (mp_size_t nn, mp_size_t dn, int mua_k)`
- **功能**: 根据 `qn >= dn` 分支委托给 `mpn_mu_divappr_q_itch`

## 关键算法步骤详解

> 以下详解基于 `mu_divappr_q.c` 的 `mpn_preinv_mu_divappr_q`（L161-L309），
> `mu_div_qr.c` 的 `mpn_preinv_mu_div_qr`（L233-L358）结构完全一致。

### 预处理 (L182-L192)
- **指针高位化**: L182-L183 `np += qn; qp += qn` —— 从最高位开始向低位迭代
- **初始余数归一化**: L185-L189
  - `qh = mpn_cmp (np, dp, dn) >= 0` —— 检查高位 N 是否 >= D
  - 若是：`qh = 1`，`mpn_sub_n (rp, np, dp, dn)` —— 预减一次
  - 若否：`MPN_COPY (rp, np, dn)` —— 直接复制
- **关键不变量**: 进入主循环前 `rp < dp`（归一化），且 `rp` 为 `dn` limbs

### 逆元调用（入口函数 L106-L154 / L178-L226）
- **位置**: `mpn_mu_divappr_q` L118, L128；`mpn_mu_div_qr2` L190, L200
- **构造**: 把 `dp` 的低 `in+1` 位 +1（或前置 1）作为 `in+1` 位数 `d'`，调用 `mpn_invertappr (ip, d', in+1, scratch)`
- **截断**: `MPN_COPY_INCR (ip, ip+1, in)` —— 逆元取低 `in` 位（最高位隐含为 1）
- **极端 fallback**: 若 `+1` 溢出（`cy != 0`），`ip = 0`（L124-L125 / L196-L197）

### 主循环结构 (L194-L285)
```
for (;;) {
  if (qn < in) { ip += in - qn; in = qn; }   // L196-L200: 最后一块可能不足 in
  np -= in; qp -= in;                          // L201-L202: 指针下移
  mpn_mul_n (tp, rp + dn - in, ip, in);        // L206: mulhi 产生 in 位商块
  cy = mpn_add_n (qp, tp + in, rp + dn - in, in); // L207: + R 高位（I 的 msb 隐含 1）
  qn -= in;                                    // L210
  if (qn == 0) break;                          // L211-L212: 最后一块退出
  // 计算 qblock * D
  mpn_mul (tp, dp, dn, qp, in);                // L219: dn+in limbs，高 in 位抵消
  // 或 mpn_mulmod_bnm1 路径 L222-L232
  r = rp[dn - in] - tp[dn];                    // L235: 关键 underflow 探测
  // 减法得到新 rp
  if (dn != in) {                              // L239
    cy = mpn_sub_n (tp, np, tp, in);           // L241: 取 N 新 in 位
    cy = mpn_sub_nc (tp + in, rp, tp + in, dn - in, cy); // L242
    MPN_COPY (rp, tp, dn);                     // L243
  } else {
    cy = mpn_sub_n (rp, np, tp, in);           // L247: dn==in 直接 in-place
  }
  r -= cy;                                     // L254: 把 sub 的 cy 累加到 r
  while (r != 0) { ... }                       // L255-L264: 修正 1
  if (mpn_cmp (rp, dp, dn) >= 0) { ... }       // L265-L271: 修正 2
}
```

### 每块商 chunk 的计算流程

#### 步骤 A: mulhi 计算商块 (L206-L208)
- **L206**: `mpn_mul_n (tp, rp + dn - in, ip, in)` —— 乘法 `R_high * I`
  - `rp + dn - in` 是当前余数的高 `in` 位
  - `ip` 是 `in` 位逆元
  - 结果 `tp` 为 `2*in` limbs，取高 `in` 位 `tp + in` 即为商块（近似）
- **L207**: `cy = mpn_add_n (qp, tp + in, rp + dn - in, in)`
  - **关键**: 加上 `R_high` 自身 —— 因为逆元 `I` 的最高位被截断了，其隐含值为 `1`，所以商 = `R_high * ip + R_high * B^in = R_high * (ip + B^in)`
  - 这是 Möller-Granlund 算法的核心技巧
- **L208**: `ASSERT_ALWAYS (cy == 0)` —— 加法不应产生进位（理论保证）

#### 步骤 B: 计算 qblock * D (L218-L233)
- **路径 1 (小 in, L218-L219)**: `mpn_mul (tp, dp, dn, qp, in)` —— 普通乘法，得 `dn+in` limbs
  - 注释 L219: "high 'in' cancels" —— 高 `in` 位理论上应与 rp 高 in 位抵消
- **路径 2 (大 in, L220-L233)**: `mpn_mulmod_bnm1` 模 `B^tn - 1` 乘法 + 修正
  - L222: `tn = mpn_mulmod_bnm1_next_size (dn + 1)`
  - L223: `mpn_mulmod_bnm1 (tp, tn, dp, dn, qp, in, scratch_out)`
  - L224: `wn = dn + in - tn` —— wrap 起来的 limb 数
  - L225-L232: 修正 wrap 部分（用 `mpn_sub_n` + `mpn_sub_1` + `mpn_incr_u`）

#### 步骤 C: 关键 underflow 探测 (L235)
```c
r = rp[dn - in] - tp[dn];
```
- **意义**: `rp[dn - in]` 是当前余数中刚好高于"被减区域"的那一位
- `tp[dn]` 是 `qblock * D` 的第 `dn` 位（应当与 rp 高位对齐抵消）
- 理论上 `rp[dn-in] == tp[dn]`，差值 `r` 反映商块是否偏大
- **r 是 mp_limb_t（无符号）**，下溢时变为大正数，靠后续 `r -= cy` 累加进位来恢复

#### 步骤 D: 减法得到新余数 (L239-L248)
- **L241 (dn != in)**: `cy = mpn_sub_n (tp, np, tp, in)` —— 从 np 取新 `in` 位 limbs，减去 prod 的低 `in` 位
- **L242**: `cy = mpn_sub_nc (tp + in, rp, tp + in, dn - in, cy)` —— 继续用 rp 减去 prod 的中段，带 carry-in
- **L243**: `MPN_COPY (rp, tp, dn)` —— 把结果复制回 rp
- **L247 (dn == in)**: `cy = mpn_sub_n (rp, np, tp, in)` —— 直接 in-place

#### 步骤 E: 累加 carry 到 r (L254)
```c
r -= cy;
```
- **关键**: 步骤 D 中的 `mpn_sub_n` 若产生 borrow（cy=1），意味着 prod 太大（商偏大），把 cy 累加进 r
- 此时 r 的语义：`r = (rp_high - tp_high) - borrow_from_sub`
- 若 r != 0，说明 qblock 偏大，需要修正

### 修正循环（关键 bug 修复参考点）

#### 修正 1: while (r != 0) 循环 (L255-L264)
```c
r -= cy;                       // L254
while (r != 0)                 // L255
{
  mpn_incr_u (qp, 1);          // L260: qblock += 1
  cy = mpn_sub_n (rp, rp, dp, dn);  // L261: rp -= D
  r -= cy;                     // L262: r -= borrow
  STAT (err++);
}
```
- **触发条件**: 步骤 E 后 `r != 0`（商块偏大 1 或 2）
- **修正方式**: 商 +1，余数 -D，重复直到 r 归零
- **概率注释 L257-L259**: 0 次循环 69%，1 次 31%，2 次 0.6%
- **关键**: `r` 是 mp_limb_t，每次 `r -= cy` 可能让 r 从 0 下溢到大正数，循环会继续；只有真正归零才退出
- **bug 修复重点**: 这里的 `r -= cy` 必须用无符号语义理解 —— 若 sub 产生 borrow，说明 rp 还 >= 0（modular），需要继续减；若 r == 0 且 cy == 0，则 rp 已正确归一化

#### 修正 2: if (mpn_cmp (rp, dp, dn) >= 0) (L265-L271)
```c
if (mpn_cmp (rp, dp, dn) >= 0)  // L265
{
  mpn_incr_u (qp, 1);           // L268: qblock += 1
  cy = mpn_sub_n (rp, rp, dp, dn);  // L269: rp -= D
  STAT (err++);
}
```
- **触发条件**: 修正 1 后 `rp >= dp`（商还偏小 1）
- **概率注释 L267**: 76% 概率执行
- **关键差异**: 修正 1 处理"商偏大"，修正 2 处理"商偏小 1"
- **不循环**: 修正 2 只执行一次，因为 mu 逆的误差上界保证最多偏小 1

### 末尾饱和加 (L287-L306) 【仅 mu_divappr_q，mu_div_qr 无此步】
```c
qn = nn - dn;
cy += mpn_add_1 (qp, qp, qn, 3);    // L291: 商 += 3
if (cy != 0) {
  if (qh != 0) {                     // L294: 已有 qh 进位
    for (i = 0; i < qn; i++) qp[i] = GMP_NUMB_MAX;  // L299: 饱和
  } else {
    qh = 1;                          // L304: 进位到 qh
  }
}
```
- **目的**: 强制 `返回商 >= 真实商`（divappr 语义），最大误差 +4 中预留 +3 给末尾，+1 给循环内
- **bug 修复重点对照**: 这一步确保 divappr 的单向误差，是 `mpn_mu_div_q` 中 `if (tp[0] > 4)` 阈值的依据

## 与 moptm_fusion.cpp 的对应关系
- `mpn_mu_divappr_q` ↔ `absDivMu` (moptm_fusion.cpp L3282-L3424)
- 关键差异: GMP 使用 `mpn_submul_1` / `mpn_sub_n` 修正，moptm 使用 `absSub + absAdd1` 修正
- 关键差异: GMP 直接用 `np` 缓冲区作 in-place 修正，moptm 用 window
- 关键差异: GMP 修正 1 用 `while (r != 0)` 靠 `r -= cy` 无符号下溢驱动，moptm 可能用显式 underflow 标志
- 关键差异: GMP 修正 2 仅 `if (rp >= dp)` 一次，不循环
- **bug 修复重点对照点**:
  - **L235** `r = rp[dn - in] - tp[dn]` —— 对照 moptm 的 underflow 检测点
  - **L254** `r -= cy` —— 对照 moptm 是否正确累加 borrow 到检测变量
  - **L255-L264** while 循环 —— 对照 moptm 修正 1 的循环条件与终止
  - **L265-L271** if 修正 —— 对照 moptm 修正 2 是否存在
  - **L290-L306** 末尾 +3 饱和 —— 仅 divappr 需要，moptm 若实现精确商则不应有此步
