# GMP 索引 06: gmp-impl.h 内部头文件

## 源文件信息
- 源路径: `E:\gmp-6.3.0\gmp-impl.h`
- 总行数: 4485
- 版权: 1991-2018, 2021, 2022 Free Software Foundation, Inc.
- 文件性质: GMP 内部实现头文件，包含所有宏、内部函数原型、阈值声明。**内容仅供内部使用，未来版本可能不兼容**。
- 包含关系: `#include "config.h"`, `gmp.h`, `gmp-mparam.h`, `fib_table.h`, `fac_table.h`, `sieve_table.h`, `mp_bases.h`, 以及可选的 `fat.h`（WANT_FAT_BINARY）。

---

## 1. 关键宏定义 (按功能分类)

### 1.1 乘法/Toom 阈值 (L2147-L2248, L2363-L2407)

| 宏名 | 默认值 | 行号 | 说明 |
|---|---|---|---|
| `MUL_TOOM22_THRESHOLD` | 30 | L2148 | Karatsuba/Toom22 启用阈值 |
| `MUL_TOOM33_THRESHOLD` | 100 | L2152 | Toom33 启用阈值 |
| `MUL_TOOM44_THRESHOLD` | 300 | L2156 | Toom44 启用阈值 |
| `MUL_TOOM6H_THRESHOLD` | 350 | L2160 | Toom6H 启用阈值 |
| `MUL_TOOM8H_THRESHOLD` | 450 | L2168 | Toom8H 启用阈值 |
| `MUL_TOOM32_TO_TOOM43_THRESHOLD` | 100 | L2176 | Toom32→Toom43 切换 |
| `MUL_TOOM32_TO_TOOM53_THRESHOLD` | 110 | L2180 | Toom32→Toom53 切换 |
| `MUL_TOOM42_TO_TOOM53_THRESHOLD` | 100 | L2184 | Toom42→Toom53 切换 |
| `MUL_TOOM42_TO_TOOM63_THRESHOLD` | 110 | L2188 | Toom42→Toom63 切换 |
| `MUL_TOOM43_TO_TOOM54_THRESHOLD` | 150 | L2192 | Toom43→Toom54 切换 |
| `MUL_TOOM22_THRESHOLD_LIMIT` | =MUL_TOOM22_THRESHOLD | L2200 | Toom22 上限（fat binary 用） |
| `MUL_TOOM33_THRESHOLD_LIMIT` | =MUL_TOOM33_THRESHOLD | L2203 | Toom33 上限 |
| `MULLO_BASECASE_THRESHOLD_LIMIT` | =MULLO_BASECASE_THRESHOLD | L2206 | Mullo basecase 上限 |
| `SQRLO_BASECASE_THRESHOLD_LIMIT` | =SQRLO_BASECASE_THRESHOLD | L2209 | Sqrlo basecase 上限 |
| `SQRLO_DC_THRESHOLD_LIMIT` | =SQRLO_DC_THRESHOLD | L2212 | Sqrlo DC 上限 |

### 1.2 平方阈值 (L2225-L2245)

| 宏名 | 默认值 | 行号 | 说明 |
|---|---|---|---|
| `SQR_BASECASE_THRESHOLD` | 0 | L2226 | 从 0 起就用 mpn_sqr_basecase |
| `SQR_TOOM2_THRESHOLD` | 50 | L2230 | sqr→toom2 |
| `SQR_TOOM3_THRESHOLD` | 120 | L2234 | sqr→toom3 |
| `SQR_TOOM4_THRESHOLD` | 400 | L2238 | sqr→toom4 |
| `SQR_TOOM6_THRESHOLD` | =MUL_TOOM6H_THRESHOLD | L2164 | sqr→toom6 |
| `SQR_TOOM8_THRESHOLD` | =MUL_TOOM8H_THRESHOLD | L2172 | sqr→toom8 |
| `SQR_TOOM3_THRESHOLD_LIMIT` | =SQR_TOOM3_THRESHOLD | L2243 | Toom3 上限 |

### 1.3 Mulmid / Mullo / Sqrlo 阈值 (L2246-L2272)

| 宏名 | 默认值 | 行号 | 说明 |
|---|---|---|---|
| `MULMID_TOOM42_THRESHOLD` | =MUL_TOOM22_THRESHOLD | L2247 | mulmid→toom42_mulmid |
| `MULLO_BASECASE_THRESHOLD` | 0 | L2251 | 不用 mul_basecase |
| `MULLO_DC_THRESHOLD` | 2*MUL_TOOM22_THRESHOLD | L2255 | mullo→dc |
| `MULLO_MUL_N_THRESHOLD` | 2*MUL_FFT_THRESHOLD | L2259 | mullo→mul_n |
| `SQRLO_BASECASE_THRESHOLD` | 0 | L2263 | 不用 sqr_basecase |
| `SQRLO_DC_THRESHOLD` | =MULLO_DC_THRESHOLD | L2267 | sqrlo→dc |
| `SQRLO_SQR_THRESHOLD` | =MULLO_MUL_N_THRESHOLD | L2271 | sqrlo→sqr |

### 1.4 除法/逆元阈值 (L2274-L2336, L3290-L3318)

| 宏名 | 默认值 | 行号 | 说明 |
|---|---|---|---|
| `DC_DIV_QR_THRESHOLD` | 2*MUL_TOOM22_THRESHOLD (L2275)，后覆盖为 3*MUL_TOOM22_THRESHOLD (L2423) | L2275/L2423 | DC div_qr 启用 |
| `DC_DIVAPPR_Q_THRESHOLD` | 200 | L2279 | DC divappr_q 启用 |
| `DC_BDIV_QR_THRESHOLD` | 2*MUL_TOOM22_THRESHOLD | L2283 | DC bdiv_qr |
| `DC_BDIV_Q_THRESHOLD` | 180 | L2287 | DC bdiv_q |
| `DIVEXACT_JEB_THRESHOLD` | 25 | L2291 | Jebelean 精确除法阈值 |
| `INV_MULMOD_BNM1_THRESHOLD` | 4*MULMOD_BNM1_THRESHOLD | L2295 | 逆元用 mulmod_bnm1 |
| `INV_APPR_THRESHOLD` | =INV_NEWTON_THRESHOLD | L2299 | 近似逆元阈值 |
| `INV_NEWTON_THRESHOLD` | 200 | L2303 | Newton 逆元阈值 |
| `BINV_NEWTON_THRESHOLD` | 300 | L2307 | 二项逆元阈值 |
| `MU_DIVAPPR_Q_THRESHOLD` | 2000 | L2311 | MU divappr_q 启用 |
| `MU_DIV_QR_THRESHOLD` | 2000 | L2315 | MU div_qr 启用 |
| `MUPI_DIV_QR_THRESHOLD` | 200 | L2319 | MUPI div_qr |
| `MU_BDIV_Q_THRESHOLD` | 2000 | L2323 | MU bdiv_q |
| `MU_BDIV_QR_THRESHOLD` | 2000 | L2327 | MU bdiv_qr |
| `DIVEXACT_1_THRESHOLD` | 0 | L3314 | divexact_1 阈值 |
| `BMOD_1_TO_MOD_1_THRESHOLD` | 10 | L3317 | modexact_1_odd→mod_1 |
| `PREINV_MOD_1_TO_MOD_1_THRESHOLD` | 10 | L3291 | preinv_mod_1→mod_1 |
| `DIVREM_1_NORM_THRESHOLD` | MP_SIZE_T_MAX (nails 时禁用) | L617 | divrem_1 规范化阈值 |
| `DIVREM_1_UNNORM_THRESHOLD` | MP_SIZE_T_MAX (nails 时禁用) | L618 | divrem_1 非规范化阈值 |
| `MOD_1_NORM_THRESHOLD` | MP_SIZE_T_MAX (nails 时禁用) | L619 | mod_1 规范化阈值 |
| `MOD_1_UNNORM_THRESHOLD` | MP_SIZE_T_MAX (nails 时禁用) | L620 | mod_1 非规范化阈值 |
| `DIVREM_2_THRESHOLD` | MP_SIZE_T_MAX (nails 时禁用) | L622 | divrem_2 阈值 |
| `USE_PREINV_DIVREM_1` | 1 (默认启用) | L3279 | 是否用 preinv 版本 |

