# GMP 索引 03: Middle Product + Cyclic Convolution

## 源文件信息
- mulmid.c: 255 行
- mulmid_n.c: 61 行
- mulmid_basecase.c: 82 行
- mulmod_bnm1.c: 374 行
- mulmod_bknp1.c: 502 行
- sqrmod_bnm1.c: 328 行

## 函数索引 TREE

### mulmid.c

#### `mpn_mulmid` (L44-L255)
- **签名**: `void mpn_mulmid (mp_ptr rp, mp_srcptr ap, mp_size_t an, mp_srcptr bp, mp_size_t bn)`
- **功能**: 计算 middle product，输出 `an - bn + 3` 个 limb（含 2 个 carry limb hi/lo），即 A * B 中所有满足 `bn-1 ≤ i+j ≤ an-1` 的对角线之和
- **前置条件**: `an ≥ bn ≥ 1`，rp 与 ap/bp 不重叠
- **应用**: Newton 逆迭代中计算 D * I 的截断部分；matrix middle product
- **算法**: 根据 `MULMID_TOOM42_THRESHOLD` 与 `CHUNK = 200 + MULMID_TOOM42_THRESHOLD` 分四条路径
  - **L57-L106**: `bn < MULMID_TOOM42_THRESHOLD`（区域不够高，toom42 不划算）
    - L61-L66: 若 `an < CHUNK`，直接 `mpn_mulmid_basecase`
    - L68-L105: 否则沿 an 方向切 CHUNK 大小的块（diagram `AAABBBCC..`），每块 basecase 后通过 `ADDC_LIMB + MPN_INCR_U` 把保存的 `rp[0],rp[1]` 加回去（重叠 1 个对角线）
  - **L108-L163**: `bn ≥ threshold` 但 `rn = an-bn+1 < threshold`（区域不够宽）
    - L118-L123: 若 `bn < CHUNK`，直接 basecase
    - L125-L162: 否则沿 bn 方向切块（diagram `AAAAA....`），每块用 temp 累加 `mpn_add_n`
  - **L165-L209**: `bn > rn`（高大于宽），沿 bn 方向切 rn 大小的块，每块 `mpn_toom42_mulmid`
  - **L210-L254**: `bn ≤ rn`（宽大于等于高），沿 an 方向切 bn 大小的块，每块 `mpn_toom42_mulmid`，最后一块可能递归调用 `mpn_mulmid` 自身

### mulmid_n.c

#### `mpn_mulmid_n` (L41-L61)
- **签名**: `void mpn_mulmid_n (mp_ptr rp, mp_srcptr ap, mp_srcptr bp, mp_size_t n)`
- **功能**: 平衡 middle product，输入 `{ap, 2n-1}` 与 `{bp, n}`，输出 `n+2` limb
- **算法**:
  - L48-L51: `n < MULMID_TOOM42_THRESHOLD` → `mpn_mulmid_basecase(rp, ap, 2n-1, bp, n)`
  - L52-L60: 否则 `mpn_toom42_mulmid(rp, ap, bp, n, scratch)`，scratch = `mpn_toom42_mulmid_itch(n)`

### mulmid_basecase.c

#### `mpn_mulmid_basecase` (L46-L82)
- **签名**: `void mpn_mulmid_basecase (mp_ptr rp, mp_srcptr up, mp_size_t un, mp_srcptr vp, mp_size_t vn)`
- **功能**: 经典 O(un*vn) middle product，输出 `un - vn + 3` limb
- **算法**:
  - L59-L60: `up += vn - 1; un -= vn - 1;`（shift 起点至最右上角的乘数位置）
  - L63-L64: `lo = mpn_mul_1(rp, up, un, vp[0]); hi = 0;`（第一行）
  - L67-L72: 循环 `vn-1` 次，每次 `up--; vp++; cy = mpn_addmul_1(rp, up, un, vp[0]); add_ssaaaa(hi, lo, hi, lo, 0, cy);`（逐行累加，hi/lo 跟踪溢出）
  - L80-L81: `rp[un] = lo; rp[un+1] = hi;`（最后两个 limb 是 carry）
