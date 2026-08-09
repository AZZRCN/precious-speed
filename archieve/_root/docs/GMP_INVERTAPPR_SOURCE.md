# GMP `mpn/generic/invertappr.c` 完整源码与分析

> **任务**: 获取 GMP 的 `mpn/generic/invertappr.c` 完整源码，重点分析 `mpn_ni_invertappr` 函数的实现。
> **来源**: https://raw.githubusercontent.com/alisw/GMP/master/mpn/generic/invertappr.c
> **获取方式**: WebFetch（其他 URL 如 `gmplib.org/repo/gmp/...` 与 `github.com/alisw/GMP/blob/...` 均失败，仅 raw.githubusercontent.com 可用）
> **作者**: Marco Bodrato（贡献给 GNU 项目）
> **算法依据**: Brent-Zimmermann《Modern Computer Arithmetic》算法 3.5 "ApproximateReciprocal"
> **撰写时间**: 2026-07-20

---

## 0. 文件接口约定（来自源码注释 L51-L65）

```c
/* All the three functions mpn{,_bc,_ni}_invertappr (ip, dp, n, scratch), take
   the strictly normalised value {dp,n} (i.e., most significant bit must be set)
   as an input, and compute {ip,n}: the approximate reciprocal of {dp,n}.
   Let e = mpn*_invertappr (ip, dp, n, scratch) be the returned value; the
   following conditions are satisfied by the output:
     0 <= e <= 1;
     {dp,n}*(B^n+{ip,n}) < B^{2n} <= {dp,n}*(B^n+{ip,n}+1+e) .
   I.e. e=0 means that the result {ip,n} equals the one given by mpn_invert.
        e=1 means that the result _may_ be one less than expected.
   The _bc version returns e=1 most of the time.
   The _ni version should return e=0 most of the time; only about 1% of
   possible random input should give e=1.
   When the strict result is needed, i.e., e=0 in the relation above:
     {dp,n}*(B^n+{ip,n}) < B^{2n} <= {dp,n}*(B^n+{ip,n}+1) ;
     the function mpn_invert (ip, dp, n, scratch) should be used instead.  */
```

- 输入 `{dp,n}` 必须严格规范化（最高位为 1）。
- 输出 `{ip,n}` 满足 `{dp,n}*(B^n+{ip,n}) < B^{2n} <= {dp,n}*(B^n+{ip,n}+1+e)`，`e ∈ {0,1}`。
- `_bc`（base case）大多数情况下返回 `e=1`；`_ni`（Newton iteration）约 99% 返回 `e=0`。
- 若需严格 `e=0`，应使用 `mpn_invert` 而非 `mpn_invertappr`。

---

## 1. 完整源码（带行号）

下面是 `invertappr.c` 的完整内容（共 256 行），行号与原始文件一致：