### 1.5 模乘阈值 (L2330-L2340, L2363-L2407)

| 宏名 | 默认值 | 行号 | 说明 |
|---|---|---|---|
| `MULMOD_BNM1_THRESHOLD` | 16 | L2331 | mulmod_bnm1 启用 |
| `SQRMOD_BNM1_THRESHOLD` | 16 | L2335 | sqrmod_bnm1 启用 |
| `MUL_TO_MULMOD_BNM1_FOR_2NXN_THRESHOLD` | =INV_MULMOD_BNM1_THRESHOLD/2 | L2339 | mul→mulmod_bnm1 |
| `FFT_FIRST_K` | 4 | L2363 | modF FFT 首个 k 值 |
| `MUL_FFT_MODF_THRESHOLD` | 3*MUL_TOOM33_THRESHOLD | L2367 | modF FFT 启用 |
| `SQR_FFT_MODF_THRESHOLD` | 3*SQR_TOOM3_THRESHOLD | L2371 | modF FFT 平方启用 |
| `MUL_FFT_THRESHOLD` | 10*MUL_FFT_MODF_THRESHOLD (~3000) | L2379 | NxN→2N FFT 启用 |
| `SQR_FFT_THRESHOLD` | 10*SQR_FFT_MODF_THRESHOLD | L2383 | NxN→2N FFT 平方启用 |
| `MUL_FFT_TABLE` | {k=5..10 阶梯} | L2389 | modF FFT k 选择表 |
| `SQR_FFT_TABLE` | {k=5..10 阶梯} | L2399 | modF FFT k 选择表（平方） |
| `MPN_FFT_TABLE_SIZE` | 16 | L2419 | FFT 表大小 |

### 1.6 REDC 阈值 (L2342-L2357)

| 宏名 | 默认值 | 行号 | 说明 |
|---|---|---|---|
| `REDC_1_TO_REDC_2_THRESHOLD` | 15 (有 native addmul_2/redc_2) | L2345 | redc_1→redc_2 |
| `REDC_2_TO_REDC_N_THRESHOLD` | 100 | L2349 | redc_2→redc_n |
| `REDC_1_TO_REDC_N_THRESHOLD` | 100 (无 native addmul_2/redc_2) | L2354 | redc_1→redc_n |

### 1.7 字符串转换 / 阶乘阈值 (L2426-L2448)

| 宏名 | 默认值 | 行号 | 说明 |
|---|---|---|---|
| `GET_STR_DC_THRESHOLD` | 18 | L2427 | get_str→dc |
| `GET_STR_PRECOMPUTE_THRESHOLD` | 35 | L2431 | get_str→预计算 |
| `SET_STR_DC_THRESHOLD` | 750 | L2435 | set_str→dc |
| `SET_STR_PRECOMPUTE_THRESHOLD` | 2000 | L2439 | set_str→预计算 |
| `FAC_ODD_THRESHOLD` | 35 | L2443 | 阶乘 odd 启用 |
| `FAC_DSC_THRESHOLD` | 400 | L2447 | 阶乘 dsc 启用 |

### 1.8 HGCD / GCD 阈值 (L4264-L4409)

| 宏名 | 默认值 | 行号 | 说明 |
|---|---|---|---|
| `MATRIX22_STRASSEN_THRESHOLD` | 30 | L4265 | 2x2 矩阵乘 Strassen 启用 |
| `HGCD_THRESHOLD` | 400 | L4392 | HGCD 启用 |
| `HGCD_APPR_THRESHOLD` | 400 | L4396 | HGCD 近似启用 |
| `HGCD_REDUCE_THRESHOLD` | 1000 | L4400 | HGCD reduce 启用 |
| `GCD_DC_THRESHOLD` | 1000 | L4404 | GCD DC 启用 |
| `GCDEXT_DC_THRESHOLD` | 600 | L4408 | GCDext DC 启用 |

### 1.9 Toom 最小尺寸常量 (L1433-L1456)

| 宏名 | 值 | 行号 |
|---|---|---|
| `MPN_TOOM22_MUL_MINSIZE` | 6 | L1433 |
| `MPN_TOOM2_SQR_MINSIZE` | 4 | L1434 |
| `MPN_TOOM33_MUL_MINSIZE` | 17 | L1436 |
| `MPN_TOOM3_SQR_MINSIZE` | 17 | L1437 |
| `MPN_TOOM44_MUL_MINSIZE` | 30 | L1439 |
| `MPN_TOOM4_SQR_MINSIZE` | 30 | L1440 |
| `MPN_TOOM6H_MUL_MINSIZE` | 46 | L1442 |
| `MPN_TOOM6_SQR_MINSIZE` | 46 | L1443 |
| `MPN_TOOM8H_MUL_MINSIZE` | 86 | L1445 |
| `MPN_TOOM8_SQR_MINSIZE` | 86 | L1446 |
| `MPN_TOOM32_MUL_MINSIZE` | 10 | L1448 |
| `MPN_TOOM42_MUL_MINSIZE` | 10 | L1449 |
| `MPN_TOOM43_MUL_MINSIZE` | 25 | L1450 |
| `MPN_TOOM53_MUL_MINSIZE` | 17 | L1451 |
| `MPN_TOOM54_MUL_MINSIZE` | 31 | L1452 |
| `MPN_TOOM63_MUL_MINSIZE` | 49 | L1453 |
| `MPN_TOOM42_MULMID_MINSIZE` | 4 | L1455 |

### 1.10 阈值判断辅助宏 (L1412-L1430)

- `ABOVE_THRESHOLD(size, thresh)` (L1419/L1424): size>=thresh 时为真，支持 0/MP_SIZE_T_MAX 特殊值
- `BELOW_THRESHOLD(size, thresh)` (L1429): 上式的否定

### 1.11 常用辅助宏 (L530-L600, L633-L731)

#### 通用宏
- `CRAY_Pragma(str)` (L531): 映射到 `__GMP_CRAY_Pragma`
- `MPN_CMP(result, xp, yp, size)` (L532): 包装 `__GMPN_CMP`
- `LIKELY(cond)` (L533): 包装 `__GMP_LIKELY`
- `UNLIKELY(cond)` (L534): 包装 `__GMP_UNLIKELY`
- `ABS(x)` (L536): 绝对值
- `NEG_CAST(T,x)` (L537): 类型 T 下取负
- `ABS_CAST(T,x)` (L538): 类型 T 下绝对值
- `MIN(l,o)` (L540), `MAX(h,i)` (L542)
- `numberof(x)` (L543): 数组元素数
- `POW2_P(n)` (L558): 2 的幂判定
- `LOG2C(n)` (L562): 编译期 log2 上取整
- `MP_LIMB_T_MAX` (L568): `~(mp_limb_t) 0`
- `ULONG_HIGHBIT`/`UINT_HIGHBIT`/`USHRT_HIGHBIT` (L573-L575)
- `GMP_LIMB_HIGHBIT` (L576)
- `MP_SIZE_T_MAX/MIN` (L579/L582)
- `MP_EXP_T_MAX/MIN` (L587/L588)
- `LONG_HIGHBIT`/`INT_HIGHBIT`/`SHRT_HIGHBIT` (L590-L592)
- `GMP_NUMB_HIGHBIT` (L595)
- `GMP_NAIL_LOWBIT` (L598/L600)
- `GMP_LIMB_BYTES` = `SIZEOF_MP_LIMB_T` (L224)
- `GMP_LIMB_BITS` = `8*SIZEOF_MP_LIMB_T` (L227)
- `BITS_PER_ULONG` = `8*SIZEOF_UNSIGNED_LONG` (L230)