- **边界**: 输出前 `un` (= `an-bn+1`) 个 limb 是真正的 middle product 结果，后 2 个 limb 是高位的 carry (hi, lo)

### mulmod_bnm1.c

#### `mpn_bc_mulmod_bnm1` (L46-L59)
- **签名**: `void mpn_bc_mulmod_bnm1 (mp_ptr rp, mp_srcptr ap, mp_srcptr bp, mp_size_t rn, mp_ptr tp)`
- **功能**: 平衡 basecase：`(A * B) mod (B^rn - 1)`，输入 `{ap,rn}` 与 `{bp,rn}`，scratch `tp` 至少 `2rn` limb
- **算法**:
  - L54: `mpn_mul_n(tp, ap, bp, rn)` — 完整 2rn limb 乘积
  - L55: `cy = mpn_add_n(rp, tp, tp + rn, rn)` — 把高 rn 位加到低 rn 位（cyclic fold）
  - L58: `MPN_INCR_U(rp, rn, cy)` — 处理进位（最多 B^rn - 2，不会溢出）

#### `mpn_bc_mulmod_bnp1` (L66-L100, static)
- **签名**: `static void mpn_bc_mulmod_bnp1 (mp_ptr rp, mp_srcptr ap, mp_srcptr bp, mp_size_t rn, mp_ptr tp)`
- **功能**: basecase `(A * B) mod (B^rn + 1)`，输入 `{ap,rn+1}` 与 `{bp,rn+1}`（高 1 位是符号/carry 标志 0 或 1）
- **算法**:
  - L75-L81: 若任一高位为 1，处理 `±A * ±B` 的符号（`mpn_neg` 取负 mod B^n+1）
  - L82-L92: 若 `MPN_MULMOD_BKNP1_USABLE(rn,k,...)`，转 `mpn_mulmod_bknp1`
  - L93-L97: 否则 `mpn_mul_n(tp, ap, bp, rn); cy = mpn_sub_n(rp, tp, tp+rn, rn);` — 高位减低位（negacyclic fold）
  - L98-L99: `rp[rn] = 0; MPN_INCR_U(rp, rn+1, cy);` — 规范化