```c
     1  /* mpn_invertappr and helper functions.  Compute I such that
     2     floor((B^{2n}-1)/U - 1 <= I + B^n <= floor((B^{2n}-1)/U.
     3     Contributed to the GNU project by Marco Bodrato.
     4     The algorithm used here was inspired by ApproximateReciprocal from "Modern
     5     Computer Arithmetic", by Richard P. Brent and Paul Zimmermann.  Special
     6     thanks to Paul Zimmermann for his very valuable suggestions on all the
     7     theoretical aspects during the work on this code.
     8     THE FUNCTIONS IN THIS FILE ARE INTERNAL WITH MUTABLE INTERFACES.  IT IS ONLY
     9     SAFE TO REACH THEM THROUGH DOCUMENTED INTERFACES.  IN FACT, IT IS ALMOST
    10     GUARANTEED THAT THEY WILL CHANGE OR DISAPPEAR IN A FUTURE GMP RELEASE.
    11  Copyright (C) 2007, 2009, 2010, 2012, 2015, 2016 Free Software
    12  Foundation, Inc.
    13  This file is part of the GNU MP Library.
    14  The GNU MP Library is free software; you can redistribute it and/or modify
    15  it under the terms of either:
    16    * the GNU Lesser General Public License as published by the Free
    17      Software Foundation; either version 3 of the License, or (at your
    18      option) any later version.
    19  or
    20    * the GNU General Public License as published by the Free Software
    21      Foundation; either version 2 of the License, or (at your option) any
    22      later version.
    23  or both in parallel, as here.
    24  The GNU MP Library is distributed in the hope that it will be useful, but
    25  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
    26  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    27  for more details.
    28  You should have received copies of the GNU General Public License and the
    29  GNU Lesser General Public License along with the GNU MP Library.  If not,
    30  see https://www.gnu.org/licenses/.  */
    31  #include "gmp-impl.h"
    32  #include "longlong.h"
    33  /* FIXME: The iterative version splits the operand in two slightly unbalanced
    34     parts, the use of log_2 (or counting the bits) underestimate the maximum
    35     number of iterations.  */
    36  #if TUNE_PROGRAM_BUILD
    37  #define NPOWS \
    38   ((sizeof(mp_size_t) > 6 ? 48 : 8*sizeof(mp_size_t)))
    39  #define MAYBE_dcpi1_divappr   1
    40  #else
    41  #define NPOWS \
    42   ((sizeof(mp_size_t) > 6 ? 48 : 8*sizeof(mp_size_t)) - LOG2C (INV_NEWTON_THRESHOLD))
    43  #define MAYBE_dcpi1_divappr \
    44    (INV_NEWTON_THRESHOLD < DC_DIVAPPR_Q_THRESHOLD)
    45  #if (INV_NEWTON_THRESHOLD > INV_MULMOD_BNM1_THRESHOLD) && \
    46      (INV_APPR_THRESHOLD > INV_MULMOD_BNM1_THRESHOLD)
    47  #undef  INV_MULMOD_BNM1_THRESHOLD
    48  #define INV_MULMOD_BNM1_THRESHOLD 0 /* always when Newton */
    49  #endif
    50  #endif
    51  /* All the three functions mpn{,_bc,_ni}_invertappr (ip, dp, n, scratch), take
    52     the strictly normalised value {dp,n} (i.e., most significant bit must be set)
    53     as an input, and compute {ip,n}: the approximate reciprocal of {dp,n}.
    54     Let e = mpn*_invertappr (ip, dp, n, scratch) be the returned value; the
    55     following conditions are satisfied by the output:
    56       0 <= e <= 1;
    57       {dp,n}*(B^n+{ip,n}) < B^{2n} <= {dp,n}*(B^n+{ip,n}+1+e) .
    58     I.e. e=0 means that the result {ip,n} equals the one given by mpn_invert.
    59          e=1 means that the result _may_ be one less than expected.
    60     The _bc version returns e=1 most of the time.
    61     The _ni version should return e=0 most of the time; only about 1% of
    62     possible random input should give e=1.
    63     When the strict result is needed, i.e., e=0 in the relation above:
    64       {dp,n}*(B^n+{ip,n}) < B^{2n} <= {dp,n}*(B^n+{ip,n}+1) ;
    65       the function mpn_invert (ip, dp, n, scratch) should be used instead.  */
    66  /* Maximum scratch needed by this branch (at xp): 2*n */
    67  static mp_limb_t
    68  mpn_bc_invertappr (mp_ptr ip, mp_srcptr dp, mp_size_t n, mp_ptr xp)
    69  {
    70    ASSERT (n > 0);
    71    ASSERT (dp[n-1] & GMP_NUMB_HIGHBIT);
    72    ASSERT (! MPN_OVERLAP_P (ip, n, dp, n));
    73    ASSERT (! MPN_OVERLAP_P (ip, n, xp, mpn_invertappr_itch(n)));
    74    ASSERT (! MPN_OVERLAP_P (dp, n, xp, mpn_invertappr_itch(n)));
    75    /* Compute a base value of r limbs. */
    76    if (n == 1)
    77      invert_limb (*ip, *dp);
    78    else {
    79      /* n > 1 here */
    80      MPN_FILL (xp, n, GMP_NUMB_MAX);
    81      mpn_com (xp + n, dp, n);
    82      /* Now xp contains B^2n - {dp,n}*B^n - 1 */
    83      /* FIXME: if mpn_*pi1_divappr_q handles n==2, use it! */
    84      if (n == 2) {
    85        mpn_divrem_2 (ip, 0, xp, 4, dp);
    86      } else {
    87        gmp_pi1_t inv;
    88        invert_pi1 (inv, dp[n-1], dp[n-2]);
    89        if (! MAYBE_dcpi1_divappr
    90            || BELOW_THRESHOLD (n, DC_DIVAPPR_Q_THRESHOLD))
    91          mpn_sbpi1_divappr_q (ip, xp, 2 * n, dp, n, inv.inv32);
    92        else
    93          mpn_dcpi1_divappr_q (ip, xp, 2 * n, dp, n, &inv);
    94        MPN_DECR_U(ip, n, CNST_LIMB (1));
    95        return 1;
    96      }
    97    }
    98    return 0;
    99  }
   100  /* mpn_ni_invertappr: computes the approximate reciprocal using Newton's
   101     iterations (at least one).
   102     Inspired by Algorithm "ApproximateReciprocal", published in "Modern Computer
   103     Arithmetic" by Richard P. Brent and Paul Zimmermann, algorithm 3.5, page 121
   104     in version 0.4 of the book.
   105     Some adaptations were introduced, to allow product mod B^m-1 and return the
   106     value e.
   107     We introduced a correction in such a way that "the value of
   108     B^{n+h}-T computed at step 8 cannot exceed B^n-1" (the book reads
   109     "2B^n-1").
   110     Maximum scratch needed by this branch <= 2*n, but have to fit 3*rn
   111     in the scratch, i.e. 3*rn <= 2*n: we require n>4.
   112     We use a wrapped product modulo B^m-1.  NOTE: is there any normalisation
   113     problem for the [0] class?  It shouldn't: we compute 2*|A*X_h - B^{n+h}| <
   114     B^m-1.  We may get [0] if and only if we get AX_h = B^{n+h}.  This can
   115     happen only if A=B^{n}/2, but this implies X_h = B^{h}*2-1 i.e., AX_h =
   116     B^{n+h} - A, then we get into the "negative" branch, where X_h is not
   117     incremented (because A < B^n).
   118     FIXME: the scratch for mulmod_bnm1 does not currently fit in the scratch, it
   119     is allocated apart.
   120   */
   121  mp_limb_t
   122  mpn_ni_invertappr (mp_ptr ip, mp_srcptr dp, mp_size_t n, mp_ptr scratch)
   123  {
   124    mp_limb_t cy;
   125    mp_size_t rn, mn;
   126    mp_size_t sizes[NPOWS], *sizp;
   127    mp_ptr tp;
   128    TMP_DECL;
   129  #define xp scratch
   130    ASSERT (n > 4);
   131    ASSERT (dp[n-1] & GMP_NUMB_HIGHBIT);
   132    ASSERT (! MPN_OVERLAP_P (ip, n, dp, n));
   133    ASSERT (! MPN_OVERLAP_P (ip, n, scratch, mpn_invertappr_itch(n)));
   134    ASSERT (! MPN_OVERLAP_P (dp, n, scratch, mpn_invertappr_itch(n)));
   135    /* Compute the computation precisions from highest to lowest, leaving the
   136       base case size in 'rn'.  */
   137    sizp = sizes;
   138    rn = n;
   139    do {
   140      *sizp = rn;
   141      rn = (rn >> 1) + 1;
   142      ++sizp;
   143    } while (ABOVE_THRESHOLD (rn, INV_NEWTON_THRESHOLD));
   144    /* We search the inverse of 0.{dp,n}, we compute it as 1.{ip,n} */
   145    dp += n;
   146    ip += n;
   147    /* Compute a base value of rn limbs. */
   148    mpn_bc_invertappr (ip - rn, dp - rn, rn, scratch);
   149    TMP_MARK;
   150    if (ABOVE_THRESHOLD (n, INV_MULMOD_BNM1_THRESHOLD))
   151      {
   152        mn = mpn_mulmod_bnm1_next_size (n + 1);
   153        tp = TMP_ALLOC_LIMBS (mpn_mulmod_bnm1_itch (mn, n, (n >> 1) + 1));
   154      }
   155    /* Use Newton's iterations to get the desired precision.*/
   156    while (1) {
   157      n = *--sizp;
   158      /*
   159        v    n  v
   160        +----+--+
   161        ^ rn ^
   162      */
   163      /* Compute i_jd . */
   164      if (BELOW_THRESHOLD (n, INV_MULMOD_BNM1_THRESHOLD)
   165          || ((mn = mpn_mulmod_bnm1_next_size (n + 1)) > (n + rn))) {
   166        /* FIXME: We do only need {xp,n+1}*/
   167        mpn_mul (xp, dp - n, n, ip - rn, rn);
   168        mpn_add_n (xp + rn, xp + rn, dp - n, n - rn + 1);
   169        cy = CNST_LIMB(1); /* Remember we truncated, Mod B^(n+1) */
   170        /* We computed (truncated) {xp,n+1} <- 1.{ip,rn} * 0.{dp,n} */
   171      } else { /* Use B^mn-1 wraparound */
   172        mpn_mulmod_bnm1 (xp, mn, dp - n, n, ip - rn, rn, tp);
   173        /* We computed {xp,mn} <- {ip,rn} * {dp,n} mod (B^mn-1) */
   174        /* We know that 2*|ip*dp + dp*B^rn - B^{rn+n}| < B^mn-1 */
   175        /* Add dp*B^rn mod (B^mn-1) */
   176        ASSERT (n >= mn - rn);
   177        cy = mpn_add_n (xp + rn, xp + rn, dp - n, mn - rn);
   178        cy = mpn_add_nc (xp, xp, dp - (n - (mn - rn)), n - (mn - rn), cy);
   179        /* Subtract B^{rn+n}, maybe only compensate the carry*/
   180        xp[mn] = CNST_LIMB (1); /* set a limit for DECR_U */
   181        MPN_DECR_U (xp + rn + n - mn, 2 * mn + 1 - rn - n, CNST_LIMB (1) - cy);
   182        MPN_DECR_U (xp, mn, CNST_LIMB (1) - xp[mn]); /* if DECR_U eroded xp[mn] */
   183        cy = CNST_LIMB(0); /* Remember we are working Mod B^mn-1 */
   184      }
   185      if (xp[n] < CNST_LIMB (2)) { /* "positive" residue class */
   186        cy = xp[n]; /* 0 <= cy <= 1 here. */
   187  #if HAVE_NATIVE_mpn_sublsh1_n
   188        if (cy++) {
   189          if (mpn_cmp (xp, dp - n, n) > 0) {
   190            mp_limb_t chk;
   191            chk = mpn_sublsh1_n (xp, xp, dp - n, n);
   192            ASSERT (chk == xp[n]);
   193            ++ cy;
   194          } else
   195            ASSERT_CARRY (mpn_sub_n (xp, xp, dp - n, n));
   196        }
   197  #else /* no mpn_sublsh1_n*/
   198        if (cy++ && !mpn_sub_n (xp, xp, dp - n, n)) {
   199          ASSERT_CARRY (mpn_sub_n (xp, xp, dp - n, n));
   200          ++cy;
   201        }
   202  #endif
   203        /* 1 <= cy <= 3 here. */
   204  #if HAVE_NATIVE_mpn_rsblsh1_n
   205        if (mpn_cmp (xp, dp - n, n) > 0) {
   206          ASSERT_NOCARRY (mpn_rsblsh1_n (xp + n, xp, dp - n, n));
   207          ++cy;
   208        } else
   209          ASSERT_NOCARRY (mpn_sub_nc (xp + 2 * n - rn, dp - rn, xp + n - rn, rn, mpn_cmp (xp, dp - n, n - rn) > 0));
   210  #else /* no mpn_rsblsh1_n*/
   211        if (mpn_cmp (xp, dp - n, n) > 0) {
   212          ASSERT_NOCARRY (mpn_sub_n (xp, xp, dp - n, n));
   213          ++cy;
   214        }
   215        ASSERT_NOCARRY (mpn_sub_nc (xp + 2 * n - rn, dp - rn, xp + n - rn, rn, mpn_cmp (xp, dp - n, n - rn) > 0));
   216  #endif
   217        MPN_DECR_U(ip - rn, rn, cy); /* 1 <= cy <= 4 here. */
   218      } else { /* "negative" residue class */
   219        ASSERT (xp[n] >= GMP_NUMB_MAX - CNST_LIMB(1));
   220        MPN_DECR_U(xp, n + 1, cy);
   221        if (xp[n] != GMP_NUMB_MAX) {
   222          MPN_INCR_U(ip - rn, rn, CNST_LIMB (1));
   223          ASSERT_CARRY (mpn_add_n (xp, xp, dp - n, n));
   224        }
   225        mpn_com (xp + 2 * n - rn, xp + n - rn, rn);
   226      }
   227      /* Compute x_ju_j. FIXME:We need {xp+rn,rn}, mulhi? */
   228      mpn_mul_n (xp, xp + 2 * n - rn, ip - rn, rn);
   229      cy = mpn_add_n (xp + rn, xp + rn, xp + 2 * n - rn, 2 * rn - n);
   230      cy = mpn_add_nc (ip - n, xp + 3 * rn - n, xp + n + rn, n - rn, cy);
   231      MPN_INCR_U (ip - rn, rn, cy);
   232      if (sizp == sizes) { /* Get out of the cycle */
   233        /* Check for possible carry propagation from below. */
   234        cy = xp[3 * rn - n - 1] > GMP_NUMB_MAX - CNST_LIMB (7); /* Be conservative. */
   235        /*    cy = mpn_add_1 (xp + rn, xp + rn, 2*rn - n, 4); */
   236        break;
   237      }
   238      rn = n;
   239    }
   240    TMP_FREE;
   241    return cy;
   242  #undef xp
   243  }
   244  mp_limb_t
   245  mpn_invertappr (mp_ptr ip, mp_srcptr dp, mp_size_t n, mp_ptr scratch)
   246  {
   247    ASSERT (n > 0);
   248    ASSERT (dp[n-1] & GMP_NUMB_HIGHBIT);
   249    ASSERT (! MPN_OVERLAP_P (ip, n, dp, n));
   250    ASSERT (! MPN_OVERLAP_P (ip, n, scratch, mpn_invertappr_itch(n)));
   251    ASSERT (! MPN_OVERLAP_P (dp, n, scratch, mpn_invertappr_itch(n)));
   252    if (BELOW_THRESHOLD (n, INV_NEWTON_THRESHOLD))
   253      return mpn_bc_invertappr (ip, dp, n, scratch);
   254    else
   255      return mpn_ni_invertappr (ip, dp, n, scratch);
   256  }
```

