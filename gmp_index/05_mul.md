# GMP 索引 05: 乘法 (mul + mul_n + mul_fft + sqr)

## 源文件信息
- mul.c: 441 行
- mul_n.c: 96 行
- mul_fft.c: 1105 行 (最大)
- sqr.c: 98 行
- mul_basecase.c: 165 行
- sqr_basecase.c: 361 行

## 函数索引 TREE

### mul.c

#### `mpn_mul` (L113-L441)
- **签名**: `mp_limb_t mpn_mul (mp_ptr prodp, mp_srcptr up, mp_size_t un, mp_srcptr vp, mp_size_t vn)`
- **功能**: 通用 m×n 乘法总入口。要求 `un >= vn >= 1`，输出 `un+vn` limb 到 `prodp`，返回最高 limb（历史接口）。`prodp` 不得与 `up`/`vp` 重叠。
- **宏定义**:
  - `MUL_BASECASE_MAX_UN = 500` (L37-L39): 未定义时的缺省值，控制 basecase 分块上限。
  - `TOOM33_OK(an,bn) = (6 + 2*an < 3*bn)` (L74)
  - `TOOM44_OK(an,bn) = (12 + 3*an < 4*bn)` (L75)
- **调度阈值**: `MUL_TOOM22_THRESHOLD` → `MUL_TOOM33_THRESHOLD` → `MUL_TOOM44_THRESHOLD` → `MUL_TOOM6H_THRESHOLD` → `MUL_TOOM8H_THRESHOLD` → `MUL_FFT_THRESHOLD`；并辅以 `MUL_TOOM32_TO_TOOM43_THRESHOLD` / `MUL_TOOM32_TO_TOOM53_THRESHOLD` / `MUL_TOOM42_TO_TOOM53_THRESHOLD` / `MUL_TOOM42_TO_TOOM63_THRESHOLD` 等精细切换点。

### mul_n.c

#### `mpn_mul_n` (L35-L96)
- **签名**: `void mpn_mul_n (mp_ptr p, mp_srcptr a, mp_srcptr b, mp_size_t n)`
- **功能**: 对称 n×n 乘法专用调度；输出 `2n` limb。`p` 不得与 `a`/`b` 重叠。
- **调度链** (按阈值递增):
  - `< MUL_TOOM22_THRESHOLD` → `mpn_mul_basecase` (L42-L45)
  - `< MUL_TOOM33_THRESHOLD` → `mpn_toom22_mul`，栈上定长 `ws[]` (L46-L53)
  - `< MUL_TOOM44_THRESHOLD` → `mpn_toom33_mul` (L54-L62)
  - `< MUL_TOOM6H_THRESHOLD` → `mpn_toom44_mul` (L63-L71)
  - `< MUL_TOOM8H_THRESHOLD` → `mpn_toom6h_mul` (L72-L80)
  - `< MUL_FFT_THRESHOLD` → `mpn_toom8h_mul` (L81-L89)
  - 否则 → `mpn_fft_mul` (L90-L95)，FFT 自行分配空间。

### mul_fft.c (大文件，详细列出所有函数)

#### `mpn_fft_best_k` (L114-L132) — 第三代 (TUNE_PROGRAM_BUILD 或 MUL_FFT_TABLE3)
- **签名**: `int mpn_fft_best_k (mp_size_t n, int sqr)`
- **功能**: 在 `mpn_fft_table3[sqr][]` 表中扫描，返回对模 `2^(m*GMP_NUMB_BITS)+1` 的 FFT 选取的最佳 `k`。`sqr=1` 表示平方。

#### `mpn_fft_best_k` (L146-L160) — 第一代 (回落版)
- **签名**: 同上。
- **功能**: 在 `mpn_fft_table[sqr][]` 中线性扫描；末尾以 `4*last` 作为额外一项的判定。

#### `mpn_fft_next_size` (L172-L177)
- **签名**: `mp_size_t mpn_fft_next_size (mp_size_t pl, int k)`
- **功能**: 返回 `>= pl` 且为 `2^k` 倍数的最小 limb 数；用于保证 FFT 大小合法。非 static（tuneup 需要）。