#### 字段访问宏 (L545-L553)
- `SIZ(x)` → `((x)->_mp_size)` (L546)
- `ABSIZ(x)` → `ABS(SIZ(x))` (L547)
- `PTR(x)` → `((x)->_mp_d)` (L548)
- `EXP(x)` → `((x)->_mp_exp)` (L549)
- `PREC(x)` → `((x)->_mp_prec)` (L550)
- `ALLOC(x)` → `((x)->_mp_alloc)` (L551)
- `NUM(x)` → `mpq_numref(x)` (L552)
- `DEN(x)` → `mpq_denref(x)` (L553)

#### 交换宏 (L633-L694)
- `MP_LIMB_T_SWAP(x, y)` (L633)
- `MP_SIZE_T_SWAP(x, y)` (L639)
- `MP_PTR_SWAP(x, y)` (L646)
- `MP_SRCPTR_SWAP(x, y)` (L652)
- `MPN_PTR_SWAP(xp,xs, yp,ys)` (L659)
- `MPN_SRCPTR_SWAP(xp,xs, yp,ys)` (L664)
- `MPZ_PTR_SWAP(x, y)` (L670)
- `MPZ_SRCPTR_SWAP(x, y)` (L676)
- `MPQ_PTR_SWAP(x, y)` (L683)
- `MPQ_SRCPTR_SWAP(x, y)` (L689)

#### 内存分配宏 (L699-L731)
- `__gmp_allocate_func`/`__gmp_reallocate_func`/`__gmp_free_func` (L699-L701)
- `__GMP_ALLOCATE_FUNC_TYPE(n,type)` (L707)
- `__GMP_ALLOCATE_FUNC_LIMBS(n)` (L709)
- `__GMP_REALLOCATE_FUNC_TYPE/2` (L711/L714)
- `__GMP_FREE_FUNC_TYPE/LIMBS` (L717/L718)
- `__GMP_REALLOCATE_FUNC_MAYBE` (L720)
- `__GMP_REALLOCATE_FUNC_MAYBE_TYPE` (L726)

#### regparm 宏 (L756-L772)
- `USE_LEADING_REGPARM` (L758/L760): x86 + GCC 2.96+ + !PIC + !profiling 时启用
- `REGPARM_2_1(a,b,x)` (L765/L769)
- `REGPARM_3_1(a,b,c,x)` (L766/L770)
- `REGPARM_ATTR(n)` (L767/L771)

#### TMP_ALLOC 宏族 (L329-L528)
- `TMP_DECL`/`TMP_MARK`/`TMP_ALLOC`/`TMP_FREE` (alloca/reentrant/notreentrant/debug 各变体)
- `TMP_SDECL`/`TMP_SMARK`/`TMP_SALLOC`/`TMP_SFREE`
- `TMP_BALLOC(n)` (大块分配)
- `TMP_ALLOC_TYPE(n,type)` (L486), `TMP_SALLOC_TYPE` (L487), `TMP_BALLOC_TYPE` (L488)
- `TMP_ALLOC_LIMBS(n)` (L489), `TMP_SALLOC_LIMBS` (L490), `TMP_BALLOC_LIMBS` (L491)
- `TMP_ALLOC_MP_PTRS(n)` (L492), `TMP_SALLOC_MP_PTRS` (L493), `TMP_BALLOC_MP_PTRS` (L494)
- `TMP_ALLOC_LIMBS_2(xp,xsize, yp,ysize)` (L501)
- `TMP_ALLOC_LIMBS_3(xp,xsize, yp,ysize, zp,zsize)` (L514)
- `ROUND_UP_MULTIPLE(a,m)` (L360)
- `__TMP_ALIGN` (L352)

#### 拷贝/填充/规范化宏 (L794-L2073)
- `MPN_COPY_INCR(dst, src, n)` (L794/L1823/L1834)
- `MPN_COPY_DECR(dst, src, n)` (L1860/L1876/L1887)
- `MPN_COPY(d,s,n)` (L1914)
- `MPN_REVERSE(dst, src, size)` (L1923)
- `MPN_FILL(dst, n, f)` (L1961/L1973)
- `MPN_ZERO(dst, n)` (L1984)
- `MPN_NORMALIZE(DST, NLIMBS)` (L2005): 去除高 0
- `MPN_NORMALIZE_NOT_ZERO(DST, NLIMBS)` (L2016)
- `MPN_STRIP_LOW_ZEROS_NOT_ZERO(ptr, size, low)` (L2032)
- `MPZ_TMP_INIT(X, NLIMBS)` (L2050)
- `MPZ_REALLOC(z,n)` (L2071)
- `MPZ_NEWALLOC(z,n)` (L2074)
- `MPZ_EQUAL_1_P(z)` (L2078)
- `MPN_FIB2_SIZE(n)` (L2101)

#### Overlap 检测宏 (L2450-L2476)
- `MPN_OVERLAP_P(xp, xsize, yp, ysize)` (L2453)
- `MEM_OVERLAP_P(xp, xsize, yp, ysize)` (L2455)
- `MPN_SAME_OR_SEPARATE_P(xp, yp, size)` (L2461)
- `MPN_SAME_OR_SEPARATE2_P` (L2463)
- `MPN_SAME_OR_INCR2_P`/`MPN_SAME_OR_INCR_P` (L2469/L2471)
- `MPN_SAME_OR_DECR2_P`/`MPN_SAME_OR_DECR_P` (L2473/L2475)

#### ASSERT 宏族 (L2479-L2614)
- `ASSERT_LINE` (L2485), `ASSERT_FILE` (L2491)
- `ASSERT_FAIL(expr)` (L2499)
- `ASSERT_ALWAYS(expr)` (L2500)
- `ASSERT(expr)` (L2507/L2509)
- `ASSERT_CARRY(expr)` (L2520/L2523)
- `ASSERT_NOCARRY(expr)` (L2521/L2524)
- `ASSERT_CODE(expr)` (L2531/L2533)
- `ASSERT_MPQ_CANONICAL(q)` (L2541)
- `ASSERT_ALWAYS_LIMB(limb)` (L2564)
- `ASSERT_ALWAYS_MPN(ptr, size)` (L2569)
- `ASSERT_LIMB(limb)` (L2580/L2583)
- `ASSERT_MPN(ptr, size)` (L2581/L2584)
- `ASSERT_MPN_ZERO_P(ptr,size)` (L2591)
- `ASSERT_MPN_NONZERO_P(ptr,size)` (L2598)

#### 逻辑位运算宏 (L2632-L2700)
- `MPN_LOGOPS_N_INLINE(rp, up, vp, n, operation)` (L2632)
- `mpn_com(d,s,n)` (L2619)
- `mpn_and_n`/`mpn_andn_n`/`mpn_nand_n`/`mpn_ior_n`/`mpn_iorn_n`/`mpn_nior_n`/`mpn_xor_n`/`mpn_xnor_n` (L2656-L2700)

#### 精确除法小常数宏 (L1711-L1764)
- `DIVEXACT_BY3_METHOD` (L1713)
- `mpn_divexact_by3(dst,src,size)` (L1721)
- `mpn_divexact_by5` (L1732)
- `mpn_divexact_by7` (L1737)
- `mpn_divexact_by9` (L1742)
- `mpn_divexact_by11` (L1747)
- `mpn_divexact_by13` (L1752)
- `mpn_divexact_by15` (L1757)
- `mpn_divexact_by17` (L1762)