---

## 2. `mpn_ni_invertappr` 函数逐段详解

### 2.1 函数签名与局部变量（L121-L129）

```c
mp_limb_t
mpn_ni_invertappr (mp_ptr ip, mp_srcptr dp, mp_size_t n, mp_ptr scratch)
{
  mp_limb_t cy;
  mp_size_t rn, mn;
  mp_size_t sizes[NPOWS], *sizp;
  mp_ptr tp;
  TMP_DECL;
#define xp scratch
```

- `ip`：输出倒数，长度 `n`。
- `dp`：输入规范化的 divisor，长度 `n`（最高位为 1）。
- `n`：当前层精度（limb 数），要求 `n > 4`。
- `scratch`：scratch 内存，最大需 `2*n` limb；`xp` 是 `scratch` 的别名。
- `cy`：进位/借位暂存。
- `rn`：上一层（低精度）的精度，约 `n/2`。
- `mn`：cyclic convolution 的模长（`B^mn - 1`）。
- `sizes[]`：从高到低记录每层精度，`sizp` 是遍历指针。
- `tp`：`mpn_mulmod_bnm1` 的独立 scratch（注释 L118-L119 说明它当前不占 `scratch`，单独分配）。

### 2.2 ASSERT 与阈值检查（L130-L134）

```c
ASSERT (n > 4);
ASSERT (dp[n-1] & GMP_NUMB_HIGHBIT);
ASSERT (! MPN_OVERLAP_P (ip, n, dp, n));
ASSERT (! MPN_OVERLAP_P (ip, n, scratch, mpn_invertappr_itch(n)));
ASSERT (! MPN_OVERLAP_P (dp, n, scratch, mpn_invertappr_itch(n)));
```

- `n > 4`：因为 scratch 至多 `2*n`，但要装下 `3*rn`，即 `3*rn <= 2*n`，所以 `n` 不能太小（见注释 L110-L111）。
- 规范化保证 `dp[n-1]` 最高位为 1。
- 三段内存 `ip`/`dp`/`scratch` 不得两两重叠。

### 2.3 计算各层精度（L135-L143）

```c
/* Compute the computation precisions from highest to lowest, leaving the
   base case size in 'rn'.  */
sizp = sizes;
rn = n;
do {
  *sizp = rn;
  rn = (rn >> 1) + 1;
  ++sizp;
} while (ABOVE_THRESHOLD (rn, INV_NEWTON_THRESHOLD));
```

- 从最高精度 `n` 开始，每次折半加 1（`rn = (rn>>1) + 1`），把每一层的精度写入 `sizes[]`，直到 `rn` 跌破 `INV_NEWTON_THRESHOLD`。
- 循环结束后 `rn` 是 base case 的精度。
- 例：`n=100` → `sizes=[100, 51, 27, 15, ...]`，直到 `rn` 小于阈值。
- `sizes[]` 是**从高到低**写入，`sizp` 指向尾后；后续迭代时 `--sizp` 反向遍历（从低到高）。

### 2.4 指针前移 + base case（L144-L148）

```c
/* We search the inverse of 0.{dp,n}, we compute it as 1.{ip,n} */
dp += n;
ip += n;
/* Compute a base value of rn limbs. */
mpn_bc_invertappr (ip - rn, dp - rn, rn, scratch);
```

**指针运算展开**：
- `dp += n` 后，`dp` 指向原 `{dp,n}` 的尾后；`dp - k` 表示原 `{dp,n}` 的**最高 k 位**。
- `ip += n` 同理，`ip - k` 表示输出区 `{ip,n}` 的**最高 k 位**。
- 注释 "inverse of `0.{dp,n}`" 即把 `{dp,n}` 视为小数 `0.d1 d2 ... dn`（< 1），其倒数约为 `1.i1 i2 ... in`（介于 1 和 2 之间）。
- 调用 `mpn_bc_invertappr` 在低精度 `rn` 上算出 base case 倒数 `{ip-rn, rn}`。