#### `mpn_mulmod_bnm1` (L119-L354)
- **签名**: `void mpn_mulmod_bnm1 (mp_ptr rp, mp_size_t rn, mp_srcptr ap, mp_size_t an, mp_srcptr bp, mp_size_t bn, mp_ptr tp)`
- **功能**: 计算 `{rp, MIN(rn, an+bn)} ← {ap,an} * {bp,bn} Mod (B^rn - 1)`（cyclic convolution）
- **前置条件**: `0 < bn ≤ an ≤ rn`，`an + bn > rn/2`
- **scratch**: 至少 `2*rn + 4` limb；`tp == rp` 允许
- **算法**:
  - **L126-L144 (basecase 路径)**: 当 `rn` 为奇数或 `rn < MULMOD_BNM1_THRESHOLD`
    - L128-L133: 若 `an+bn ≤ rn`，直接 `mpn_mul`（无需 fold，因为 `(B^an-1)(B^bn-1) < B^rn-1`）
    - L134-L140: 否则 `mpn_mul(tp, ...)` 然后 `mpn_add(rp, tp, rn, tp+rn, an+bn-rn)` + carry
    - L142-L143: `bn ≥ rn` 时调用 `mpn_bc_mulmod_bnm1`
  - **L145-L353 (递归 CRT 路径)**: `rn` 为偶数且超过阈值
    - 设 `n = rn >> 1`，将 A 分成 `a0 = ap[0..n-1]`，`a1 = ap[n..an-1]`；B 同理
    - **核心公式 (L161-L165)**:
      ```
      x = -xp * B^n + (B^n + 1) * [ (xp + xm)/2 mod (B^n-1) ]
      ```
      其中 `xm = a*b mod (B^n-1)`，`xp = a*b mod (B^n+1)`
    - **L179-L210 (计算 xm)**:
      - L186-L201: `am1 = a0 + a1 (mod B^n-1)`，`bm1 = b0 + b1 (mod B^n-1)`（用 `mpn_add` + `MPN_INCR_U`）
      - L209: 递归 `mpn_mulmod_bnm1(rp, n, am1, anm, bm1, bnm, so)` — 结果 xm 存入 `rp[0..n-1]`
    - **L212-L263 (计算 xp)**:
      - L219-L231: `ap1 = a0 - a1 (mod B^n+1)`，`bp1 = b0 - b1 (mod B^n+1)`（用 `mpn_sub` + `MPN_INCR_U`，注意 B^n+1 下 -x = B^n+1 - x）
      - L237-L245: 选择 FFT 路径 `mpn_fft_best_k(n, 0)`
      - L246-L247: 若 `k >= FFT_FIRST_K`，调用 `mpn_mul_fft(xp, n, ...)` — FFT mod B^n+1
      - L248-L260: 若 `bp1 == b0`（即 bn ≤ n，无需 fold b），用 `mpn_mul` + `mpn_sub` 手工 fold
      - L261-L262: 否则 `mpn_bc_mulmod_bnp1(xp, ap1, bp1, n, xp)`
    - **L265-L315 (CRT 第一步：求 (xm + xp)/2 mod B^n-1)**:
      - L267: `xm <- (xp + xm)/2 = (xp + xm) * B^n/2 mod (B^n-1)`，除以 2 即按位旋转
      - L276-L311: 三种实现：`mpn_rsh1add_nc` / `mpn_rsh1add_n` / `mpn_add_n + mpn_rshift`，处理 `xp[n]` 高位进位
      - L315: `MPN_INCR_U(rp, n, cy)` — 规范化
    - **L317-L346 (CRT 第二步：unwrap / 高半部分重组)** — **【cyclic reconstruction 关键】**
      - 公式: `高半 = ([(xp+xm)/2 mod B^n-1] - xp) * B^n`
      - **L320-L339 (an+bn < rn 的 unwrap 路径)**:
        - L328: `cy = mpn_sub_n(rp + n, rp, xp, an + bn - n)` — `rp[n..an+bn-1] = rp[0..an+bn-n-1] - xp[0..an+bn-n-1]`
        - L333-L334: `cy = xp[n] + mpn_sub_nc(xp + an+bn-n, rp + an+bn-n, xp + an+bn-n, rn-(an+bn), cy)` — 继续减剩余 `rn-(an+bn)` 位（仅为获取 carry 与 sanity check）
        - L337: `cy = mpn_sub_1(rp, rp, an+bn, cy)` — 把 borrow 反传到低位
        - 此时 `rp[0..an+bn-1]` 是最终非 wrap-around 的自然数结果
      - **L340-L346 (an+bn ≥ rn 的标准 wrap 路径)**:
        - L342: `cy = xp[n] + mpn_sub_n(rp + n, rp, xp, n)` — `rp[n..2n-1] = rp[0..n-1] - xp[0..n-1]`
        - L345: `MPN_DECR_U(rp, 2*n, cy)` — 把 borrow 传播到低 n 位（cyclic borrow propagation，因为 mod B^rn-1 等价于把 borrow 加到高位）

#### `mpn_mulmod_bnm1_next_size` (L356-L374)
- **签名**: `mp_size_t mpn_mulmod_bnm1_next_size (mp_size_t n)`
- **功能**: 给定目标 size n，返回 ≥ n 且适合 mulmod_bnm1 递归的尺寸（保证偶数 / 4 对齐 / 8 对齐 / FFT 友好）

### mulmod_bknp1.c

