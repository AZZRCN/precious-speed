# GMP 索引 01: Newton 逆近似 (invertappr.c + invert.c)

## 源文件信息
- `E:\gmp-6.3.0\mpn\generic\invertappr.c` — 共 300 行
- `E:\gmp-6.3.0\mpn\generic\invert.c` — 共 86 行

## 函数索引 TREE

### invertappr.c

#### `mpn_invertappr` (L287-L300) — 顶层入口/分发器
- **签名**: `mp_limb_t mpn_invertappr (mp_ptr ip, mp_srcptr dp, mp_size_t n, mp_ptr scratch)`
- **返回**: 误差标志 `e`，`0 <= e <= 1`（`e=1` 表示结果可能比真值小 1）
- **功能**: 对归一化输入 `{dp,n}`（最高位必为 1）计算近似逆 `{ip,n}`，满足
  `{dp,n}*(B^n+{ip,n}) < B^{2n} <= {dp,n}*(B^n+{ip,n}+1+e)`
- **算法**: 阈值分发
  - L296-L297: `BELOW_THRESHOLD(n, INV_NEWTON_THRESHOLD)` → `mpn_bc_invertappr`（基例）
  - L298-L299: 否则 → `mpn_ni_invertappr`（Newton 迭代）

#### `mpn_bc_invertappr` (L87-L122) — 基例逆近似（static 内部函数）
- **签名**: `static mp_limb_t mpn_bc_invertappr (mp_ptr ip, mp_srcptr dp, mp_size_t n, mp_ptr xp)`
- **返回**: `e`（多数情况返回 1，仅 n==1 与 n==2 路径返回 0）
- **功能**: 小尺寸基例，使用除法直接计算近似逆
- **分支**:
  - L97-L98: `n==1` — 直接 `invert_limb(*ip, *dp)`
  - L101-L102: `n>1` 预处理 — `MPN_FILL(xp, n, GMP_NUMB_MAX)` 后 `mpn_com(xp+n, dp, n)`，使 `xp = B^{2n} - dp*B^n - 1`
  - L107-L108: `n==2` — `mpn_divrem_2(ip, 0, xp, 4, dp)`（4 = 2n）
  - L109-L119: `n>2` — 用 `invert_pi1` 初始化，然后
    - L112-L114: `BELOW_THRESHOLD(n, DC_DIVAPPR_Q_THRESHOLD)` 或 `!MAYBE_dcpi1_divappr` → `mpn_sbpi1_divappr_q`
    - L115-L116: 否则 → `mpn_dcpi1_divappr_q`
    - L117: `MPN_DECR_U(ip, n, 1)` — 减 1 修正
    - L118: `return 1`
  - L121: `return 0`

#### `mpn_ni_invertappr` (L152-L285) — Newton 迭代主函数
- **签名**: `mp_limb_t mpn_ni_invertappr (mp_ptr ip, mp_srcptr dp, mp_size_t n, mp_ptr scratch)`
- **功能**: Newton 迭代计算近似逆，至少执行一次迭代
- **算法**: Brent–Zimmermann《Modern Computer Arithmetic》算法 3.5（ApproximateReciprocal）的改良版，引入 `B^m-1` 环绕乘积与误差返回值 `e`
- **不变量**: `D * I <= B^{2k}`（下界逆）
- **关键子步骤**:
  - L160: `#define xp scratch` — 别名宏
  - L162-L166: 断言 `n > 4`，dp 归一化，无重叠
  - L168-L176: **精度序列生成** — 从 `n` 递减到基例，存入 `sizes[]` 数组
  - L179-L180: `dp += n; ip += n` — 指针右移，工作于高位
  - L183: `mpn_bc_invertappr(ip - rn, dp - rn, rn, scratch)` — 计算基例逆
  - L185-L191: `TMP_MARK`；若 `ABOVE_THRESHOLD(n, INV_MULMOD_BNM1_THRESHOLD)` 则预分配 `tp` 给 `mulmod_bnm1`
  - L194-L280: **Newton 主循环** `while(1)`（详见下文）
  - L281: `TMP_FREE`
  - L283: `return cy`
  - L284: `#undef xp`