### 2.5 为 cyclic 分配独立 scratch（L149-L154）

```c
TMP_MARK;
if (ABOVE_THRESHOLD (n, INV_MULMOD_BNM1_THRESHOLD))
  {
    mn = mpn_mulmod_bnm1_next_size (n + 1);
    tp = TMP_ALLOC_LIMBS (mpn_mulmod_bnm1_itch (mn, n, (n >> 1) + 1));
  }
```

- 仅当顶层 `n` 超过 `INV_MULMOD_BNM1_THRESHOLD` 时才预先分配 `tp`。
- `mn = mpn_mulmod_bnm1_next_size(n+1)`：取 `>= n+1` 且适合 cyclic 的尺寸（GMP 内部策略，通常支持非 2 幂）。
- `mpn_mulmod_bnm1_itch` 给出该函数需要的 scratch 大小。

### 2.6 Newton 迭代主循环（L155-L239）

```c
/* Use Newton's iterations to get the desired precision.*/
while (1) {
  n = *--sizp;
```

- 无限循环，每次 `--sizp` 从 `sizes[]` 反向取下一层精度 `n`。
- 第一次迭代时 `n` 是 base case 上一层的精度（约 `2*rn - 2`）。

#### 2.6.1 第一步：计算 `xp = (ip * dp + dp * B^rn - B^{rn+n})` 的两种实现

##### A) 常规截断乘法分支（L164-L170）

```c
if (BELOW_THRESHOLD (n, INV_MULMOD_BNM1_THRESHOLD)
    || ((mn = mpn_mulmod_bnm1_next_size (n + 1)) > (n + rn))) {
  /* FIXME: We do only need {xp,n+1}*/
  mpn_mul (xp, dp - n, n, ip - rn, rn);
  mpn_add_n (xp + rn, xp + rn, dp - n, n - rn + 1);
  cy = CNST_LIMB(1); /* Remember we truncated, Mod B^(n+1) */
  /* We computed (truncated) {xp,n+1} <- 1.{ip,rn} * 0.{dp,n} */
```

- 触发条件：`n` 低于 cyclic 阈值，**或** `mn > n + rn`（即 cyclic 模长比需要的还长，不值得用）。
- `mpn_mul(xp, dp-n, n, ip-rn, rn)`：算 `{dp,n} * {ip,rn}`，结果 `n+rn` limb 写入 `xp`。
- `mpn_add_n(xp+rn, xp+rn, dp-n, n-rn+1)`：把 `dp` 的高 `n-rn+1` 位加到 `xp` 中 `rn` 偏移处，模拟 `+ dp * B^rn`（截断到 `n+1` 位）。
- `cy = 1`：标记做了截断（模 `B^(n+1)`），后续减 `B^{rn+n}` 隐含在截断里。

##### B) Cyclic convolution 分支（L171-L184）—— 重点

```c
} else { /* Use B^mn-1 wraparound */
  mpn_mulmod_bnm1 (xp, mn, dp - n, n, ip - rn, rn, tp);
  /* We computed {xp,mn} <- {ip,rn} * {dp,n} mod (B^mn-1) */
  /* We know that 2*|ip*dp + dp*B^rn - B^{rn+n}| < B^mn-1 */
  /* Add dp*B^rn mod (B^mn-1) */
  ASSERT (n >= mn - rn);
  cy = mpn_add_n (xp + rn, xp + rn, dp - n, mn - rn);
  cy = mpn_add_nc (xp, xp, dp - (n - (mn - rn)), n - (mn - rn), cy);
  /* Subtract B^{rn+n}, maybe only compensate the carry*/
  xp[mn] = CNST_LIMB (1); /* set a limit for DECR_U */
  MPN_DECR_U (xp + rn + n - mn, 2 * mn + 1 - rn - n, CNST_LIMB (1) - cy);
  MPN_DECR_U (xp, mn, CNST_LIMB (1) - xp[mn]); /* if DECR_U eroded xp[mn] */
  cy = CNST_LIMB(0); /* Remember we are working Mod B^mn-1 */
}
```

**逐行解析**：

1. **L172 `mpn_mulmod_bnm1(xp, mn, dp-n, n, ip-rn, rn, tp)`**：
   - 计算 `{ip,rn} * {dp,n} mod (B^mn - 1)`，结果 `mn` limb 写入 `xp[0..mn-1]`。
   - `tp` 是独立 scratch。
   - cyclic convolution 比 full mul 短一半（`mn` 比 `n+rn` 小）。

2. **关键不变量（L174 注释）**：`2*|ip*dp + dp*B^rn - B^{rn+n}| < B^mn-1`
   - 即真实值 `X = ip*dp + dp*B^rn - B^{rn+n}` 满足 `|2X| < B^mn-1`。
   - 这意味着 `X mod (B^mn-1)` 能无歧义区分正负（不会绕一圈）。

3. **L176-L178 加 `dp * B^rn mod (B^mn-1)`**：
   - `ASSERT(n >= mn - rn)`：保证 `dp` 的长度 `n` 足够覆盖 cyclic wrap 部分 `mn - rn`。
   - **L177** `cy = mpn_add_n(xp+rn, xp+rn, dp-n, mn-rn)`：
     - 把 `dp` 的高 `mn-rn` 位（即 `dp[n - (mn-rn) .. n-1]`，指针 `dp-n` 起的 `mn-rn` limb）加到 `xp[rn .. mn-1]`。
     - 这对应 `dp_high * B^rn` 这一段。
   - **L178** `cy = mpn_add_nc(xp, xp, dp - (n - (mn-rn)), n - (mn-rn), cy)`：
     - 把 `dp` 的低 `n - (mn-rn)` 位（即 `dp[0 .. n-(mn-rn)-1]`）加到 `xp[0 .. n-(mn-rn)-1]`，带进位 `cy`。
     - 这对应 cyclic wrap 部分：`dp_low * B^rn` 超过 `B^mn` 后绕回到低位。
   - 加完后 `cy` 是最高进位。

4. **L179-L182 减 `B^{rn+n}`**：
   - **L180** `xp[mn] = 1`：在 `xp[mn]` 设一个"哨兵" limb = 1，作为后续 `MPN_DECR_U` 的借位终点（防止下溢）。
   - **L181** `MPN_DECR_U(xp + rn + n - mn, 2*mn + 1 - rn - n, 1 - cy)`：
     - 从 `xp[rn+n-mn]` 开始，长度 `2*mn+1-rn-n`，减去 `1 - cy`。
     - 这是在 `xp` 中 `B^{rn+n-mn}` 位置减 `1`，等价于整体减 `B^{rn+n}`（因为偏移 `rn+n-mn` + 模长 `mn` 的 wrap）。
     - 如果 `cy=1`（前面加法产生了进位，相当于已经多加了 1 个 `B^mn = B^{rn+n}` 的 wrap），则 `1-cy=0`，无需再减。
   - **L182** `MPN_DECR_U(xp, mn, 1 - xp[mn])`：
     - 如果上一步 `MPN_DECR_U` 把哨兵 `xp[mn]` 从 1 减到 0（即发生了跨 `mn` 边界的借位），则 `1 - xp[mn] = 1`，需要再从 `xp[0..mn-1]` 减 1 来补偿。
     - 如果哨兵仍为 1（未跨边界），则 `1 - xp[mn] = 0`，无操作。
   - **L183** `cy = 0`：标记当前是模 `B^mn-1` 运算，没有"截断进位"。

#### 2.6.2 正/负剩余类判定（L185-L226）