#### `mpn_fft_initl` (L181-L197) — static
- **签名**: `static void mpn_fft_initl (int **l, int k)`
- **功能**: 填充位反转表 `l[i][j] = bitrev(j)`，`i=0..k`，`j=0..2^i-1`，迭代式构造。

#### `mpn_fft_mul_2exp_modF` (L204-L299) — static
- **签名**: `static void mpn_fft_mul_2exp_modF (mp_ptr r, mp_srcptr a, mp_bitcnt_t d, mp_size_t n)`
- **功能**: `r <- a * 2^d mod (2^(n*GMP_NUMB_BITS)+1)`，输入半归一化 `a[n] <= 1`。`r` 与 `a` 不重叠，长度均 `n+1`。
- **核心**: 按 `m = d / GMP_NUMB_BITS` 与 `n` 的大小关系分两路：`m >= n` 时取负、`m < n` 时直接移位并取补；处理 `sh = d % GMP_NUMB_BITS` 的非零分支。

#### `mpn_fft_add_sub_modF` (L302-L318) — static inline (HAVE_NATIVE_mpn_add_n_sub_n)
- **签名**: `static inline void mpn_fft_add_sub_modF (mp_ptr A0, mp_ptr Ai, mp_srcptr tp, mp_size_t n)`
- **功能**: 一次 `mpn_add_n_sub_n` 同时算出 `A0 <- A0+tp`、`Ai <- A0-tp` (mod F)，并对 `A[n]` 做归一化修正。

#### `mpn_fft_add_modF` (L325-L351) — static inline (无原生 add_n_sub_n 时)
- **签名**: `static inline void mpn_fft_add_modF (mp_ptr r, mp_srcptr a, mp_srcptr b, mp_size_t n)`
- **功能**: `r <- a+b mod F`，处理进位 `c ∈ [0,3]`，用位运算绕开 GCC 4.1 的 50% 分支预测问题。

#### `mpn_fft_sub_modF` (L356-L382) — static inline (无原生 add_n_sub_n 时)
- **签名**: `static inline void mpn_fft_sub_modF (mp_ptr r, mp_srcptr a, mp_srcptr b, mp_size_t n)`
- **功能**: `r <- a-b mod F`，处理借位 `c ∈ [-2,1]`。

#### `mpn_fft_fft` (L389-L438) — static
- **签名**: `static void mpn_fft_fft (mp_ptr *Ap, mp_size_t K, int **ll, mp_size_t omega, mp_size_t n, mp_size_t inc, mp_ptr tp)`
- **功能**: **正向 FFT 内核**。输入 `A[0..inc*(K-1)]` 为模 `2^N+1` 残差，输出 `A[inc*l[k][i]] <- Σ (2^omega)^(ij) A[inc*j]`。`K=2` 为 butterfly 基情形；`K>2` 时分两半递归（步长 `2*inc`，omega 加倍），再以 `mpn_fft_mul_2exp_modF` + `add_sub_modF` 合并。

#### `mpn_fft_normalize` (L454-L470) — static inline
- **签名**: `static inline void mpn_fft_normalize (mp_ptr ap, mp_size_t n)`
- **功能**: 将半归一化的 `ap[0..n]` 规约到模 `2^(n*GMP_NUMB_BITS)+1`；若 `ap[n] != 0` 则减模，极少数情况下需要清零并置 `ap[n]=1`。