### invert.c

#### `mpn_invert` (L40-L86) — 精确逆（非近似）
- **签名**: `void mpn_invert (mp_ptr ip, mp_srcptr dp, mp_size_t n, mp_ptr scratch)`
- **返回**: 无；结果满足严格不等式 `{dp,n}*(B^n+{ip,n}) < B^{2n} <= {dp,n}*(B^n+{ip,n}+1)`
- **功能**: 计算 `floor((B^{2n}-1)/U) - B^n`（精确逆）
- **分支**:
  - L49-L50: `n==1` — `invert_limb(*ip, *dp)`
  - L51-L68: `BELOW_THRESHOLD(n, INV_APPR_THRESHOLD)` — 直接除法分支
    - L56: `xp = scratch`（2n limbs）
    - L58-L59: `MPN_FILL(xp, n, GMP_NUMB_MAX)` + `mpn_com(xp+n, dp, n)`，构造 `xp = B^{2n} - dp*B^n - 1`
    - L60-L61: `n==2` — `mpn_divrem_2(ip, 0, xp, 4, dp)`
    - L62-L67: `n>2` — `invert_pi1` 初始化 + `mpn_sbpi1_div_q`（注意是 `_div_q`，不是 `_divappr_q`）
  - L69-L85: 大尺寸分支 — 调用近似逆再修正
    - L73: `e = mpn_ni_invertappr(ip, dp, n, scratch)` — 取近似逆
    - L75-L84: `if (UNLIKELY(e))` — 修正 off-by-one
      - L77: `mpn_mul_n(scratch, ip, dp, n)` — 计算 `ip*dp`
      - L78: `e = mpn_add_n(scratch, scratch, dp, n)` — 加 `dp`，捕获进位
      - L79-L80: `if (LIKELY(e)) e = mpn_add_nc(scratch+n, scratch+n, dp, n, e)` — 高半部分加 `dp`
      - L82: `e ^= CNST_LIMB(1)` — 翻转（无进位 → 需 +1 修正）
      - L83: `MPN_INCR_U(ip, n, e)` — 增量修正

## 关键算法步骤详解

### Newton 迭代主循环 (L194-L280)

#### 不变量与迭代公式
- **不变量**: `{dp,rn} * {ip,rn} <= B^{2*rn}`（下界逆，I 偏小）
- **迭代公式**: `I' = I + I * (B^k - D*I) / B^k`（误差平方收敛）
- **实现切分**: `n` 在每一轮被切成 `[n-rn | rn]`，`rn = (n>>1)+1`

#### 步骤 1：精度序列生成 (L168-L176)
```
sizp = sizes; rn = n;
do {
  *sizp = rn; rn = (rn >> 1) + 1; ++sizp;
} while (ABOVE_THRESHOLD (rn, INV_NEWTON_THRESHOLD));
```
- L172-L176: do-while 循环将 `n, n1, n2, ...` 自顶向下存入 `sizes[]`，直到 `rn <= INV_NEWTON_THRESHOLD`
- L183: 用 `rn`（最小精度）调用 `mpn_bc_invertappr` 求基例逆

#### 步骤 2：指针重定位 (L178-L180)
- L179-L180: `dp += n; ip += n` — 让 `dp-rn`、`ip-rn` 指向当前工作段的低位起点

#### 步骤 3：循环内取下一精度 (L194-L200)
- L195: `n = *--sizp` — 从 `sizes[]` 弹出下一目标精度
- L196-L200: 注释图示切分结构 `[v n | v]^rn`

#### 步骤 4：计算 `D * I`（i_jd 步骤）(L202-L223)