```c
if (xp[n] < CNST_LIMB (2)) { /* "positive" residue class */
  ...
} else { /* "negative" residue class */
  ...
}
```

**判定依据**：
- `xp[n]` 是 `xp` 数组第 `n` 位（超出 cyclic 模长 `mn` 的位置，因为 `mn <= n+rn < 2n`）。
- 由于 `2*|X| < B^mn-1`，`X mod (B^mn-1)` 的"符号"由 `xp[n]` 反映：
  - **正剩余类**：`xp[n] ∈ {0, 1}`（`X > 0`，结果直接是 `X`）。
  - **负剩余类**：`xp[n] ∈ {B-2, B-1}`（`X < 0`，结果是 `B^{mn+...} - |X|`，高位为 `B-1`）。

##### A) 正剩余类修正（L185-L217）

```c
if (xp[n] < CNST_LIMB (2)) { /* "positive" residue class */
  cy = xp[n]; /* 0 <= cy <= 1 here. */
#if HAVE_NATIVE_mpn_sublsh1_n
  if (cy++) {
    if (mpn_cmp (xp, dp - n, n) > 0) {
      mp_limb_t chk;
      chk = mpn_sublsh1_n (xp, xp, dp - n, n);
      ASSERT (chk == xp[n]);
      ++ cy;
    } else
      ASSERT_CARRY (mpn_sub_n (xp, xp, dp - n, n));
  }
#else /* no mpn_sublsh1_n*/
  if (cy++ && !mpn_sub_n (xp, xp, dp - n, n)) {
    ASSERT_CARRY (mpn_sub_n (xp, xp, dp - n, n));
    ++cy;
  }
#endif
  /* 1 <= cy <= 3 here. */
#if HAVE_NATIVE_mpn_rsblsh1_n
  if (mpn_cmp (xp, dp - n, n) > 0) {
    ASSERT_NOCARRY (mpn_rsblsh1_n (xp + n, xp, dp - n, n));
    ++cy;
  } else
    ASSERT_NOCARRY (mpn_sub_nc (xp + 2 * n - rn, dp - rn, xp + n - rn, rn, mpn_cmp (xp, dp - n, n - rn) > 0));
#else /* no mpn_rsblsh1_n*/
  if (mpn_cmp (xp, dp - n, n) > 0) {
    ASSERT_NOCARRY (mpn_sub_n (xp, xp, dp - n, n));
    ++cy;
  }
  ASSERT_NOCARRY (mpn_sub_nc (xp + 2 * n - rn, dp - rn, xp + n - rn, rn, mpn_cmp (xp, dp - n, n - rn) > 0));
#endif
  MPN_DECR_U(ip - rn, rn, cy); /* 1 <= cy <= 4 here. */
}
```

**解析**：

- **L186** `cy = xp[n]`：保存正剩余类的值（0 或 1）。
- **第一段减法（L187-L202）**：处理 `cy=1` 的情形（即 `xp[n]=1`，需要再减去一个 `dp` 来归一化）。
  - `cy++` 先判断再加 1（注意：`cy++` 表达式返回原值，所以 `if (cy++)` 等价于 `if (cy) { cy++; ... }`）。
  - 若有原生 `mpn_sublsh1_n`（计算 `a - (b<<1)`）：
    - 如果 `xp >= dp`（高 n 位比较），用 `sublsh1_n` 一次减两倍 `dp`，`cy` 再加 1（共减 2 个 `dp`）。
    - 否则只减一个 `dp`（`sub_n`），`cy` 不变。
  - 否则用两次 `sub_n` 模拟：先减一次，若有借位（说明不够减）则再减一次（这时 ASSERT_CARRY 表示一定有借位，即第二次减会下溢），`cy++`。
- **第二段减法（L204-L216）**：进一步归一化到 `xp[0..n-1] < dp`。
  - 若有原生 `mpn_rsblsh1_n`（计算 `(b<<1) - a`）：
    - 若 `xp >= dp`：`rsblsh1_n(xp+n, xp, dp-n, n)` 即 `(dp<<1) - xp`，写入 `xp+n`，`cy++`。
    - 否则：`sub_nc(xp+2n-rn, dp-rn, xp+n-rn, rn, borrow)` 计算高位部分的减法。
  - 否则用 `sub_n` + `sub_nc` 等价实现。
- **L217** `MPN_DECR_U(ip-rn, rn, cy)`：把累计的 `cy`（1~4）从 `{ip-rn, rn}` 减去。这是正剩余类对 `ip` 的最终修正。

##### B) 负剩余类修正（L218-L226）

```c
} else { /* "negative" residue class */
  ASSERT (xp[n] >= GMP_NUMB_MAX - CNST_LIMB(1));
  MPN_DECR_U(xp, n + 1, cy);
  if (xp[n] != GMP_NUMB_MAX) {
    MPN_INCR_U(ip - rn, rn, CNST_LIMB (1));
    ASSERT_CARRY (mpn_add_n (xp, xp, dp - n, n));
  }
  mpn_com (xp + 2 * n - rn, xp + n - rn, rn);
}
```

**解析**：

- **L219** `ASSERT(xp[n] >= GMP_NUMB_MAX - 1)`：负剩余类时 `xp[n]` 必须是 `B-1` 或 `B-2`（`GMP_NUMB_MAX = B-1`）。
- **L220** `MPN_DECR_U(xp, n+1, cy)`：从 `xp` 减去之前保存的 `cy`（来自第一步的截断标记，cyclic 分支为 0，常规分支为 1）。
- **L221-L224** 若 `xp[n] != GMP_NUMB_MAX`（即 `xp[n] = B-2`，不是纯负）：
  - **L222** `MPN_INCR_U(ip-rn, rn, 1)`：给 `ip` 加 1（负剩余类下 `ip` 偏小，需补偿）。
  - **L223** `ASSERT_CARRY(mpn_add_n(xp, xp, dp-n, n))`：把 `dp` 加到 `xp` 上，一定产生进位（用 ASSERT_CARRY 断言）。
- **L225** `mpn_com(xp+2n-rn, xp+n-rn, rn)`：对 `xp` 的高 `rn` 位取补（`B-1 - a`），结果写到 `xp+2n-rn`。
  - 这是负剩余类的核心：把 `B^{...} - |X|` 转成 `|X|` 的表示。

#### 2.6.3 第二步：`mpn_mul_n` 与组合（L227-L231）

```c
/* Compute x_ju_j. FIXME:We need {xp+rn,rn}, mulhi? */
mpn_mul_n (xp, xp + 2 * n - rn, ip - rn, rn);
cy = mpn_add_n (xp + rn, xp + rn, xp + 2 * n - rn, 2 * rn - n);
cy = mpn_add_nc (ip - n, xp + 3 * rn - n, xp + n + rn, n - rn, cy);
MPN_INCR_U (ip - rn, rn, cy);
```

**解析**：

- **L228** `mpn_mul_n(xp, xp+2n-rn, ip-rn, rn)`：
  - 计算 `{xp+2n-rn, rn} * {ip-rn, rn}`（两段长度均为 `rn` 的乘法），结果 `2*rn` limb 写入 `xp[0..2rn-1]`。
  - `xp+2n-rn` 是第一步修正后归一化的"误差项"高位。
  - `ip-rn` 是当前低精度倒数。
  - 这一步对应 Newton 迭代 `X_new = X_old + X_old * (1 - D * X_old)` 中的乘法部分。