#### 其他常量宏
- `MP_LIMB_T_MAX` (L568)
- `CNST_LIMB(C)` (L4003/L4005)
- `PP`/`PP_INVERTED`/`PP_FIRST_OMITTED` (L4009-L4037): 小素数乘积，perfsqr 用
- `MODLIMB_INVERSE_3` (L3401)
- `GMP_NUMB_CEIL_MAX_DIV3` (L3405)
- `GMP_NUMB_CEIL_2MAX_DIV3` (L3406)
- `MP_BASE_AS_DOUBLE` (L3911)
- `LIMBS_PER_DOUBLE` (L3914)
- `LIMBS_PER_ULONG` (L3058/L3070)
- `BITS_TO_LIMBS(n)` (L3051)
- `MPN_SET_UI(zp, zn, u)` (L3059/L3071)
- `MPZ_FAKE_UI(z, zp, u)` (L3062/L3075)
- `TARGET_REGISTER_STARVED` (L3086)
- `FFT_TABLE_ATTRS` (L2416)
- `GMP_DECIMAL_POINT` (L4495)
- `MOD_BKNP1_USE11` (L1280)
- `MOD_BKNP1_ONLY3` (L1283)
- `MPN_MULMOD_BKNP1_USABLE(rn, k, mn)` (L1292/L1296)
- `MPN_SQRMOD_BKNP1_USABLE(rn, k, mn)` (L1320/L1323)
- `MPN_EXTRACT_NUMB(count, xh, xl)` (L4284)
- `MPN_HGCD_MATRIX_INIT_ITCH(n)` (L4318)
- `MPN_GCDEXT_LEHMER_N_ITCH(n)` (L4383)
- `MPN_GCDEXT_LEHMER_ITCH(an, bn)` (L4389)
- `MPN_GCD_SUBDIV_STEP_ITCH(n)` (L4362)

### 1.12 进位/借位处理宏 (L2709-L2947, L3111-L3441)

#### 单 limb 进位/借位
- `ADDC_LIMB(cout, w, x, y)` (L2711, L2720): w=x+y, cout=carry
- `SUBC_LIMB(cout, w, x, y)` (L2734, L2743): w=x-y, cout=borrow

#### 多 limb 加减 1
- `MPN_IORD_U(ptr, incr, aors)` (L2773): x86 内联汇编版本
- `MPN_INCR_U(ptr, size, incr)` (L2808/L2812/L2927): {ptr,size} += incr
- `MPN_DECR_U(ptr, size, incr)` (L2809/L2813/L2939): {ptr,size} -= incr
- `mpn_incr_u(p, incr)` (L2815/L2821/L2864): 单指针版本
- `mpn_decr_u(p, incr)` (L2816/L2841/L2894): 单指针版本

#### Limb 逆元
- `mpn_invert_limb` (L3112): 库函数
- `invert_limb(invxl, xl)` (L3115, L3122): 单 limb 逆元宏
- `invert_pi1(dinv, d1, d0)` (L3130): 双 limb pi1 逆元
- `binvert_limb(inv, n)` (L3372): 模 2^GMP_NUMB_BITS 的乘法逆元
- `binvert_limb_table` (L3369): 8-bit 查表
- `modlimb_invert` = `binvert_limb` (L3395, 兼容性别名)
- `LIMB_HIGHBIT_TO_MASK(n)` (L3105): 高位→全 1/全 0 mask
- `NEG_MOD(r, a, d)` (L3415): r = -a mod d
- `LOW_ZEROS_MASK(n)` (L3440): 低位零的 mask

#### 预计算除法相关
- `udiv_qrnnd_preinv(q, r, nh, nl, d, di)` (L3180): 预逆元单 limb 除法
- `udiv_rnnd_preinv(r, nh, nl, d, di)` (L3212): 仅返回余数
- `udiv_qr_3by2(q, r1, r0, n2, n1, n0, d1, d0, dinv)` (L3242): 3-by-2 除法
- `MPN_DIVREM_OR_PREINV_DIVREM_1(...)` (L3283): divrem_1 / preinv_divrem_1 选择
- `MPN_MOD_OR_PREINV_MOD_1(...)` (L3296)
- `MPN_DIVREM_OR_DIVEXACT_1(...)` (L3320)
- `MPN_MOD_OR_MODEXACT_1_ODD(...)` (L3344)

#### 字节序交换
- `BSWAP_LIMB(dst, src)` (L3521/L3542/L3550/L3558/L3567/L3580/L3584/L3589/L3599/L3616)
- `BSWAP_LIMB_FETCH(limb, src)` (L3639/L3652)
- `BSWAP_LIMB_STORE(dst, limb)` (L3664/L3676)
- `MPN_BSWAP(dst, src, size)` (L3681)
- `MPN_BSWAP_REVERSE(dst, src, size)` (L3699)

#### 位计数
- `popc_limb(result, input)` (L3730/L3738/L3746/L3761/L3772/L3784/L3797)
- `ULONG_PARITY(p, n)` (L3448/L3458/L3468/L3486/L3500)

#### 双精度浮点
- `DOUBLE_NAN_INF_ACTION(x, a_nan, a_inf)` (L3927/L3943/L3951)
- `FORCE_DOUBLE(d)` (L3981/L3984)
- `ieee_double_extract` 联合体 (L3850/L3865/L3880)

#### Jacobi 符号宏 (L4058-L4192)
- `JACOBI_S0(a)`, `JACOBI_U0(a)`, `JACOBI_LS0`, `JACOBI_Z0`, `JACOBI_0U`, `JACOBI_0S`, `JACOBI_0LS`
- `JACOBI_BIT1_TO_PN` (L4087)
- `JACOBI_TWO_U_BIT1(b)` (L4093)
- `JACOBI_TWOS_U_BIT1(twos, b)` (L4097), `JACOBI_TWOS_U` (L4101)
- `JACOBI_N1B_BIT1(b)` (L4106)
- `JACOBI_ASGN_SU_BIT1(a, b)` (L4111)
- `JACOBI_BSGN_SS_BIT1`/`JACOBI_BSGN_SZ_BIT1`/`JACOBI_BSGN_ZS_BIT1` (L4116/L4121/L4126)
- `JACOBI_RECIP_UU_BIT1(a, b)` (L4134)
- `JACOBI_STRIP_LOW_ZEROS(...)` (L4141)
- `JACOBI_MOD_OR_MODEXACT_1_ODD(...)` (L4173)

---

## 2. 函数原型分类

### 2.1 乘法相关 (L806-L1568, L1700-L1703)

#### 基础乘加（_basecase 系列）
- `mpn_mul_basecase(mp_ptr, mp_srcptr, mp_size_t, mp_srcptr, mp_size_t)` — L1179
- `mpn_mullo_n(mp_ptr, mp_srcptr, mp_srcptr, mp_size_t)` — L1183
- `mpn_mullo_basecase(mp_ptr, mp_srcptr, mp_srcptr, mp_size_t)` — L1187
- `mpn_sqr_basecase(mp_ptr, mp_srcptr, mp_size_t)` — L1192
- `mpn_sqrlo(mp_ptr, mp_srcptr, mp_size_t)` — L1196
- `mpn_sqrlo_basecase(mp_ptr, mp_srcptr, mp_size_t)` — L1199
- `mpn_mulmid_basecase(mp_ptr, mp_srcptr, mp_size_t, mp_srcptr, mp_size_t)` — L1202
- `mpn_mulmid_n(mp_ptr, mp_srcptr, mp_srcptr, mp_size_t)` — L1205
- `mpn_mulmid(mp_ptr, mp_srcptr, mp_size_t, mp_srcptr, mp_size_t)` — L1208
- `mpn_sqr_diagonal(mp_ptr, mp_srcptr, mp_size_t)` — L1457
- `mpn_sqr_diag_addlsh1(mp_ptr, mp_srcptr, mp_srcptr, mp_size_t)` — L1460