#### `_mpn_modbknp1dbnp1_n` (L47-L107, static)
- **签名**: `static void _mpn_modbknp1dbnp1_n (mp_ptr rp, mp_srcptr op, mp_size_t n, unsigned k)`
- **功能**: `{rp, (k-1)*n} = {op, k*n+1} % (B^{k*n}+1) / (B^n+1)`
- **算法**: 把 `k` 段（每段 n limb）交替加减折叠到前 `k-1` 段，使用 `mpn_add_n` 与 `mpn_sub_n`

#### `_mpn_modbnp1_pn_ip` (L109-L119, static)
- **功能**: 正数规范化 mod B^n+1（处理 `r[n] = h` 的情况）

#### `_mpn_modbnp1_neg_ip` (L121-L128, static)
- **功能**: 负数规范化（`-h` mod B^n+1）

#### `_mpn_modbnp1_nc_ip` (L130-L143, static)
- **功能**: 根据 `h` 符号分派到 pn_ip 或 neg_ip

#### `_mpn_modbnp1` (L147-L167, static)
- **签名**: `static void _mpn_modbnp1 (mp_ptr rp, mp_size_t rn, mp_srcptr op, mp_size_t on)`
- **功能**: `{rp, rn+1} = {op, on} mod (B^rn+1)`，用于 `rn < on < 2*rn`
- **算法**: `mpn_sub(rp, op, rn, op+rn, on-rn)` + INCR 处理 borrow

#### `_mpn_modbnp1_kn` (L171-L196, static)
- **签名**: `static void _mpn_modbnp1_kn (mp_ptr rp, mp_srcptr op, mp_size_t rn, unsigned k)`
- **功能**: `{rp, rn+1} = {op, k*rn+1} % (B^rn+1)`，奇数 `k ≥ 3`
- **算法**: 把 k 段交替 `add_n` / `sub_n` 折叠到 n limb

#### `_mpn_crt` (L261-L374, static)
- **签名**: `static void _mpn_crt (mp_ptr rp, mp_srcptr ap, mp_srcptr bp, mp_size_t n, unsigned k, mp_ptr tp)`
- **功能**: CRT 重组：给定 `{ap,k*n+1} mod (B^{k*n}+1)/(B^n+1)` 和 `{bp,n+1} mod (B^n+1)`，输出 `{rp,k*n+1} mod (B^{k*n}+1)`
- **算法**: 利用 `mpn_mod_34lsub1` 计算 `mod (k)` 的辅助量，再用 `mpn_divexact_by{3,5,7,11,13,17}` 精除，最后交替加减重组

#### `_mpn_mulmod_bnp1_tp` (L377-L407, static)
- **功能**: mod B^rn+1 乘法分派器；处理符号位、转 `mpn_mulmod_bknp1`、或 `mpn_mul_n + sub_n`

#### `mpn_mulmod_bknp1` (L411-L441)
- **签名**: `void mpn_mulmod_bknp1 (mp_ptr rp, mp_srcptr ap, mp_srcptr bp, mp_size_t n, unsigned k, mp_ptr tp)`
- **功能**: `{rp, k*n+1} = {ap,k*n+1} * {bp,k*n+1} % (B^{k*n}+1)`，奇数 `k ≥ 3`
- **scratch**: 至少 `4*(k-1)*n+1` limb
- **算法**:
  - L425-L429: 计算 `(A mod (B^{kn}+1)/(B^n+1)) * (B mod ...)` 用 `mpn_mul_n`，再 `_mpn_modbnp1` 规范化
  - L431-L438: 计算 `(A mod B^n+1) * (B mod B^n+1)` 用 `_mpn_mulmod_bnp1_tp`
  - L440: `_mpn_crt` 重组

#### `_mpn_sqrmod_bnp1_tp` (L444-L473, static)
- **功能**: mod B^rn+1 平方分派器；类似 `_mpn_mulmod_bnp1_tp`，但用 `mpn_sqr`