**分支 A：截断乘法 (L203-L209)**
- 触发条件：`BELOW_THRESHOLD(n, INV_MULMOD_BNM1_THRESHOLD)` 或 `mn > n+rn`
- L206: `mpn_mul(xp, dp-n, n, ip-rn, rn)` — 普通乘法（截断到 `n+rn` limbs）
- L207: `mpn_add_n(xp+rn, xp+rn, dp-n, n-rn+1)` — 加上 `dp*B^rn` 的低位部分
- L208: `cy = 1` — 记录做了截断（模 `B^(n+1)`）

**分支 B：环绕乘法 mod `B^mn-1` (L210-L223)**
- 触发条件：阈值满足且 `mn <= n+rn`
- L211: `mpn_mulmod_bnm1(xp, mn, dp-n, n, ip-rn, rn, tp)` — 计算 `{ip,rn}*{dp,n} mod (B^mn-1)`
- L215: `ASSERT(n >= mn - rn)`
- L216: `cy = mpn_add_n(xp+rn, xp+rn, dp-n, mn-rn)` — 加 `dp*B^rn mod (B^mn-1)` 的低位
- L217: `cy = mpn_add_nc(xp, xp, dp-(n-(mn-rn)), n-(mn-rn), cy)` — 高位环绕加（带进位输入）
- L219: `xp[mn] = 1` — 设置 DECR_U 的边界
- L220: `MPN_DECR_U(xp+rn+n-mn, 2*mn+1-rn-n, 1-cy)` — 减去 `B^{rn+n}` 的低位部分
- L221: `MPN_DECR_U(xp, mn, 1-xp[mn])` — 若上一行侵蚀了 `xp[mn]`，继续减
- L222: `cy = 0` — 记录工作在 `mod B^mn-1` 下

#### 步骤 5：正/负剩余类处理 (L225-L266)

**正剩余类（"positive" residue class）(L225-L257)**
- 条件：`xp[n] < 2`（即 0 或 1）
- L226: `cy = xp[n]`（0 或 1）
- L227-L242: **条件编译 `HAVE_NATIVE_mpn_sublsh1_n`**
  - 原生分支 L228-L236:
    - L229: `mpn_cmp(xp, dp-n, n) > 0` → `mpn_sublsh1_n`（左移1位减法，等价于 `2*dp`）
    - L234-L235: 否则 → `mpn_sub_n`（减一次 `dp`）
  - 回退分支 L238-L241: 用两次 `mpn_sub_n` 模拟
- L243: 注释 `1 <= cy <= 3 here`
- L244-L256: **条件编译 `HAVE_NATIVE_mpn_rsblsh1_n`**
  - 原生分支 L245-L249: `mpn_rsblsh1_n(xp+n, xp, dp-n, n)`（反向减左移1：`2*dp - xp`）
  - 回退分支 L251-L255: `mpn_sub_n` + `mpn_sub_nc`
- L257: `MPN_DECR_U(ip-rn, rn, cy)` — 从 `ip` 减去累积修正量（`1 <= cy <= 4`）

**负剩余类（"negative" residue class）(L258-L266)**
- 条件：`xp[n] >= 2`（实际断言 `xp[n] >= GMP_NUMB_MAX-1`，L259）
- L260: `MPN_DECR_U(xp, n+1, cy)` — 减去截断 carry
- L261-L264: `if (xp[n] != GMP_NUMB_MAX)` —
  - L262: `MPN_INCR_U(ip-rn, rn, 1)` — ip 增 1
  - L263: `ASSERT_CARRY(mpn_add_n(xp, xp, dp-n, n))` — xp 加 dp
- L265: `mpn_com(xp+2*n-rn, xp+n-rn, rn)` — 取反（`B^rn - 1 - xp`）

#### 步骤 6：计算 `x * I`（x_ju_j 步骤）(L268-L272)
- L269: `mpn_mul_n(xp, xp+2*n-rn, ip-rn, rn)` — 平方尺寸乘法，得到 `{xp, 2*rn}`
- L270: `cy = mpn_add_n(xp+rn, xp+rn, xp+2*n-rn, 2*rn-n)` — 加法累积
- L271: `cy = mpn_add_nc(ip-n, xp+3*rn-n, xp+n+rn, n-rn, cy)` — 写入 `ip` 的新高半部分
- L272: `MPN_INCR_U(ip-rn, rn, cy)` — carry 传播到 `ip` 低半