- **L229** `cy = mpn_add_n(xp+rn, xp+rn, xp+2n-rn, 2*rn-n)`：
  - 把 `xp[2n-rn .. 3rn-n-1]`（长度 `2*rn-n`）加到 `xp[rn .. 3rn-n-1]`。
  - 这是组合误差项与乘积的高位。
- **L230** `cy = mpn_add_nc(ip-n, xp+3rn-n, xp+n+rn, n-rn, cy)`：
  - 把 `xp[n+rn .. 2n-1]`（长度 `n-rn`）加到 `ip-n` 起的 `n-rn` 位，带进位 `cy`。
  - 结果写入 `{ip-n, n-rn}`（即输出区中 `rn` 之上的高位部分）。
- **L231** `MPN_INCR_U(ip-rn, rn, cy)`：
  - 把剩余的进位 `cy` 加到 `{ip-rn, rn}`（低 `rn` 位）。
  - 可能产生连锁进位。

#### 2.6.4 循环退出与保守进位检测（L232-L238）

```c
if (sizp == sizes) { /* Get out of the cycle */
  /* Check for possible carry propagation from below. */
  cy = xp[3 * rn - n - 1] > GMP_NUMB_MAX - CNST_LIMB (7); /* Be conservative. */
  /*    cy = mpn_add_1 (xp + rn, xp + rn, 2*rn - n, 4); */
  break;
}
rn = n;
```

- **L232** `sizp == sizes`：当 `sizp` 退到 `sizes` 数组开头（即已经处理完最高精度层），退出循环。
- **L234** `cy = xp[3*rn - n - 1] > GMP_NUMB_MAX - 7`：
  - 取 `xp` 中位置 `3*rn-n-1` 的 limb，若大于 `B-8` 则 `cy=1`，否则 `cy=0`。
  - 这是**保守**估计：该位置接近 `B-1` 时，可能在未来更高位迭代时产生进位传播。
  - 注释 L235 给出等价的非保守版本：`mpn_add_1(xp+rn, xp+rn, 2*rn-n, 4)`（加 4 看是否溢出）。
- **L236** `break`：退出循环。
- **L238** `rn = n`：若未退出，把当前层 `n` 作为下一层的 `rn`。

### 2.7 返回值（L240-L243）

```c
  TMP_FREE;
  return cy;
#undef xp
}
```

- `TMP_FREE`：释放 `TMP_ALLOC_LIMBS` 分配的栈内存。
- 返回 `cy`（最后一次迭代计算的保守进位估计），即接口约定中的 `e ∈ {0,1}`。
- `#undef xp`：清除 `xp` 宏定义。

---

## 3. `mpn_bc_invertappr`（base case，L67-L99）

```c
static mp_limb_t
mpn_bc_invertappr (mp_ptr ip, mp_srcptr dp, mp_size_t n, mp_ptr xp)
{
  ASSERT (n > 0);
  ASSERT (dp[n-1] & GMP_NUMB_HIGHBIT);
  ASSERT (! MPN_OVERLAP_P (ip, n, dp, n));
  ASSERT (! MPN_OVERLAP_P (ip, n, xp, mpn_invertappr_itch(n)));
  ASSERT (! MPN_OVERLAP_P (dp, n, xp, mpn_invertappr_itch(n)));
  /* Compute a base value of r limbs. */
  if (n == 1)
    invert_limb (*ip, *dp);
  else {
    /* n > 1 here */
    MPN_FILL (xp, n, GMP_NUMB_MAX);
    mpn_com (xp + n, dp, n);
    /* Now xp contains B^2n - {dp,n}*B^n - 1 */
    /* FIXME: if mpn_*pi1_divappr_q handles n==2, use it! */
    if (n == 2) {
      mpn_divrem_2 (ip, 0, xp, 4, dp);
    } else {
      gmp_pi1_t inv;
      invert_pi1 (inv, dp[n-1], dp[n-2]);
      if (! MAYBE_dcpi1_divappr
          || BELOW_THRESHOLD (n, DC_DIVAPPR_Q_THRESHOLD))
        mpn_sbpi1_divappr_q (ip, xp, 2 * n, dp, n, inv.inv32);
      else
        mpn_dcpi1_divappr_q (ip, xp, 2 * n, dp, n, &inv);
      MPN_DECR_U(ip, n, CNST_LIMB (1));
      return 1;
    }
  }
  return 0;
}
```

**解析**：
- `n==1`：单 limb，直接用 `invert_limb`（硬件指令或查表）。
- `n>1`：
  - 构造 `xp = B^{2n} - 1 - {dp,n}*B^n`（即 `MPN_FILL(xp, n, B-1)` 后 `mpn_com(xp+n, dp, n)`）。
  - 注释 "Now xp contains `B^2n - {dp,n}*B^n - 1`"。
  - `n==2`：用 `mpn_divrem_2` 直接除。
  - `n>2`：用 `mpn_sbpi1_divappr_q`（small）或 `mpn_dcpi1_divappr_q`（divide & conquer）做近似除法，最后 `MPN_DECR_U(ip, n, 1)` 减 1，返回 `e=1`。

---

## 4. `mpn_invertappr`（顶层分发，L244-L256）

```c
mp_limb_t
mpn_invertappr (mp_ptr ip, mp_srcptr dp, mp_size_t n, mp_ptr scratch)
{
  ASSERT (n > 0);
  ASSERT (dp[n-1] & GMP_NUMB_HIGHBIT);
  ASSERT (! MPN_OVERLAP_P (ip, n, dp, n));
  ASSERT (! MPN_OVERLAP_P (ip, n, scratch, mpn_invertappr_itch(n)));
  ASSERT (! MPN_OVERLAP_P (dp, n, scratch, mpn_invertappr_itch(n)));
  if (BELOW_THRESHOLD (n, INV_NEWTON_THRESHOLD))
    return mpn_bc_invertappr (ip, dp, n, scratch);
  else
    return mpn_ni_invertappr (ip, dp, n, scratch);
}
```

- 公开接口：根据 `n` 是否超过 `INV_NEWTON_THRESHOLD` 分发到 base case 或 Newton 迭代版本。

---

## 5. 辅助函数说明

### 5.1 `mpn_mulmod_bnm1`

```c
void mpn_mulmod_bnm1 (mp_ptr rp, mp_size_t mn,
                      mp_srcptr ap, mp_size_t an,
                      mp_srcptr bp, mp_size_t bn,
                      mp_ptr scratch);
```

- **功能**：计算 `{ap,an} * {bp,bn} mod (B^mn - 1)`，结果 `mn` limb 写入 `rp`。
- **数学**：cyclic convolution。`B^mn ≡ 1 (mod B^mn-1)`，所以超过 `mn` 的高位会"绕回"加到低位。
- **实现**：通常用 FFT 或 NTT 加速（GMP 内部根据 `mn` 大小选择不同后端）。
- **用途**：在 Newton 迭代第一步中，把 `{ip,rn} * {dp,n}` 用 cyclic 计算到 `mn` limb，节省约一半 FFT 长度。
- **约束**：`mn` 由 `mpn_mulmod_bnm1_next_size` 给出，GMP 支持非 2 幂；用户自实现版本（如 `fftMulModBm1`）若要求 2 幂，则覆盖率受影响（见 `FFT_MULMOD_INTEGRATION_DESIGN.md` 3.3 节）。
- **scratch**：通过 `mpn_mulmod_bnm1_itch(mn, an, bn)` 计算所需 scratch 大小。

### 5.2 `mpn_sublsh1_n`

```c
mp_limb_t mpn_sublsh1_n (mp_ptr rp, mp_srcptr ap, mp_srcptr bp, mp_size_t n);
```