#### `mpn_sqrmod_bknp1` (L477-L502)
- **签名**: `void mpn_sqrmod_bknp1 (mp_ptr rp, mp_srcptr ap, mp_size_t n, unsigned k, mp_ptr tp)`
- **功能**: `{rp, k*n+1} = {ap,k*n+1}^2 % (B^{k*n}+1)`
- **scratch**: 至少 `3*(k-1)*n+1` limb
- **算法**: 与 `mpn_mulmod_bknp1` 对称，用 `mpn_sqr` 替代 `mpn_mul_n`

### sqrmod_bnm1.c

#### `mpn_bc_sqrmod_bnm1` (L46-L58, static)
- **功能**: 平衡 basecase 平方 mod B^rn-1，`mpn_sqr(tp, ap, rn); cy = mpn_add_n(rp, tp, tp+rn, rn); MPN_INCR_U(rp, rn, cy);`

#### `mpn_bc_sqrmod_bnp1` (L65-L94, static)
- **功能**: basecase 平方 mod B^rn+1；处理 `ap[rn]==1` 时结果为 1；可转 `mpn_sqrmod_bknp1`；否则 `mpn_sqr + mpn_sub_n`

#### `mpn_sqrmod_bnm1` (L111-L308)
- **签名**: `void mpn_sqrmod_bnm1 (mp_ptr rp, mp_size_t rn, mp_srcptr ap, mp_size_t an, mp_ptr tp)`
- **功能**: 计算 `{rp, MIN(rn, 2an)} ← {ap,an}^2 Mod (B^rn-1)`
- **前置条件**: `rn/4 < an ≤ rn`
- **算法**: 与 `mpn_mulmod_bnm1` 结构完全对称
  - **L117-L135 (basecase 路径)**: `rn` 奇数或低于 `SQRMOD_BNM1_THRESHOLD`
    - L121-L124: `2*an ≤ rn` → 直接 `mpn_sqr`
    - L125-L131: 否则 `mpn_sqr + mpn_add + INCR`（cyclic fold）
    - L133-L134: `an ≥ rn` → `mpn_bc_sqrmod_bnm1`
  - **L136-L307 (递归 CRT 路径)**: `rn` 偶数
    - 设 `n = rn >> 1`，`a0 = ap[0..n-1]`，`a1 = ap[n..an-1]`
    - L165-L172: `am1 = a0 + a1 (mod B^n-1)`（用 `mpn_add + INCR`）
    - L180: 递归 `mpn_sqrmod_bnm1(rp, n, am1, anm, so)` — xm
    - L188-L197: `ap1 = a0 - a1 (mod B^n+1)`（用 `mpn_sub + INCR`）
    - L199-L221: 计算 xp = ap1^2 mod (B^n+1)，三种子路径：FFT(`mpn_mul_fft`)、`mpn_sqr + sub`、`mpn_bc_sqrmod_bnp1`
    - L235-L274: CRT 第一步 `(xm + xp)/2 mod B^n-1`（与 mulmod_bnm1 完全相同的 rsh1add 逻辑）
    - L279-L302: CRT 第二步 unwrap（结构与 mulmod_bnm1 L320-L346 相同）
      - L285-L294: `2*an < rn` 时的 unwrap 路径
      - L298-L301: 标准路径 `cy = xp[n] + mpn_sub_n(rp+n, rp, xp, n); MPN_DECR_U(rp, 2*n, cy);`

#### `mpn_sqrmod_bnm1_next_size` (L310-L328)
- **功能**: 与 `mpn_mulmod_bnm1_next_size` 对称，但用 `SQRMOD_BNM1_THRESHOLD` 和 `SQR_FFT_MODF_THRESHOLD`

## 关键算法步骤详解

### mpn_mulmod_bnm1 的 cyclic reconstruction (L317-L346)

**核心数学公式**:
```
x mod (B^{2n}-1) = -xp * B^n + (B^n + 1) * [ (xp + xm)/2 mod (B^n-1) ]
                 = [mid - xp] * B^n + mid
```
其中 `mid = (xp + xm)/2 mod (B^n-1)` 已在 L265-L315 计算并存入 `rp[0..n-1]`。