#### 步骤 7：退出判定与保守 carry (L273-L279)
- L273: `if (sizp == sizes)` — 已处理到最小精度（栈底），退出
- L275: `cy = xp[3*rn-n-1] > GMP_NUMB_MAX - 7` — **保守 carry 检测**（用 7 作阈值留余量）
- L277: `break`
- L279: `rn = n` — 准备下一轮迭代

### 截断逆 (Truncated Inverse) 处理 (L203-L223)

- **截断分支（分支 A, L203-L209）**：使用普通 `mpn_mul` 后只保留 `{xp, n+1}` limbs，相当于 `mod B^(n+1)`。`cy=1` 标记后续需要补偿截断误差。
- **环绕分支（分支 B, L210-L223）**：使用 `mpn_mulmod_bnm1`（mod `B^mn-1`），需要：
  - 加回 `dp*B^rn mod (B^mn-1)`（L216-L217）
  - 减去 `B^{rn+n}`（L219-L221）
  - `cy=0` 标记无需补偿

### 上界/下界修正逻辑 (L225-L266)

**正剩余类（`xp[n] < 2`，D*I 偏小，I 是下界逆）**：
- L226: `cy = xp[n]`（0 或 1，源自截断或环绕）
- L228-L242: 检查 `xp`（= `D*I - B^k` 的剩余）是否 `> dp`；若是，则需多减一次 `dp`（因为 `2*(D*I - B^k) >= D`，对应 `I` 应增 1），通过 `mpn_sublsh1_n`（减 `2*dp`）一步完成
- L244-L256: 第二阶段 `mpn_rsblsh1_n` / `mpn_sub_nc` 处理 `2*dp - xp`，等价于计算 `2*dp - (D*I - B^k)` 的低位部分
- L257: `MPN_DECR_U(ip-rn, rn, cy)` — 从 `I` 减去修正（`cy` 范围 1..4，对应 1..2 次减 `dp` 的进位 + 截断 `cy`）

**负剩余类（`xp[n] >= GMP_NUMB_MAX-1`，D*I 偏大）**：
- L260: `MPN_DECR_U(xp, n+1, cy)` — 减去截断 carry，回到无截断状态
- L261-L264: 若 `xp[n] != GMP_NUMB_MAX`，说明 `D*I >= B^{rn+n}`，I 偏大 1
  - L262: `MPN_INCR_U(ip-rn, rn, 1)` — **修正方向相反**：ip 增 1（此处配合下一步 `mpn_com` 实现等价修正）
  - L263: `mpn_add_n(xp, xp, dp-n, n)` — xp 加 dp
- L265: `mpn_com(xp+2*n-rn, xp+n-rn, rn)` — 取补，将负剩余转为正剩余形式供步骤 6 使用

### mpn_invert 的 off-by-one 修正 (invert.c L75-L84)
- L77: `mpn_mul_n(scratch, ip, dp, n)` — 全长乘法 `I*D`
- L78: `mpn_add_n(scratch, scratch, dp, n)` — 加 `D`，检查是否产生进位
  - 若产生进位：`I*D + D >= B^{2n}`，即 `I+1` 是真值 → `e=1` 翻转为 0，不修正
  - 若无进位：`I*D + D < B^{2n}`，I 偏小 → `e=1` 翻转为 1，`ip` 增 1
- L82: `e ^= 1` — 翻转
- L83: `MPN_INCR_U(ip, n, e)` — 增量修正

## 关键宏/常量

### invertappr.c 内部宏 (L51-L65)
- **`NPOWS`** (L52-L53, L56-L57): `sizes[]` 数组的最大容量
  - TUNE_PROGRAM_BUILD 分支：`8*sizeof(mp_size_t)`（或 48，当 `sizeof(mp_size_t)>6`）
  - 正常分支：减去 `LOG2C(INV_NEWTON_THRESHOLD)`，因为最小精度不低于该阈值