#### `mpn_fft_mul_modF_K` (L473-L615) — static
- **签名**: `static void mpn_fft_mul_modF_K (mp_ptr *ap, mp_ptr *bp, mp_size_t n, mp_size_t K)`
- **功能**: **点值乘法**：`a[i] <- a[i]*b[i] mod F`，`0 <= i < K`。`ap==bp` 时识别为平方。
- **三条路径**:
  - `n >= (sqr?SQR_FFT_MODF_THRESHOLD:MUL_FFT_MODF_THRESHOLD)` (L483-L550): 递归调用 FFT。计算子问题 `K2=2^k`、`nprime2`、`Nprime2`、`M2`、`Mp2`，对每个 i 做 `mpn_mul_fft_decompose` + `mpn_mul_fft_internal`，最终写回 `(*ap)[n] = cy`。
  - `MPN_MULMOD_BKNP1_USABLE` (L551-L577): 调用原生 `mpn_mulmod_bknp1` / `mpn_sqrmod_bknp1`（特殊数论模 `B^k·n+1` 的硬件优化例程）。
  - 否则 (L578-L613): 用普通 `mpn_mul_n`/`mpn_sqr` 计算 `2n` limb 乘积，再折叠回 `n+1` limb 模 F 残差。

#### `mpn_fft_fftinv` (L623-L670) — static
- **签名**: `static void mpn_fft_fftinv (mp_ptr *Ap, mp_size_t K, mp_size_t omega, mp_size_t n, mp_ptr tp)`
- **功能**: **逆向 FFT**（IFFT）。`K=2` 为基情形，`K>2` 时递归两半，再用 `mpn_fft_mul_2exp_modF` 合并；输出含 `K*` 系数，需后续 `mpn_fft_div_2exp_modF` 除以 `K`。

#### `mpn_fft_div_2exp_modF` (L674-L685) — static
- **签名**: `static void mpn_fft_div_2exp_modF (mp_ptr r, mp_srcptr a, mp_bitcnt_t k, mp_size_t n)`
- **功能**: `R <- A/2^k mod F`，通过 `1/2^k = 2^(2nL-k) mod F` 转化为乘法，最后归一化。

#### `mpn_fft_norm_modF` (L692-L720) — static
- **签名**: `static mp_size_t mpn_fft_norm_modF (mp_ptr rp, mp_size_t n, mp_ptr ap, mp_size_t an)`
- **功能**: `{rp,n} <- {ap,an} mod (2^(n*GMP_NUMB_BITS)+1)`，约束 `n <= an <= 3n`；返回 `1` 当且仅当输入为 `-1 mod F`（此时 `rp=0`）。

#### `mpn_mul_fft_decompose` (L728-L813) — static
- **签名**: `static void mpn_mul_fft_decompose (mp_ptr A, mp_ptr *Ap, mp_size_t K, mp_size_t nprime, mp_srcptr n, mp_size_t nl, mp_size_t l, mp_size_t Mp, mp_ptr T)`
- **功能**: **分解**：把 `{n, nl}` 按 `M = l*GMP_NUMB_BITS` 位切成 `K` 段，写入 `A[0..nprime]`、`A[nprime+1..]` …；若 `nl > K*l` 则先归约到模 `2^(Kl*GMP_NUMB_BITS)+1`。每段经 `mpn_fft_mul_2exp_modF` 乘 `2^(i*Mp)` 完成 Schoenhage 的"twiddle"。

#### `mpn_mul_fft_internal` (L821-L901) — static
- **签名**: `static mp_limb_t mpn_mul_fft_internal (mp_ptr op, mp_size_t pl, int k, mp_ptr *Ap, mp_ptr *Bp, mp_ptr unusedA, mp_ptr B, mp_size_t nprime, mp_size_t l, mp_size_t Mp, int **fft_l, mp_ptr T, int sqr)`
- **功能**: **FFT 主流程**：① 正向 FFT (`mpn_fft_fft` 对 A、B)；② 点值乘法 (`mpn_fft_mul_modF_K`)；③ 逆 FFT (`mpn_fft_fftinv`)；④ 逐项除 `2^k` (`mpn_fft_div_2exp_modF`)；⑤ 把 K 段叠加到结果 `p` 并做符号进位修正；⑥ 末尾 `mpn_fft_norm_modF` 给出 `op` 的 `pl` limb。返回最高位。