**GMP 实现关键（unwrap 的每一步）**:

#### 路径 A: `an+bn ≥ rn`（标准 wrap，L340-L346）
```c
// L342: 计算高 n 位 = mid - xp (mod B^n)
cy = xp[n] + mpn_sub_n(rp + n, rp, xp, n);
// rp[n..2n-1] = rp[0..n-1] - xp[0..n-1] (借位 cy)
// xp[n] 是 xp 的 mod B^n+1 高位标志 (0 or 1)

// L345: 把借位 cy 反向传播到低 n 位（cyclic: B^{2n} ≡ 1 mod B^{2n}-1）
MPN_DECR_U(rp, 2*n, cy);
// 即 rp[0..2n-1] -= cy，等价于高位的借位 1 等于低位的 +1 (mod B^{2n}-1)
```

#### 路径 B: `an+bn < rn`（unwrap 到自然数，L320-L339）
当输入乘积实际不足 `rn` 位时，结果应是自然数（不取模），需要"展开":
```c
// L328: 高位的有效部分只有 an+bn-n 位（而非 n 位）
cy = mpn_sub_n(rp + n, rp, xp, an + bn - n);
// rp[n..an+bn-1] = rp[0..an+bn-n-1] - xp[0..an+bn-n-1]

// L333-L334: 继续减剩余的 rn-(an+bn) 位（仅为提取 carry + sanity check）
cy = xp[n] + mpn_sub_nc(xp + an+bn-n, rp + an+bn-n, xp + an+bn-n,
                         rn - (an+bn), cy);

// L337: 把 borrow 反传到低位
cy = mpn_sub_1(rp, rp, an+bn, cy);
// 此时 rp[0..an+bn-1] 是非负自然数结果，rp[an+bn..rn-1] 为 0

// L338: ASSERT(cy == (xp + an+bn-n)[0]) — 验证 borrow 一致
```

### mpn_mulmid 的应用场景

- **Newton 逆迭代**: `I_new = I + I*(B^k - D*I)/B^k`
  - 其中 `D*I` 是 `2k-1 × k` 的 middle product，结果 `k` 位
  - 用 `mpn_mulmid` 直接给出 `D*I` 的高 `k` 位，避免完整 `2k` 位乘法再截断
- **middle product 边界**: 输出 `an-bn+3` limb = `an-bn+1` 个 middle limb + 2 个 carry limb (lo, hi)
- **何时走 basecase**:
  - `bn < MULMID_TOOM42_THRESHOLD` 且 `an < CHUNK` → 直接 basecase
  - `bn < MULMID_TOOM42_THRESHOLD` 且 `an ≥ CHUNK` → 分块 basecase（沿 an 切）
  - `bn ≥ threshold` 且 `rn < threshold` 且 `bn < CHUNK` → 直接 basecase
  - `bn ≥ threshold` 且 `rn < threshold` 且 `bn ≥ CHUNK` → 分块 basecase（沿 bn 切）
- **何时走 toom42**: `bn ≥ MULMID_TOOM42_THRESHOLD` 且 `rn = an-bn+1 ≥ MULMID_TOOM42_THRESHOLD`
  - `bn > rn`: 沿 bn 方向切 rn 块，全部 toom42
  - `bn ≤ rn`: 沿 an 方向切 bn 块，全部 toom42，最后一块可能递归 mpn_mulmid

### mpn_mulmod_bnm1 的路径选择

- **basecase 路径 (L126-L144)**: `rn` 奇数 或 `rn < MULMOD_BNM1_THRESHOLD`
  - `an+bn ≤ rn`: 纯 `mpn_mul`，无 fold（乘积本身 < B^rn-1）
  - `an+bn > rn`: `mpn_mul` + `mpn_add` (fold 高位)
  - `bn ≥ rn`: `mpn_bc_mulmod_bnm1`（完整 mul_n + add_n fold）