- **功能**：计算 `rp = ap - (bp << 1)`（即 `bp` 左移 1 位后从 `ap` 减去），长度 `n` limb。
- **返回**：借位（0 或 1）。
- **用途**：在正剩余类修正中（L188-L196），当 `xp >= dp` 时，一次减去两倍 `dp`，等价于 `sub_n` 两次，但更快（原生指令）。
- **可用性**：由 `HAVE_NATIVE_mpn_sublsh1_n` 宏控制；不可用时退化为两次 `mpn_sub_n`（L197-L202）。

### 5.3 `mpn_rsblsh1_n`

```c
mp_limb_t mpn_rsblsh1_n (mp_ptr rp, mp_srcptr ap, mp_srcptr bp, mp_size_t n);
```

- **功能**：计算 `rp = (bp << 1) - ap`（反向：左移 1 位的 `bp` 减去 `ap`），长度 `n` limb。
- **返回**：借位。
- **用途**：正剩余类修正第二段（L204-L209），当 `xp >= dp` 时计算 `(dp << 1) - xp`，写入 `xp+n`。
- **可用性**：由 `HAVE_NATIVE_mpn_rsblsh1_n` 控制；不可用时退化为 `sub_n` + `sub_nc`（L210-L216）。

### 5.4 `mpn_com`

```c
void mpn_com (mp_ptr rp, mp_srcptr ap, mp_size_t n);
```

- **功能**：按位取补，`rp[i] = B - 1 - ap[i]`（即 `GMP_NUMB_MAX - ap[i]`），长度 `n` limb。
- **数学**：等价于 `rp = (B^n - 1) - ap`。
- **用途**：
  1. `mpn_bc_invertappr` L81：构造 `xp = B^{2n} - 1 - {dp,n}*B^n`。
  2. `mpn_ni_invertappr` L225（负剩余类）：把 `xp` 高 `rn` 位从 "B^rn - 1 - |X|" 形式转成 `|X|`。

### 5.5 `mpn_add_n` / `mpn_add_nc`

```c
mp_limb_t mpn_add_n (mp_ptr rp, mp_srcptr ap, mp_srcptr bp, mp_size_t n);
mp_limb_t mpn_add_nc (mp_ptr rp, mp_srcptr ap, mp_srcptr bp, mp_size_t n, mp_limb_t carry_in);
```

- **`mpn_add_n`**：`{rp,n} = {ap,n} + {bp,n}`，返回最高进位（0 或 1）。
- **`mpn_add_nc`**：带初始进位 `carry_in` 的版本，`{rp,n} = {ap,n} + {bp,n} + carry_in`。
- **用途**：cyclic 修正中加 `dp * B^rn`（L177-L178）、组合步骤（L229-L230）。

### 5.6 `mpn_sub_n` / `mpn_sub_nc`

```c
mp_limb_t mpn_sub_n (mp_ptr rp, mp_srcptr ap, mp_srcptr bp, mp_size_t n);
mp_limb_t mpn_sub_nc (mp_ptr rp, mp_srcptr ap, mp_srcptr bp, mp_size_t n, mp_limb_t borrow_in);
```

- **`mpn_sub_n`**：`{rp,n} = {ap,n} - {bp,n}`，返回最高借位（0 或 1，1 表示下溢）。
- **`mpn_sub_nc`**：带初始借位的版本。
- **用途**：正剩余类修正（L195, L198-L199, L209, L212, L215）。

### 5.7 `mpn_mul` / `mpn_mul_n`

```c
mp_limb_t mpn_mul (mp_ptr rp, mp_srcptr ap, mp_size_t an, mp_srcptr bp, mp_size_t bn);
void      mpn_mul_n (mp_ptr rp, mp_srcptr ap, mp_srcptr bp, mp_size_t n);
```

- **`mpn_mul`**：不等长乘法，`{rp, an+bn} = {ap,an} * {bp,bn}`。
- **`mpn_mul_n`**：等长乘法，`{rp, 2n} = {ap,n} * {bp,n}`。
- **用途**：
  - L167：常规分支第一步 `{dp,n} * {ip,rn}`。
  - L228：第二步 `{xp+2n-rn, rn} * {ip-rn, rn}`。

### 5.8 `MPN_DECR_U` / `MPN_INCR_U`

```c
#define MPN_DECR_U(ptr, size, limb)  /* {ptr,size} -= limb, 可能借位传播 */
#define MPN_INCR_U(ptr, size, limb)  /* {ptr,size} += limb, 可能进位传播 */
```

- **`MPN_DECR_U(ptr, size, limb)`**：从大整数 `{ptr, size}` 减去单 limb `limb`，处理借位传播。若 `limb >= B`，等价于减 `limb mod B` 并传播 `limb / B` 的借位。
- **`MPN_INCR_U(ptr, size, limb)`**：对称的加法版本。
- **用途**（在 `mpn_ni_invertappr` 中）：
  - L181-L182：减 `B^{rn+n}` 修正。
  - L217：正剩余类最终修正 `{ip-rn, rn} -= cy`。
  - L220：负剩余类减截断进位 `{xp, n+1} -= cy`。
  - L222：负剩余类加 1 到 `{ip-rn, rn}`。
  - L231：组合步骤进位传播 `{ip-rn, rn} += cy`。
- **实现**：通常是宏，展开为 `mpn_sub_1` / `mpn_add_1`（带借位/进位传播循环）。

### 5.9 `mpn_mulmod_bnm1_next_size`

```c
mp_size_t mpn_mulmod_bnm1_next_size (mp_size_t n);
```

- **功能**：返回 `>= n` 且适合作为 `mpn_mulmod_bnm1` 模长的尺寸（GMP 内部根据 FFT 后端选择，通常支持非 2 幂以匹配 FFT 长度）。
- **用途**：L152, L165 决定 cyclic 模长 `mn`。

### 5.10 `mpn_cmp`

```c
int mpn_cmp (mp_srcptr ap, mp_srcptr bp, mp_size_t n);
```

- **功能**：比较 `{ap,n}` 与 `{bp,n}`，返回 -1/0/1。
- **用途**：正剩余类修正中判断 `xp` 与 `dp` 的大小关系（L189, L205, L211, L209, L215）。

### 5.11 `ASSERT_CARRY` / `ASSERT_NOCARRY`

```c
#define ASSERT_CARRY(expr)    /* 断言 expr 返回非零（有进/借位） */
#define ASSERT_NOCARRY(expr)  /* 断言 expr 返回零（无进/借位） */
```

- 调试用宏，Release 构建中展开为空。

---

## 6. 指针运算展开（重点）

源码中大量使用 `dp - n`、`ip - rn`、`xp + 2*n - rn` 等指针表达式。下面统一展开：

### 6.1 `dp` 与 `ip` 的"前移"约定

```c
dp += n;   /* L145 */
ip += n;   /* L146 */
```

- 执行后：
  - `dp` 指向**原 `{dp,n}` 的尾后**（即 `dp[n]` 越界位置）。
  - `dp - k`（`1 <= k <= n`）指向原 `{dp,n}` 的**最高 k 位的起点**，即原 `dp[n-k]`。
  - 同理 `ip - k` 指向**输出区 `{ip,n}` 最高 k 位的起点**。
- 这种约定让"取高 k 位"只需写 `dp - k`，无需在每次调用时显式偏移。

### 6.2 关键指针表达式