#### `mpn_mul_fft_lcm` (L904-L915) — static
- **签名**: `static mp_bitcnt_t mpn_mul_fft_lcm (mp_bitcnt_t a, int k)`
- **功能**: 计算 `lcm(a, 2^k)`，用于让 `Nprime` 同时是 `GMP_NUMB_BITS` 和 `2^k` 的倍数（保证递归子 FFT 合法）。

#### `mpn_mul_fft` (L918-L1000) — 主入口
- **签名**: `mp_limb_t mpn_mul_fft (mp_ptr op, mp_size_t pl, mp_srcptr n, mp_size_t nl, mp_srcptr m, mp_size_t ml, int k)`
- **功能**: **公开入口**：`op <- n*m mod 2^(pl*GMP_NUMB_BITS)+1`，返回最高 limb。要求 `pl = mpn_fft_next_size(pl, k)`。识别 `sqr = (n==m && nl==ml)` 走平方路径省一次 decompose。
- **关键步骤**:
  1. 设置 `fft_l[]` 位反转表 (L938-L946, 调 `mpn_fft_initl`)
  2. `K=2^k`，`M=N/k`，`l=1+(M-1)/GMP_NUMB_BITS`，`maxLK=lcm(GMP_NUMB_BITS, 2^k)` (L947-L950)
  3. `Nprime = ceil((2*M+k+3)/maxLK)*maxLK`，`nprime = Nprime/GMP_NUMB_BITS` (L952-L954)
  4. 若 `nprime` 仍 ≥ MODF 阈值，迭代调整使其为下一个 `K2` 的倍数 (L958-L971)
  5. 分配 `A`、`Ap`、`Bp`、`T`、`B` (L974-L995)
  6. `mpn_mul_fft_decompose(A, ...)`，非平方时再 decompose `B` (L984-L995)
  7. 调 `mpn_mul_fft_internal`，`TMP_FREE` 返回 (L996-L999)

#### `mpn_mul_fft_full` (L1004-L1104) — 旧版全乘 (WANT_OLD_FFT_FULL)
- **签名**: `void mpn_mul_fft_full (mp_ptr op, mp_srcptr n, mp_size_t nl, mp_srcptr m, mp_size_t ml)`
- **功能**: 通过双模 FFT 重构完整 `nl+ml` limb 乘积：用 `pl3 = 3*pl2/2` 同时做模 `2^(2N)+1` 与模 `2^(3N)+1` 两次 FFT，再由 `(lambda - mu) / (1 - 2^(l*GMP_NUMB_BITS))` 还原。新代码路径已不使用，仅保留兼容。

### sqr.c

#### `mpn_sqr` (L35-L98)
- **签名**: `void mpn_sqr (mp_ptr p, mp_srcptr a, mp_size_t n)`
- **功能**: 平方总入口，输出 `2n` limb。
- **调度链** (按阈值递增):
  - `< SQR_BASECASE_THRESHOLD` → `mpn_mul_basecase` (小尺寸 mul_basecase 反而比 sqr_basecase 快) (L41-L44)
  - `< SQR_TOOM2_THRESHOLD` → `mpn_sqr_basecase` (L45-L48)
  - `< SQR_TOOM3_THRESHOLD` → `mpn_toom2_sqr`，栈上定长 `ws[]` (L49-L55)
  - `< SQR_TOOM4_THRESHOLD` → `mpn_toom3_sqr` (L56-L64)
  - `< SQR_TOOM6_THRESHOLD` → `mpn_toom4_sqr` (L65-L73)
  - `< SQR_TOOM8_THRESHOLD` → `mpn_toom6_sqr` (L74-L82)
  - `< SQR_FFT_THRESHOLD` → `mpn_toom8_sqr` (L83-L91)
  - 否则 → `mpn_fft_mul(p, a, n, a, n)` (L92-L97)，复用 FFT 乘法（平方仅 decompose 一次）

### mul_basecase.c