#### 倍长乘加 (mpn_mul_k / mpn_addmul_k)
- `mpn_mul_1c(mp_ptr, mp_srcptr, mp_size_t, mp_limb_t, mp_limb_t)` — L1160
- `mpn_mul_2`/`mul_3`/`mul_4`/`mul_5`/`mul_6` — L1163-L1176
- `mpn_addmul_1c(mp_ptr, mp_srcptr, mp_size_t, mp_limb_t, mp_limb_t)` — L814
- `mpn_addmul_2` ~ `mpn_addmul_8` — L817-L837
- `mpn_addmul_2s(mp_ptr, mp_srcptr, mp_size_t, mp_srcptr)` — L841
- `mpn_submul_1c(mp_ptr, mp_srcptr, mp_size_t, mp_limb_t, mp_limb_t)` — L1211

#### Toom-Cook 系列
- `mpn_toom22_mul(mp_ptr, mp_srcptr, mp_size_t, mp_srcptr, mp_size_t, mp_ptr)` — L1503
- `mpn_toom32_mul` — L1506
- `mpn_toom42_mul` — L1509
- `mpn_toom52_mul` — L1512
- `mpn_toom62_mul` — L1515
- `mpn_toom33_mul` — L1521
- `mpn_toom43_mul` — L1524
- `mpn_toom53_mul` — L1527
- `mpn_toom54_mul` — L1530
- `mpn_toom63_mul` — L1533
- `mpn_toom44_mul` — L1539
- `mpn_toom6h_mul` — L1545
- `mpn_toom8h_mul` — L1551
- `mpn_toom2_sqr` — L1518
- `mpn_toom3_sqr` — L1536
- `mpn_toom4_sqr` — L1542
- `mpn_toom6_sqr` — L1548
- `mpn_toom8_sqr` — L1554
- `mpn_toom42_mulmid(mp_ptr, mp_srcptr, mp_srcptr, mp_size_t, mp_ptr)` — L1557

#### Toom 插值与求值
- `mpn_toom_interpolate_5pts(...)` — L1463
- `mpn_toom_interpolate_6pts(...)` — L1467
- `mpn_toom_interpolate_7pts(...)` — L1471
- `mpn_toom_interpolate_8pts(...)` — L1474
- `mpn_toom_interpolate_12pts(...)` — L1477
- `mpn_toom_interpolate_16pts(...)` — L1480
- `mpn_toom_couple_handling(...)` — L1483
- `mpn_toom_eval_dgr3_pm1` — L1486
- `mpn_toom_eval_dgr3_pm2` — L1489
- `mpn_toom_eval_pm1` — L1492
- `mpn_toom_eval_pm2` — L1495
- `mpn_toom_eval_pm2exp` — L1498
- `mpn_toom_eval_pm2rexp` — L1500

#### FFT / Nussbaumer
- `mpn_fft_best_k(mp_size_t, int)` — L1560
- `mpn_mul_fft(mp_ptr, mp_size_t, mp_srcptr, mp_size_t, mp_srcptr, mp_size_t, int)` — L1563
- `mpn_mul_fft_full(mp_ptr, mp_srcptr, mp_size_t, mp_srcptr, mp_size_t)` — L1566
- `mpn_nussbaumer_mul(mp_ptr, mp_srcptr, mp_size_t, mp_srcptr, mp_size_t)` — L1569
- `mpn_fft_next_size(mp_size_t, int)` — L1572

#### 模乘（B^n-1 / B^k*n+1）
- `mpn_bc_mulmod_bnm1(mp_ptr, mp_srcptr, mp_srcptr, mp_size_t, mp_ptr)` — L1264
- `mpn_mulmod_bnm1(mp_ptr, mp_size_t, mp_srcptr, mp_size_t, mp_srcptr, mp_size_t, mp_ptr)` — L1266
- `mpn_mulmod_bnm1_next_size(mp_size_t)` — L1268
- `mpn_mulmod_bnm1_itch(mp_size_t, mp_size_t, mp_size_t)` — L1270 (inline)
- `mpn_mulmod_bknp1(mp_ptr, mp_srcptr, mp_srcptr, mp_size_t, unsigned, mp_ptr)` — L1285
- `mpn_mulmod_bknp1_itch(mp_size_t)` — L1287 (inline)
- `mpn_sqrmod_bknp1(mp_ptr, mp_srcptr, mp_size_t, unsigned, mp_ptr)` — L1313
- `mpn_sqrmod_bknp1_itch(mp_size_t)` — L1315 (inline)
- `mpn_sqrmod_bnm1(mp_ptr, mp_size_t, mp_srcptr, mp_size_t, mp_ptr)` — L1341
- `mpn_sqrmod_bnm1_next_size(mp_size_t)` — L1343
- `mpn_sqrmod_bnm1_itch(mp_size_t, mp_size_t)` — L1345 (inline)

#### Montgomery 约简
- `mpn_redc_1(mp_ptr, mp_ptr, mp_srcptr, mp_size_t, mp_limb_t)` — L1215
- `mpn_redc_2(mp_ptr, mp_ptr, mp_srcptr, mp_size_t, mp_srcptr)` — L1219
- `mpn_redc_n(mp_ptr, mp_ptr, mp_srcptr, mp_size_t, mp_srcptr)` — L1224

#### 幂
- `mpn_powm(mp_ptr, mp_srcptr, mp_size_t, mp_srcptr, mp_size_t, mp_srcptr, mp_size_t, mp_ptr)` — L1700
- `mpn_powlo(mp_ptr, mp_srcptr, mp_srcptr, mp_size_t, mp_size_t, mp_ptr)` — L1702

### 2.2 除法相关 (L1575-L1708, L3269-L3347)

#### 单 limb 除法
- `mpn_divrem_1c(mp_ptr, mp_size_t, mp_srcptr, mp_size_t, mp_limb_t, mp_limb_t)` — L1129
- `mpn_preinv_divrem_1(mp_ptr, mp_size_t, mp_srcptr, mp_size_t, mp_limb_t, mp_limb_t, int)` — L3270
- `mpn_div_qr_1n_pi1(...)` — L1575
- `mpn_div_qr_2n_pi1(...)` — L1578
- `mpn_div_qr_2u_pi1(...)` — L1581

#### Schoolbook PI1 除法
- `mpn_sbpi1_div_qr(mp_ptr, mp_ptr, mp_size_t, mp_srcptr, mp_size_t, mp_limb_t)` — L1584
- `mpn_sbpi1_div_q(...)` — L1587
- `mpn_sbpi1_divappr_q(...)` — L1590

#### Divide-and-Conquer PI1 除法
- `mpn_dcpi1_div_qr(mp_ptr, mp_ptr, mp_size_t, mp_srcptr, mp_size_t, gmp_pi1_t *)` — L1593
- `mpn_dcpi1_div_qr_n(mp_ptr, mp_ptr, mp_srcptr, mp_size_t, gmp_pi1_t *, mp_ptr)` — L1595
- `mpn_dcpi1_div_q(...)` — L1598
- `mpn_dcpi1_divappr_q(...)` — L1601

#### Møller-Knuth (MU) 除法
- `mpn_mu_div_qr(mp_ptr, mp_ptr, mp_srcptr, mp_size_t, mp_srcptr, mp_size_t, mp_ptr)` — L1604
- `mpn_mu_div_qr_itch(mp_size_t, mp_size_t, int)` — L1606
- `mpn_preinv_mu_div_qr(...)` — L1609
- `mpn_preinv_mu_div_qr_itch(...)` — L1611
- `mpn_mu_divappr_q(...)` — L1614
- `mpn_mu_divappr_q_itch(...)` — L1616
- `mpn_mu_div_q(...)` — L1619
- `mpn_mu_div_q_itch(...)` — L1621