- **递归 CRT 路径 (L145-L353)**: `rn` 偶数 且 `rn ≥ MULMOD_BNM1_THRESHOLD`
  - 子问题 xm 用 `mpn_mulmod_bnm1` 递归（mod B^n-1）
  - 子问题 xp 用 FFT（`mpn_mul_fft`，mod B^n+1）或 `mpn_bc_mulmod_bnp1` 或手动 mul+sub
  - 最后 CRT 重组（rsh1add + unwrap）

### mpn_sqrmod_bnm1 与 mulmod_bnm1 的对称差异

- 入口条件: `rn/4 < an ≤ rn`（vs mulmod 的 `an+bn > rn/2`）
- 阈值: `SQRMOD_BNM1_THRESHOLD` (vs `MULMOD_BNM1_THRESHOLD`)
- 内部用 `mpn_sqr` 替代 `mpn_mul_n`，xp 计算 `ap1*ap1` (自平方)
- FFT 选择: `mpn_fft_best_k(n, 1)`（sqr 模式，第二参数 1）vs mulmod 的 `mpn_fft_best_k(n, 0)`

## 与 moptm_fusion.cpp 的对应关系

- `mpn_mulmod_bnm1` ↔ moptm_fusion.cpp `fftMulModBm1Pre` (L2900-L2980)
- 关键差异:
  - GMP 用 `B = 2^64` limbs (GMP_NUMB_BITS=64)，moptm 用 `B = 10^4` limbs
  - GMP 用 `mpn_mul_n` / `mpn_mul_fft` 作内核，moptm 用 SRFFT
  - GMP 在 `rn` 偶数时递归二分到 `n = rn/2`，moptm 直接用 NTT/SRFFT 一次性计算
  - GMP 用 CRT(B^n-1, B^n+1) 重组（需要 (xp+xm)/2 旋转），moptm 直接做 cyclic wrap
- **bug 修复重点对照点**:
  - **moptm L3318-L3348 的 cyclic 重建逻辑** 对照 **GMP mulmod_bnm1 的 unwrap (L317-L346)**
  - GMP 标准路径 (L342-L345):
    ```c
    cy = xp[n] + mpn_sub_n(rp + n, rp, xp, n);
    MPN_DECR_U(rp, 2*n, cy);
    ```
    - `rp[n..2n-1] = rp[0..n-1] - xp[0..n-1]`（mid - xp 放到高位）
    - `MPN_DECR_U(rp, 2*n, cy)` 把借位反向传播 — 这是 cyclic 的关键：mod B^{2n}-1 下，高位 -1 等于低位 +1
  - moptm 的 `absSub(prod_low_wn, window_high_wn, prod_low_wn)` 步骤对应 GMP **L342** 的 `mpn_sub_n(rp + n, rp, xp, n)`：
    - GMP: 高位 = mid - xp
    - moptm: 应为 low = |high - low|（绝对值减法，因为 B=10^4 下无符号位概念）
  - moptm 若漏掉 `MPN_DECR_U(rp, 2*n, cy)` 这一步（即把借位反传到低位），则会在 `mid < xp` 时出错 — 这是潜在 bug 修复对照点
  - **GMP unwrap 路径 B (L320-L339)** 对应 moptm 当 `an+bn < rn`（乘积短于模长）时的特殊处理；若 moptm 没有这条分支，会错误地把自然数结果当作 wrap-around 值

### mpn_mulmid 与 moptm 的对应

- GMP `mpn_mulmid` (L44-L255) 对应 moptm 中 Newton 逆里的 `D*I` 截断乘法
- GMP 输出 `an-bn+3` limb（含 2 carry），moptm 通常只取 `an-bn+1` limb（丢掉 carry）
- GMP basecase 用 `mpn_mul_1 + mpn_addmul_1` 行累加，moptm 可直接用 SRFFT 的 middle-product 模式