#### `mpn_mul_basecase` (L52-L165)
- **签名**: `void mpn_mul_basecase (mp_ptr rp, mp_srcptr up, mp_size_t un, mp_srcptr vp, mp_size_t vn)`
- **功能**: 基础 O(un*vn) 乘法，所有上层算法的基情形。要求 `un >= vn >= 1`，无重叠。注释明示这是"most critical code for multiplication"。
- **首 limb 优化** (L66-L80):
  - 若有原生 `mpn_mul_2` 且 `vn >= 2`：先 `rp[un+1] = mpn_mul_2(rp, up, un, vp)`，再 `rp+=2, vp+=2, vn-=2`
  - 否则：`rp[un] = mpn_mul_1(rp, up, un, vp[0])`，`rp+=1, vp+=1, vn-=1`
- **addmul_N 模板展开** (L85-L156): 按 `HAVE_NATIVE_mpn_addmul_{6,5,4,3,2}` 依次展开循环；每个 `while (vn >= N)` 块调 `mpn_addmul_N`，并通过 `MAX_LEFT` 宏链式提前 return。
- **回落循环** (L158-L164): `while (vn >= 1) mpn_addmul_1(...)`，每次 `rp++, vp++, vn--`。

### sqr_basecase.c

**注**: 本文件用 `READY_WITH_mpn_sqr_basecase` 宏在 4 个互斥版本中选择一个编译，仅一个 `mpn_sqr_basecase` 实际生效。

#### 宏 `MPN_SQR_DIAGONAL` (L41-L56)
- 有原生 `mpn_sqr_diagonal` 时调用之；否则展开为内联循环，每个 `up[i]` 用 `umul_ppmm` 自乘并写入 `rp[2i], rp[2i+1]`。

#### 宏 `MPN_SQR_DIAG_ADDLSH1` (L58-L80)
- 有原生 `mpn_sqr_diag_addlsh1` 时直接调用；有 `mpn_addlsh1_n` 时 `cy = mpn_addlsh1_n(rp+1, rp+1, tp, 2n-2)`；否则 `mpn_lshift(tp, tp, 2n-2, 1)` + `mpn_add_n`。

#### 版本 A: `mpn_sqr_basecase` (L87-L143) — HAVE_NATIVE_mpn_addmul_2s
- 用 `mpn_addmul_2s` 同时算两列交叉积；奇偶 `n` 分别处理；末尾 `MPN_SQR_DIAG_ADDLSH1`。

#### 版本 B: `mpn_sqr_basecase` (L165-L280) — HAVE_NATIVE_mpn_addmul_2
- 用 `mpn_addmul_2` 计算 `u[i]*u[i+1..n-1]`，再以 `MPN_SQR_DIAGONAL` 填对角线并对偶数下标做"加/减修正"（绕开 `addmul_2` 误算 `u[k]*u[k]` 的问题）。

#### 版本 C: `mpn_sqr_basecase` (L289-L317) — HAVE_NATIVE_mpn_sqr_diag_addlsh1
- 无栈分配版：先用 `mpn_mul_1` + `mpn_addmul_1` 累加非对角线项，再调原生 `mpn_sqr_diag_addlsh1` 一次完成对角线 + 左移 1 + 加和。

#### 版本 D (默认): `mpn_sqr_basecase` (L324-L359) — 无任何专用原生函数
- 栈分配 `tarr[2*SQR_TOOM2_THRESHOLD]`；`mpn_mul_1` + `mpn_addmul_1` 累加 `tp`；最后 `MPN_SQR_DIAG_ADDLSH1` 合并。

## 关键算法步骤详解

### mul.c 调度逻辑 (L113-L441)

**主分支树**（自顶向下，第一个匹配即返回）:

1. `BELOW_THRESHOLD(un, MUL_TOOM22_THRESHOLD)` (L123-L130) → 直接 `mpn_mul_basecase`。即使 `un>>vn` 也走此路。
2. `un == vn` (L131-L134) → `mpn_mul_n`（对称乘法专用调度）。
3. `vn < MUL_TOOM22_THRESHOLD` (L135-L205) → 校乘法。
   - 若 `un <= MUL_BASECASE_MAX_UN`（或有 `mpn_mul_2` 且 `vn<=2`）→ 单次 `mpn_mul_basecase`。
   - 否则把 `up` 切成 `MUL_BASECASE_MAX_UN` 大小的块，逐块 `mpn_mul_basecase`，每块后把高位 `vn` limb 暂存到 `tp`，下块乘完用 `mpn_add_n` 加回（注释图见 L156-L172）。