- **`MAYBE_dcpi1_divappr`** (L54, L58-L59): `INV_NEWTON_THRESHOLD < DC_DIVAPPR_Q_THRESHOLD`
  - 仅当两阈值关系满足时，`mpn_bc_invertappr` 才可能走 `mpn_dcpi1_divappr_q` 分支
- **`INV_MULMOD_BNM1_THRESHOLD` 重定义** (L60-L64): 若 `INV_NEWTON_THRESHOLD > INV_MULMOD_BNM1_THRESHOLD` 且 `INV_APPR_THRESHOLD > INV_MULMOD_BNM1_THRESHOLD`，则强制重定义为 0（Newton 路径下总是用 `mulmod_bnm1`）

### 来自 gmp-impl.h 的阈值
- **`INV_NEWTON_THRESHOLD`**: Newton 迭代方法启用阈值；`n < 该值` 走 `mpn_bc_invertappr`
- **`INV_APPR_THRESHOLD`**: `mpn_invert` 中近似逆分支阈值；`n < 该值` 走直接除法
- **`INV_MULMOD_BNM1_THRESHOLD`**: 启用 `mpn_mulmod_bnm1`（mod `B^m-1` 环绕乘）的阈值
- **`DC_DIVAPPR_Q_THRESHOLD`**: `mpn_dcpi1_divappr_q` 启用阈值（基例内部的 divide-and-conquer 切换）

### 关键宏（来自 gmp-impl.h）
- **`invert_limb`**: 单 limb 逆的宏（n==1 基例）
- **`invert_pi1`**: 双 limb 初始化除数前导逆的宏
- **`MPN_FILL`**: 用指定 limb 填充一段内存
- **`MPN_INCR_U` / `MPN_DECR_U`**: 带进位传播的加/减 1 操作（跨多 limb）
- **`GMP_NUMB_MAX`**: `B-1`（一个 limb 全 1）
- **`GMP_NUMB_HIGHBIT`**: `B/2`（最高位 bit）
- **`CNST_LIMB(x)`**: 编译期 limb 常量
- **`BELOW_THRESHOLD` / `ABOVE_THRESHOLD`**: 阈值比较宏

### mul 函数调用一览
| 函数 | 调用位置 | 用途 |
|------|----------|------|
| `mpn_mul` | L206 | 截断乘法 `D*I`（分支 A） |
| `mpn_mulmod_bnm1` | L211 | 环绕乘法 `D*I mod (B^mn-1)`（分支 B） |
| `mpn_mul_n` | L269 | 平方尺寸乘 `correction * I` |
| `mpn_mul_n` | invert.c L77 | `mpn_invert` 修正时 `ip*dp` |
| `mpn_add_n` | L207, L216, L270, invert.c L78 | 多 limb 加法 |
| `mpn_add_nc` | L217, L271, invert.c L80 | 带进位输入的加法 |
| `mpn_sub_n` | L235, L239, L252, L255 | 多 limb 减法 |
| `mpn_sub_nc` | L249, L255 | 带进位输入的减法 |
| `mpn_sublsh1_n` | L231 | 左移1位减法（原生优化） |
| `mpn_rsblsh1_n` | L246 | 反向减左移1（原生优化） |
| `mpn_com` | L265, invert.c L59 | 取反（`B^n-1-x`） |
| `mpn_divrem_2` | L108, invert.c L61 | n==2 时的除法 |
| `mpn_sbpi1_divappr_q` | L114 | 基例小尺寸除法（近似商） |
| `mpn_dcpi1_divappr_q` | L116 | 基例大尺寸除法（近似商） |
| `mpn_sbpi1_div_q` | invert.c L66 | 精确商除法（注意区别） |

## 与 moptm_fusion.cpp 的对应关系