#### 顶层除法
- `mpn_div_q(mp_ptr, mp_srcptr, mp_size_t, mp_srcptr, mp_size_t, mp_ptr)` — L1624

#### 逆元
- `mpn_invert(mp_ptr, mp_srcptr, mp_size_t, mp_ptr)` — L1627
- `mpn_invert_itch(n)` = `mpn_invertappr_itch(n)` — L1629
- `mpn_ni_invertappr(mp_ptr, mp_srcptr, mp_size_t, mp_ptr)` — L1631
- `mpn_invertappr(mp_ptr, mp_srcptr, mp_size_t, mp_ptr)` — L1633
- `mpn_invertappr_itch(n)` = `2 * n` — L1635
- `mpn_binvert(mp_ptr, mp_srcptr, mp_size_t, mp_ptr)` — L1637
- `mpn_binvert_itch(mp_size_t)` — L1639
- `mpn_invert_limb(mp_limb_t)` — L3112

#### Bdiv 系列
- `mpn_bdiv_q_1(mp_ptr, mp_srcptr, mp_size_t, mp_limb_t)` — L1642
- `mpn_pi1_bdiv_q_1(...)` — L1645
- `mpn_sbpi1_bdiv_qr(...)` — L1648
- `mpn_sbpi1_bdiv_q(...)` — L1651
- `mpn_sbpi1_bdiv_r(...)` — L1654
- `mpn_dcpi1_bdiv_qr(...)` — L1657
- `mpn_dcpi1_bdiv_qr_n_itch(mp_size_t)` — L1659
- `mpn_dcpi1_bdiv_qr_n(...)` — L1662
- `mpn_dcpi1_bdiv_q(...)` — L1664
- `mpn_mu_bdiv_qr(...)` — L1667
- `mpn_mu_bdiv_qr_itch(...)` — L1669
- `mpn_mu_bdiv_q(...)` — L1672
- `mpn_mu_bdiv_q_itch(...)` — L1674
- `mpn_bdiv_qr(...)` — L1677
- `mpn_bdiv_qr_itch(...)` — L1679
- `mpn_bdiv_q(...)` — L1682
- `mpn_bdiv_q_itch(...)` — L1684
- `mpn_bdiv_dbm1c(mp_ptr, mp_srcptr, mp_size_t, mp_limb_t, mp_limb_t)` — L1693
- `mpn_bdiv_dbm1(dst, src, size, divisor)` — L1697 (宏)

#### 精确除法 / mod_1
- `mpn_divexact(mp_ptr, mp_srcptr, mp_size_t, mp_srcptr, mp_size_t)` — L1687
- `mpn_divexact_itch(mp_size_t, mp_size_t)` — L1689
- `mpn_mod_1c(mp_srcptr, mp_size_t, mp_limb_t, mp_limb_t)` — L1157
- `mpn_mod_1_1p_cps(mp_limb_t [4], mp_limb_t)` — L1229
- `mpn_mod_1_1p(...)` — L1232
- `mpn_mod_1s_2p_cps` / `mpn_mod_1s_2p` — L1237/L1241
- `mpn_mod_1s_3p_cps` / `mpn_mod_1s_3p` — L1246/L1250
- `mpn_mod_1s_4p_cps` / `mpn_mod_1s_4p` — L1255/L1259
- `mpn_mod_34lsub1(mp_srcptr, mp_size_t)` — L3303
- `mpn_modexact_1c_odd(...)` — L3332
- `mpn_modexact_1_odd(...)` — L3337

#### Side-channel 安全除法
- `mpn_sec_pi1_div_qr(...)` — L1705
- `mpn_sec_pi1_div_r(...)` — L1707

#### 根/幂运算
- `mpn_rootrem(...)` — L1789
- `mpn_broot(...)` — L1792
- `mpn_broot_invm1(...)` — L1795
- `mpn_brootinv(...)` — L1798
- `mpn_bsqrt(...)` — L1801
- `mpn_bsqrtinv(...)` — L1804
- `mpn_divisible_p(...)` — L1786

### 2.3 加减移位 / 逻辑运算 (L970-L1135, L2617-L2700)

#### addlsh / sublsh / rsblsh 系列（带移位的加减）
- `mpn_addlsh1_n`/`_nc`/`_n_ip1`/`_nc_ip1` (L970-L985)
- `mpn_addlsh2_n`/`_nc`/`_n_ip1`/`_nc_ip1` (L987-L1002)
- `mpn_addlsh_n`/`_nc`/`_n_ip1`/`_nc_ip1` (L1004-L1019)
- `mpn_sublsh1_n`/`_nc`/`_n_ip1`/`_nc_ip1` (L1021-L1036)
- `mpn_sublsh2_n`/`_nc`/`_n_ip1`/`_nc_ip1` (L1038-L1053)
- `mpn_sublsh_n`/`_nc`/`_n_ip1`/`_nc_ip1` (L1055-L1070)
- `mpn_rsblsh1_n`/`_nc` (L1072-L1075)
- `mpn_rsblsh2_n`/`_nc` (L1077-L1080)
- `mpn_rsblsh_n`/`_nc` (L1082-L1085)
- `mpn_rsh1add_n`/`_nc` (L1087-L1090)
- `mpn_rsh1sub_n`/`_nc` (L1092-L1095)
- `mpn_lshiftc(mp_ptr, mp_srcptr, mp_size_t, unsigned int)` — L1098

#### 误差累积
- `mpn_add_err1_n`/`add_err2_n`/`add_err3_n` (L1102-L1109)
- `mpn_sub_err1_n`/`sub_err2_n`/`sub_err3_n` (L1111-L1118)

#### 混合运算
- `mpn_add_n_sub_n(mp_ptr, mp_ptr, mp_srcptr, mp_srcptr, mp_size_t)` — L1120
- `mpn_add_n_sub_nc(...)` — L1123
- `mpn_addaddmul_1msb0(...)` — L1126

#### 逻辑运算（无 native 时使用宏）
- `mpn_com(d,s,n)` (L2619)
- `mpn_and_n`/`mpn_andn_n`/`mpn_nand_n` (L2656-L2670)
- `mpn_ior_n`/`mpn_iorn_n`/`mpn_nior_n` (L2674-L2688)
- `mpn_xor_n`/`mpn_xnor_n` (L2692-L2700)

#### 拷贝
- `mpn_copyi(mp_ptr, mp_srcptr, mp_size_t)` — L1819
- `mpn_copyd(mp_ptr, mp_srcptr, mp_size_t)` — L1872

### 2.4 GCD / HGCD / Jacobi (L1148-L1155, L4044-L4388)

- `mpn_jacobi_base(mp_limb_t, mp_limb_t, int)` — L1148
- `mpn_jacobi_2(mp_srcptr, mp_srcptr, unsigned)` — L1151
- `mpn_jacobi_n(mp_ptr, mp_ptr, mp_size_t, unsigned)` — L1154
- `mpn_gcd_22(mp_limb_t, mp_limb_t, mp_limb_t, mp_limb_t)` — L4044 (返回 mp_double_limb_t)
- `mpn_hgcd2(...)` — L4299
- `mpn_hgcd_mul_matrix1_vector(...)` — L4302
- `mpn_matrix22_mul1_inverse_vector(...)` — L4305
- `mpn_hgcd2_jacobi(...)` — L4308
- `mpn_hgcd_matrix_init(...)` — L4320
- `mpn_hgcd_matrix_update_q(...)` — L4323
- `mpn_hgcd_matrix_mul_1(...)` — L4326
- `mpn_hgcd_matrix_mul(...)` — L4329
- `mpn_hgcd_matrix_adjust(...)` — L4332
- `mpn_hgcd_step(...)` — L4335
- `mpn_hgcd_reduce(...)` — L4338
- `mpn_hgcd_reduce_itch(...)` — L4341
- `mpn_hgcd_itch(mp_size_t)` — L4344
- `mpn_hgcd(...)` — L4347
- `mpn_hgcd_appr_itch(mp_size_t)` — L4350
- `mpn_hgcd_appr(...)` — L4353
- `mpn_hgcd_jacobi(...)` — L4356
- `mpn_gcd_subdiv_step(...)` — L4364 (类型 `gcd_subdiv_step_hook` 在 L4359)
- `mpn_gcdext_hook` — L4380
- `mpn_gcdext_lehmer_n(...)` — L4385
- `mpn_matrix22_mul(...)` — L4259
- `mpn_matrix22_mul_itch(...)` — L4261
- `mpn_trialdiv(mp_srcptr, mp_size_t, mp_size_t, int *)` — L2702
- `mpn_remove(...)` — L2705