4. `BELOW_THRESHOLD(vn, MUL_TOOM33_THRESHOLD)` (L206-L270) → **ToomX2 族**。
   - `un >= 3*vn`：循环切 `2vn × vn` 走 `mpn_toom42_mul`；末尾 `vn <= un < 3vn` 按 `4*un` vs `5*vn/7*vn` 选 `toom22` / `toom32` / `toom42`。
   - 否则直接按比例选 `toom22` / `toom32` / `toom42`。
5. `BELOW_THRESHOLD((un+vn)>>1, MUL_FFT_THRESHOLD) || BELOW_THRESHOLD(3*vn, MUL_FFT_THRESHOLD)` (L271-L395) → **ToomX3 族或 Toom4/6/8**。
   - 第二个条件让极度不对称的运算避开 FFT。
   - 若 `vn < MUL_TOOM44_THRESHOLD || !TOOM44_OK(un,vn)`：走 ToomX3 (`toom33/32/43/42/53/63`)，循环时按 `MUL_TOOM42_TO_TOOM63_THRESHOLD` 等切换。
   - 否则按阈值 `MUL_TOOM6H_THRESHOLD` / `MUL_TOOM8H_THRESHOLD` 选 `toom44_mul` / `toom6h_mul` / `toom8h_mul`。
6. 否则 (L396-L438) → **FFT**。
   - `un >= 8*vn`：循环切 `3vn × vn` 走 `mpn_fft_mul`，每块加回；末尾 `vn/2 <= un < 3.5vn` 递归 `mpn_mul`。
   - 否则：直接 `mpn_fft_mul(prodp, up, un, vp, vn)`。

### mul_fft.c FFT 算法 (L84-L1000)

- **FFT 类型**: **Schoenhage–Strassen FFT**，模 `2^N+1`（N = `pl * GMP_NUMB_BITS`），单位根 `2^omega` 是模 F 的原根。基 -2（递归二分到 `K=2` butterfly），非 split-radix。
- **模数选择**: `2^(n*GMP_NUMB_BITS)+1`（Fermat-style 模）；通过 `mpn_mul_fft_lcm(GMP_NUMB_BITS, k)` 让子问题 `Nprime` 同时为 `GMP_NUMB_BITS` 与 `2^k` 的倍数，保证可递归。
- **关键步骤**（`mpn_mul_fft_internal`, L821-L901）:
  1. **Split / Decompose** (`mpn_mul_fft_decompose`, L728-L813)：把 `nl` limb 的操作数切成 `K=2^k` 段，每段 `M = N/k` 位；每段 `A_i = a_i * 2^(i*Mp) mod F`。
  2. **正向 FFT** (`mpn_fft_fft`, L389-L438)：递归二分 `K -> K/2`，butterfly 用 `mpn_fft_mul_2exp_modF` 做 `x * 2^(omega*j)`，再用 `mpn_fft_add_sub_modF`（或 `add_modF`/`sub_modF`）做加减。
  3. **点值乘法** (`mpn_fft_mul_modF_K`, L473-L615)：对每个 `i` 做 `A_i * B_i mod F`，可递归到下层 FFT（`nprime2 < n`）或调用 `mpn_mulmod_bknp1` 硬件优化路径。
  4. **逆 FFT** (`mpn_fft_fftinv`, L623-L670)：与正向对称，输出带 `K*` 系数。
  5. **Div 2^k** (`mpn_fft_div_2exp_modF`, L674-L685)：每项除以 `2^k`（即乘 `2^(2nL-k) mod F`）抵消 IFFT 的 `K` 倍。
  6. **Combine / Norm** (`mpn_mul_fft_internal` 末段 L853-L900 + `mpn_fft_norm_modF`, L692-L720)：把 K 段按 `2^(i*l*GMP_NUMB_BITS)` 偏移叠加到 `p`，处理带符号进位 `cc`，最后折叠到 `pl` limb 模 F 输出。