| 表达式 | 实际指向 | 长度 | 含义 |
|---|---|---|---|
| `dp - n` | 原 `dp[0]` | `n` | 整个 divisor |
| `dp - rn` | 原 `dp[n-rn]` | `rn` | divisor 的高 `rn` 位 |
| `ip - rn` | 原 `ip[n-rn]` | `rn` | 当前低精度倒数（base case 或上次迭代结果） |
| `ip - n` | 原 `ip[0]` | `n` | 整个输出倒数区 |
| `xp` | scratch 起点 | 视上下文 | scratch 别名 |
| `xp + rn` | scratch 偏 `rn` | 视上下文 | 用于第一步乘积高位、第二步组合 |
| `xp + n` | scratch 偏 `n` | 视上下文 | 正/负剩余类判定位置（`xp[n]`） |
| `xp + 2*n - rn` | scratch 偏 `2n-rn` | `rn` | 第一步修正后归一化误差项（第二步乘数 1） |
| `xp + 3*rn - n` | scratch 偏 `3rn-n` | `n-rn` | 第二步乘积的高位片段 |
| `xp + n + rn` | scratch 偏 `n+rn` | `n-rn` | 第二步组合中的另一片段 |
| `xp + rn + n - mn` | scratch 偏 `rn+n-mn` | `2*mn+1-rn-n` | cyclic 分支减 `B^{rn+n}` 的起点 |

### 6.3 `xp` 内存布局（cyclic 分支，单次迭代）

```
偏移:   0          rn         n          mn         rn+n-mn     2n-rn     3rn-n
        +----------+----------+----------+----------+-----------+----------+
xp:     |<-- mn -->|          |          |          |           |          |
        | mulmod   |          |<-- xp[n]判定位 -->   |           |          |
        +----------+----------+----------+----------+-----------+----------+
                                       ^
                                       xp[n] 用于正/负剩余类判定
```

- `xp[0..mn-1]`：`mpn_mulmod_bnm1` 输出（cyclic 乘积）。
- `xp[mn]`：哨兵位，初值 1，用于 `MPN_DECR_U` 借位终点。
- `xp[n]`：正/负剩余类判定（`n` 可能大于 `mn`，所以这个位置在 cyclic 模长之外，反映"溢出"的符号）。

### 6.4 `xp` 内存布局（第二步 `mpn_mul_n` 之后）

```
偏移:   0                rn              2rn
        +----------------+----------------+
xp:     |<--   rn   -->  |<--   rn   -->  |
        | low(r*error)   | high(r*error)  |
        +----------------+----------------+
                          ^               ^
                          xp+rn           xp+2rn
```

- `mpn_mul_n(xp, xp+2n-rn, ip-rn, rn)` 把 `2*rn` limb 乘积写入 `xp[0..2rn-1]`。
- `xp[rn..3rn-n-1]`：与误差项高位 `xp[2n-rn..3rn-n-1]` 相加（L229）。
- `xp[3rn-n..2rn-1]`：加到 `ip-n`（L230），即输出倒数的高 `n-rn` 位。
- `xp[3rn-n-1]`：保守进位检测位置（L234）。

---

## 7. 算法整体流程总结

```
mpn_invertappr(ip, dp, n, scratch):
  if n < INV_NEWTON_THRESHOLD:
    return mpn_bc_invertappr(ip, dp, n, scratch)  # base case
  else:
    return mpn_ni_invertappr(ip, dp, n, scratch)  # Newton 迭代

mpn_ni_invertappr(ip, dp, n, scratch):
  1. 计算各层精度 sizes[] = [n, n1, n2, ..., rn_base]，rn = (n>>1)+1 递减
  2. dp += n; ip += n;  # 前移指针，方便取高位
  3. mpn_bc_invertappr(ip-rn, dp-rn, rn, scratch)  # 算 base case 倒数
  4. 若 n >= INV_MULMOD_BNM1_THRESHOLD:
       mn = next_size(n+1); 分配 tp
  5. while (有下一层 n = *--sizp):
       第一步 (cyclic 或常规):
         - cyclic: xp = (ip*dp + dp*B^rn - B^{rn+n}) mod (B^mn-1)
         - 常规:   xp = (ip*dp + dp*B^rn) 截断到 n+1 位
       正/负剩余类判定 (xp[n]):
         - 正 (xp[n] < 2):  减 dp 归一化, ip -= cy
         - 负 (xp[n] >= B-2): 取补, ip += 1
       第二步:
         xp = xp_high * ip  (mpn_mul_n)
         ip = 组合(xp, ip, cy)  (mpn_add_n + mpn_add_nc + MPN_INCR_U)
       若回到最高层: 检测保守进位, break
       否则: rn = n, 继续下层
  6. return cy  # e ∈ {0,1}
```

---

## 8. 与本项目（`moptm_fusion.cpp`）的集成要点

参见 `d:\precious_speed\FFT_MULMOD_INTEGRATION_DESIGN.md`：

- 本项目的 `fftMulModBm1` 要求 `m` 是 2 幂，而 GMP 的 `mpn_mulmod_bnm1_next_size` 支持非 2 幂。
- 对 `k=128/256/512/1024` 等 2 幂 `k`，cyclic 会退化（`mn = n+1` 也是 2 幂时与 `n+rn` 相等，cyclic 无收益）。
- 正剩余类路径覆盖约 99% 用例，负剩余类路径需额外实现 `mpn_com` 等价逻辑。
- 第二步 `mpn_mul_n` 仍是 full mul，部分抵消 cyclic 收益，但 FFT 长度峰值从 `2^18` 降到 `2^17`。

---

## 9. 关键不变量与正确性依据

### 9.1 cyclic 不变量（L174 注释）

> `2*|ip*dp + dp*B^rn - B^{rn+n}| < B^mn-1`

- 这是 cyclic convolution 能无歧义表示真实值 `X = ip*dp + dp*B^rn - B^{rn+n}` 的数学保证。
- 若 `|2X| >= B^mn-1`，则 `X mod (B^mn-1)` 会绕回，无法区分正负。
- GMP 通过选择 `mn = next_size(n+1)` 且 `mn <= n+rn` 保证此不变量。

### 9.2 正/负剩余类判定（L185, L218）

- `xp[n] < 2`：正剩余类，`X >= 0`，`xp` 直接是 `X` 的（带冗余）表示。
- `xp[n] >= B-2`：负剩余类，`X < 0`，`xp` 是 `B^{...} - |X|` 的表示。
- 中间值（`2 <= xp[n] <= B-3`）不应出现，由不变量 9.1 保证。

### 9.3 保守进位检测（L234）

> `cy = xp[3*rn - n - 1] > GMP_NUMB_MAX - 7`

- 该位置接近 `B-1` 时，可能在更高位迭代时产生连锁进位。
- 保守返回 `e=1`，让上层（如 `mpn_invert`）做最终修正。

---

## 10. 参考链接

- 源码: https://raw.githubusercontent.com/alisw/GMP/master/mpn/generic/invertappr.c
- GMP 主仓库: https://gmplib.org/repo/gmp/
- 算法依据: Brent & Zimmermann《Modern Computer Arithmetic》算法 3.5
- 本项目设计文档: `d:\precious_speed\FFT_MULMOD_INTEGRATION_DESIGN.md`

---

**文档结束**

**获取声明**: 本文档源码通过 WebFetch 从 `raw.githubusercontent.com/alisw/GMP/master/mpn/generic/invertappr.c` 获取（其他 URL 如 `gmplib.org/repo/gmp/file/tip/...` 与 `github.com/alisw/GMP/blob/master/...` 均获取失败）。源码共 256 行，已完整粘贴于第 1 节，未省略任何一行。中文注释基于源码原文注释、GMP 文档约定、以及 `FFT_MULMOD_INTEGRATION_DESIGN.md` 中的分析。指针运算展开为本文档原创分析。