### 2.5 数论 / Fibonacci / Lucas / Broot (L1135-L1142, L1766-L1806)

- `mpn_fib2_ui(mp_ptr, mp_ptr, unsigned long)` — L1135
- `mpn_fib2m(...)` — L1138
- `mpn_strongfibo(...)` — L1141
- `mpz_divexact_gcd(...)` — L1766
- `mpz_prodlimbs(...)` — L1769
- `mpz_oddfac_1(...)` — L1772
- `mpz_stronglucas(...)` — L1775
- `mpz_lucas_mod(...)` — L1778
- `mpz_inp_str_nowhite(...)` — L1781

### 2.6 字符串/数字转换 (L4421-L4430)

- `mpn_str_powtab_alloc(n)` (L4421, 宏)
- `mpn_dc_set_str_itch(n)` (L4422, 宏)
- `mpn_dc_get_str_itch(n)` (L4423, 宏)
- `mpn_compute_powtab(...)` — L4425
- `mpn_dc_set_str(...)` — L4427
- `mpn_bc_set_str(...)` — L4429
- `mpn_get_d(...)` — L3918
- `mpn_dump(...)` — L1132

### 2.7 随机数 (L1354-L1409)

- `gmp_randfnptr_t` 类型 (L1355)
- `RNG_FNPTR(rstate)` (L1363)
- `RNG_STATE(rstate)` (L1367)
- `_gmp_rand(rp, state, bits)` (L1370)
- `__gmp_randinit_mt_noseed(...)` — L1377
- `__gmp_rands_initialized` (L1392), `__gmp_rands` (L1393)
- `RANDS` (L1395), `RANDS_CLEAR()` (L1402)

### 2.8 mpz 内部辅助 (L806-L811, L1766-L1784)

- `mpz_aorsmul_1(...)` (L806, regparm 优化)
- `mpz_n_pow_ui(...)` — L810

### 2.9 素数筛 / 其他 (L2128-L2144, L2702-L2706)

- `gmp_primesieve_t` 结构 (L2129)
- `SIEVESIZE` = 512 (L2128)
- `gmp_init_primesieve(...)` — L2137
- `gmp_nextprime(...)` — L2140
- `gmp_primesieve(...)` — L2143
- `binvert_limb_table[128]` (L3370, extern)
- `jacobi_table[208]` (L4196, extern)

### 2.10 异常 / 错误处理 (L3988-L4001)

- `__gmp_junk` (L3990), `__gmp_0` (L3991)
- `__gmp_exception(int)` — L3992
- `__gmp_divide_by_zero(void)` — L3993
- `__gmp_sqrt_of_negative(void)` — L3994
- `__gmp_overflow_in_mpz(void)` — L3995
- `__gmp_invalid_operation(void)` — L3996
- `GMP_ERROR(code)` (L3997)
- `DIVIDE_BY_ZERO` (L3998)
- `SQRT_OF_NEGATIVE` (L3999)
- `MPZ_OVERFLOW` (L4000)

---

## 3. 重要类型定义

### 3.1 来自 gmp.h（在 gmp-impl.h 中通过 `#include "gmp.h"` 引入）
- `mp_limb_t`: 单 limb 类型，无符号整数，通常是 `unsigned long` 或 `unsigned long long`。`SIZEOF_MP_LIMB_T` 决定其字节数（4 或 8）。
- `mp_ptr`: `mp_limb_t *`，可写指针。
- `mp_srcptr`: `const mp_limb_t *`，只读指针。
- `mp_size_t`: limb 计数的有符号整数类型，通常是 `long` 或 `int`（由 `__GMP_MP_SIZE_T_INT` 决定）。
- `mp_limb_signed_t`: `mp_limb_t` 的有符号版本。
- `mp_bitcnt_t`: 位计数无符号类型。

### 3.2 在 gmp-impl.h 中定义的类型

| 类型 | 行号 | 说明 |
|---|---|---|
| `gmp_uint_least32_t` | L235-L246 | 至少 32 位无符号整数 |
| `gmp_intptr_t` | L251/L253 | 指针↔整数转换 |
| `gmp_pi1_t` | L258 | 单 limb 预逆元 `{mp_limb_t inv32}` |
| `gmp_pi2_t` | L259 | 双 limb 预逆元 `{mp_limb_t inv21, inv32, inv53}` |
| `mp_double_limb_t` | L4039-L4042 | 双 limb 结构 `{d0, d1}` |
| `union tmp_align_t` | L347-L351 | TMP_ALLOC 对齐联合 |
| `struct tmp_reentrant_t` | L365-L368 | 可重入临时分配头 |
| `struct tmp_marker` | L404-L408 | 非重入临时标记 |
| `struct tmp_debug_t` / `tmp_debug_entry_t` | L426-L435 | 调试临时分配 |
| `gmp_randfnptr_t` | L1355-L1360 | RNG 函数指针表 |
| `struct bases` | L2951-L2973 | 进制转换信息 |
| `struct hgcd_matrix1` | L4294-L4297 | 1-limb HGCD 矩阵 |
| `struct hgcd_matrix` | L4311-L4316 | 多 limb HGCD 矩阵 |
| `struct powers` / `powers_t` | L4412-L4420 | 幂表（字符串转换用） |
| `struct fft_table_nk` | L2409-L2413 | FFT 表项 (n:27, k:5) |
| `union ieee_double_extract` | L3850/L3865/L3880 | IEEE 754 双精度分解 |
| `gmp_primesieve_t` | L2129-L2135 | 素数筛 |
| `struct gcdext_ctx` | L4367-L4378 | GCDext 上下文 |
| `gcd_subdiv_step_hook` | L4359 | GCD 子分步回调 |
| `enum toom6_flags` | L1466 | Toom6 符号标志 |
| `enum toom7_flags` | L1470 | Toom7 符号标志 |

### 3.3 longlong.h 类型 (L3813-L3834)
- `UQItype`, `SItype`, `USItype`, `DItype`, `UDItype` (L3814-L3828)
- `UWtype` = `mp_limb_t` (L3832)
- `UHWtype` = `unsigned int` (L3833)
- `W_TYPE_SIZE` = `GMP_LIMB_BITS` (L3834)

---

## 4. 与 moptm_fusion.cpp 的对应关系

### 4.1 阈值对照表