- **辅助**:
  - `mpn_fft_best_k` (L114-L160)：根据 `mpn_fft_table3[sqr][]` 选最佳 `k`。
  - `mpn_fft_next_size` (L172-L177)：上取整到 `2^k` 倍数。
  - `mpn_fft_initl` (L181-L197)：位反转表。

## 与 moptm_fusion.cpp 的对应关系

| GMP (mul_fft.c) | moptm_fusion.cpp | 备注 |
|---|---|---|
| `mpn_mul_fft` (L918-L1000) | `fftMul` (L1958) / `fftMulPre` (L2217) | moptm `fftMulPre` 预计算 `b` 的 DFT 后复用，等价于 GMP 在 `sqr=1` 时省一次 decompose 的思路；GMP 模 `2^N+1`，moptm 模 `B^m-1` (`fftMulModBm1`, L2291) + 线性卷积组合 |
| `mpn_mul_fft_decompose` (L728-L813) | DFT 预处理 + 分块 (`fftMulUnbalanced`, L2117) | GMP 按 `M` 位切段并乘 twiddle `2^(i*Mp)`；moptm 按块切段直接做 DFT |
| `mpn_fft_fft` / `mpn_fft_fftinv` (L389-L438, L623-L670) | radix-4 自排序 DIF/DIT FFT | **关键差异**: GMP 用 radix-2 递归（K→K/2），moptm 用 radix-4 DIF/DIT（见 moptm_fusion.cpp L19 注释） |
| `mpn_fft_mul_modF_K` (L473-L615) | `fftMulPre` 内的点值相乘 | GMP 可递归到 `mpn_mulmod_bknp1`；moptm 直接做点对点 double 复数乘 |
| `mpn_mul_basecase` (mul_basecase.c L52-L165) | moptm 小乘法 fallback (schoolbook) | moptm 在 `n < kFftThreshold` 时回落到 limb-by-limb 校乘法 |
| `mpn_mul` 调度 (mul.c L113-L441) | `mulBig` / `mulDispatch` (moptm 顶部调度) | GMP 6 级阈值（basecase→toom22→toom33→toom44→toom6h→toom8h→fft）；moptm 仅 2 级（schoolbook→FFT） |
| `mpn_sqr` (sqr.c L35-L98) | `fftSqr` (复用 `fftMulPre` 同址) | GMP 平方有独立 `SQR_*_THRESHOLD` 序列，且 `SQR_BASECASE_THRESHOLD` 之下走 `mul_basecase`；moptm 平方与乘法同路径 |

**关键差异**:
- **基数**: GMP 用 `B = 2^64`（`GMP_NUMB_BITS = 64`，limb 为 `mp_limb_t = uint64_t`）；moptm 用 `B = 10^4`（`BASE = qpow(10, BASE_DIGIT)`，limb 为 `uint16_t`，见 moptm_fusion.cpp L1216-L1221）。
- **FFT 模**: GMP 模 `2^N+1`（Fermat 模，模逆为 `2^(2nL-k)`）；moptm 模 `B^m-1`（cyclic convolution，`fftMulModBm1`）+ 线性卷积冗余位。
- **位反转**: GMP 用 `mpn_fft_initl` 预填表 `l[i][j]=bitrev(j)`；moptm 用 radix-4 自排序 DIF/DIT 免显式位反转表。
- **递归**: GMP `mpn_fft_mul_modF_K` 可递归调用 `mpn_mul_fft_internal`（直到 `nprime < MODF_THRESHOLD`）；moptm 仅一层 FFT + schoolbook 基情形。
- **addmul 优化**: GMP `mul_basecase` 在有 `mpn_addmul_{2..6}` 原生实现时按 N 展开循环；moptm 用 AVX2 SIMD（`_mm256_*`，见 moptm_fusion.cpp L1687-L1705）做 `BASE=10000` 的双 limb 加法。