> 以下对应关系基于任务描述中的线索（`absInvNewtonGMP` 大致在 moptm_fusion.cpp L3180-L3260）；具体行号需以 moptm_fusion.cpp 实际源码为准。

- **`mpn_invertappr` ↔ `absInvNewtonGMP`**（moptm_fusion.cpp L3180-L3260 附近）
  - GMP: 顶层分发器，根据 `INV_NEWTON_THRESHOLD` 选择基例或 Newton
  - moptm: 对应入口函数，调用 Newton 迭代核心

- **`mpn_ni_invertappr` ↔ Newton 迭代核心**
  - GMP: 主循环 `while(1)` (L194-L280)，使用 `sizes[]` 数组管理多级精度
  - moptm: 类似的多级精度迭代

### 关键差异
| 维度 | GMP (invertappr.c) | moptm_fusion.cpp |
|------|---------------------|-------------------|
| **limb 类型** | `mp_limb_t`（64-bit 无符号） | `uint16_t` limbs，`BASE=10^4` |
| **乘法核心** | `mpn_mulmid` 不直接使用；改用 `mpn_mulmod_bnm1`（mod `B^m-1` 环绕乘，L211）+ `mpn_mul`（截断，L206）+ `mpn_mul_n`（L269） | `fftMulModBm1`（cyclic，FFT mod `B^m-1`） |
| **误差返回值** | `mp_limb_t e`，0 或 1 | （依实现而定） |
| **基例除法** | `mpn_sbpi1_divappr_q` / `mpn_dcpi1_divappr_q` / `mpn_divrem_2` | moptm 自有除法 |
| **阈值** | `INV_NEWTON_THRESHOLD`、`INV_MULMOD_BNM1_THRESHOLD`、`INV_APPR_THRESHOLD`、`DC_DIVAPPR_Q_THRESHOLD` | moptm 自定义阈值 |
| **正/负剩余类处理** | 分支 L225-L266，含 `mpn_sublsh1_n` / `mpn_rsblsh1_n` 原生优化 | moptm 简化版本 |
| **carry 保守检测** | L275: `xp[3*rn-n-1] > GMP_NUMB_MAX - 7` | moptm 类似保守判定 |
| **环绕乘补偿** | L216-L221: 加 `dp*B^rn mod (B^mn-1)` + 减 `B^{rn+n}` | moptm 对应 FFT cyclic 乘法补偿 |

### 概念对应
- GMP 的 "positive/negative residue class"（L225, L258）↔ moptm 中 D*I 与 `B^k` 的比较分支
- GMP 的 `cy` 截断标记（L208, L222）↔ moptm 中截断误差标志
- GMP 的 `MPN_DECR_U(ip-rn, rn, cy)` 修正（L257）↔ moptm 中 I 的下界修正
- GMP 的 `mpn_com`（L265，负剩余类转正）↔ moptm 中等价的取补操作

## 文件结构总览

```
invertappr.c (300 行)
├── L1-L42     版权与说明注释
├── L44-L45    #include "gmp-impl.h", "longlong.h"
├── L47-L49    FIXME 注释
├── L51-L65    宏定义: NPOWS, MAYBE_dcpi1_divappr, INV_MULMOD_BNM1_THRESHOLD 重定义
├── L67-L84    三函数文档注释（mpn{,_bc,_ni}_invertappr 接口契约）
├── L86-L122   ★ mpn_bc_invertappr (static, 基例)
├── L124-L150  mpn_ni_invertappr 算法注释
├── L152-L285  ★ mpn_ni_invertappr (Newton 迭代主函数)
└── L287-L300  ★ mpn_invertappr (顶层分发器)

invert.c (86 行)
├── L1-L35     版权注释
├── L37-L38    #include
├── L40-L86    ★ mpn_invert (精确逆，调用 mpn_ni_invertappr + 修正)
```

**函数总数**: 4 个（`mpn_bc_invertappr`, `mpn_ni_invertappr`, `mpn_invertappr`, `mpn_invert`）