| GMP 阈值 (gmp-impl.h) | 默认值 | moptm 对应 | moptm 值 | 行号 (moptm_fusion.cpp) | 备注 |
|---|---|---|---|---|---|
| `MUL_TOOM22_THRESHOLD` | 30 | — | — | — | moptm 未实现 Toom22/Karatsuba |
| `MUL_TOOM33_THRESHOLD` | 100 | — | — | — | moptm 未实现 Toom33 |
| `MUL_TOOM44_THRESHOLD` | 300 | — | — | — | moptm 未实现 Toom44 |
| `MUL_TOOM6H/8H_THRESHOLD` | 350/450 | — | — | — | moptm 未实现高级 Toom |
| `MUL_FFT_THRESHOLD` | ~3000 (= 10 × 3 × MUL_TOOM33_THRESHOLD) | `FFT_MUL_THRESHOLD` | 64 | L2097 | moptm 直接从 basecase 跳到 FFT，跳过所有 Toom |
| `SQR_FFT_THRESHOLD` | ~3600 | `FFT_SQR_THRESHOLD` | 64 | L2094 | 同上 |
| `INV_NEWTON_THRESHOLD` | 200 | `INV_NEWTON_BASE_THRESHOLD` | 64 | L2596 | moptm 的 Newton 逆元阈值更低 |
| `INV_APPR_THRESHOLD` | =INV_NEWTON_THRESHOLD | — | — | — | moptm 未单独区分 |
| `BINV_NEWTON_THRESHOLD` | 300 | — | — | — | moptm 未实现 binvert |
| `MULMOD_BNM1_THRESHOLD` | 16 | — | — | — | moptm 直接用 FFT 实现 mod B^m-1 |
| `MU_DIV_QR_THRESHOLD` | 2000 | — | — | — | moptm 用自己的块状除法（基于 fftMulModBm1） |
| `MU_DIVAPPR_Q_THRESHOLD` | 2000 | — | — | — | 同上 |
| `DC_DIV_QR_THRESHOLD` | ~60-90 | — | — | — | moptm 未实现 DC 除法 |

### 4.2 函数对应关系

| GMP 函数 (gmp-impl.h) | 行号 | moptm 对应 | 行号 (moptm_fusion.cpp) | 说明 |
|---|---|---|---|---|
| `mpn_mul_fft` / `mpn_mul_fft_full` | L1563/L1566 | `fftMul` | L1958 | FFT 乘法主体 |
| `mpn_mul_fft` (平方路径) | — | `fftSqr` | L2028 | FFT 平方 |
| `mpn_mulmod_bnm1` | L1266 | `fftMulModBm1` | L2291 | mod B^m-1 乘法（moptm 用 FFT 实现） |
| `mpn_bc_mulmod_bnm1` | L1264 | `fftMulModBm1Pre` | L2406 | 预计算 DFT 版本 |
| `mpn_sqrmod_bnm1` | L1341 | — | — | moptm 未单独提供 |
| `mpn_mulmod_bknp1` | L1285 | — | — | moptm 未实现 B^k*n+1 路径 |
| `mpn_invert` / `mpn_invertappr` | L1627/L1633 | `invNewton`/invert 流程 | L2604/L2692 | Newton 迭代逆元（moptm 用 fftMulModBm1） |
| `mpn_ni_invertappr` | L1631 | (基础逆元子程序) | L2604-L2692 | moptm 实现的 basecase 逆元 |
| `mpn_mu_divappr_q` / `mpn_mu_div_qr` | L1614/L1604 | `muDivBlockQR` (块状除法) | L3268-L3355 | moptm 用块状 qhat + fftMulModBm1Pre |
| `mpn_toom22_mul` ~ `mpn_toom8h_mul` | L1503-L1555 | — | — | moptm 完全跳过 Toom 系列 |
| `mpn_toom_couple_handling` / `mpn_toom_interpolate_*pts` | L1463-L1481 | — | — | 同上 |
| `mpn_toom_eval_*` | L1486-L1501 | — | — | 同上 |
| `mpn_mullo_n` / `mpn_mulmid` | L1183/L1208 | — | — | moptm 未实现 mullo/mulmid |
| `mpn_redc_1` / `mpn_redc_2` / `mpn_redc_n` | L1215/L1219/L1224 | — | — | moptm 未实现 Montgomery redc |
| `mpn_hgcd*` / `mpn_gcd_22` | L4299-L4348/L4044 | — | — | moptm 未实现 HGCD |
| `mpn_jacobi_base` / `mpn_jacobi_n` | L1148/L1154 | — | — | moptm 未实现 Jacobi |
| `mpn_get_d` | L3918 | — | — | moptm 用自己的 I/O |
| `mpn_bc_set_str` / `mpn_dc_set_str` | L4429/L4427 | `ParseTable` / `itostr4` | L1032/L1082 | moptm 用查表 4 位分组 |

### 4.3 宏对应关系

| GMP 宏 | moptm 对应 | 说明 |
|---|---|---|
| `ABOVE_THRESHOLD` / `BELOW_THRESHOLD` | 直接 `if (size <= FFT_MUL_THRESHOLD)` 等 | moptm 简化判断，无 MP_SIZE_T_MAX 特殊值 |
| `MPN_COPY` / `MPN_ZERO` | 直接 `std::copy` / `std::fill` 或循环 | moptm 用 C++ 标准库 |
| `ADDC_LIMB` / `SUBC_LIMB` | `__builtin_add_overflow` / `__builtin_sub_overflow` | moptm 用 GCC 内建 |
| `umul_ppmm` (longlong.h) | `__int128` 或 `Float2` 模拟 | moptm 用 128 位类型 |
| `add_ssaaaa` / `sub_ddmmss` | 同上 | moptm 用内建 |
| `MPN_NORMALIZE` | `count_true_length` (L1173, moptm_fusion.cpp) | moptm 用 constexpr 模板 |
| `invert_limb` | `mpn_invert_limb` 直接调用（如有） | moptm 在 Newton 逆元中用到 |
| `binvert_limb` | — | moptm 未使用 binvert |
| `ASSERT` / `ASSERT_ALWAYS` | `assert` | moptm 用标准 assert |

### 4.4 关键差异总结

1. **算法调度策略**:
   - GMP: basecase → Toom22/33/44/6H/8H → FFT (mul_fft_full / nussbaumer_mul)
   - moptm: basecase → FFT 直接跳转（阈值仅 64），完全跳过 Toom 系列

2. **逆元实现**:
   - GMP: `mpn_invertappr` + `INV_NEWTON_THRESHOLD=200` 触发 Newton
   - moptm: `INV_NEWTON_BASE_THRESHOLD=64` 触发 Newton basecase，调用 `fftMulModBm1`

3. **模乘路径**:
   - GMP: 同时支持 `mulmod_bnm1` (B^m-1) 和 `mulmod_bknp1` (B^k*n+1)
   - moptm: 仅实现 `fftMulModBm1` (B^m-1)

4. **除法实现**:
   - GMP: 三档调度 — SB (schoolbook) → DC (divide-and-conquer) → MU (Möller-Knuth)
   - moptm: 直接用块状除法 + `fftMulModBm1Pre`（预计算 DFT），无 SB/DC 分层

5. **未实现部分**: moptm 未实现 Montgomery redc、HGCD、Jacobi、Toom、mulmid、mullo 等接口

6. **临时内存管理**:
   - GMP: `TMP_ALLOC` 宏族（alloca/reentrant/debug 多变体）
   - moptm: `std::vector` 或 `Span` 视图，无 TMP_ALLOC 抽象

---

## 附录: 函数总数统计

| 类别 | 数量 |
|---|---|
| 乘法相关函数原型 | ~85 |
| 除法相关函数原型 | ~55 |
| 加减移位/逻辑运算 | ~60 |
| GCD/HGCD/Jacobi | ~30 |
| 数论/Fibonacci/Lucas/Broot | ~15 |
| 字符串转换 | ~6 |
| 随机数 | ~5 |
| 阈值宏 | ~60 |
| 其他辅助宏 | ~150 |
| 类型定义 | ~25 |
| **函数原型总计** | **~280** |

---

文件路径: `d:\precious_speed\gmp_index\06_gmp_impl_h.md`
源文件: `E:\gmp-6.3.0\gmp-impl.h` (4485 行)
