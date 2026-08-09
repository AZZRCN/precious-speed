# GMP 6.3.0 源码索引 TREE

> 用途：为 moptm_fusion.cpp 的 GMP cyclic Newton bug 修复提供精准行号参考。
> 索引按功能模块分文件存放于本目录，本文件为入口索引。

## 索引结构

| 编号 | 文件 | 主题 | 关键函数数 | 重点行号 |
|------|------|------|------------|----------|
| 01 | [01_invert.md](file:///d:/precious_speed/gmp_index/01_invert.md) | Newton 逆近似 | 4 | `mpn_invertappr` L287-L300, `mpn_ni_invertappr` L152-L285 |
| 02 | [02_mu_div.md](file:///d:/precious_speed/gmp_index/02_mu_div.md) | mu-based 除法 | 11 | `mpn_preinv_mu_divappr_q` L161-L309 |
| 03 | [03_mulmid_cyclic.md](file:///d:/precious_speed/gmp_index/03_mulmid_cyclic.md) | Middle product + cyclic | 17 | `mpn_mulmod_bnm1` L194-L373 |
| 04 | [04_div_misc.md](file:///d:/precious_speed/gmp_index/04_div_misc.md) | 通用除法 | 7 | `mpn_div_q` 调度, `mpn_tdiv_qr` |
| 05 | [05_mul.md](file:///d:/precious_speed/gmp_index/05_mul.md) | 乘法 + FFT | 24 | `mpn_mul_fft` (Schoenhage) |
| 06 | [06_gmp_impl_h.md](file:///d:/precious_speed/gmp_index/06_gmp_impl_h.md) | gmp-impl.h | ~280 原型 | 阈值宏 + 函数原型 |

## 当前 bug 修复关键参考点（P0）

### bug 现象
- moptm_fusion.cpp 的 `absInvNewtonGMP` + cyclic convolution 路径在 4/200 fuzz case 失败
- FAIL #37: a=50000, b=10000 → quotient 长度 57991 vs 正确 50003（多 ~8000 位，严重错误）
- underflow 检测和 post-verification 修复都失败 → bug 在更深层次

### GMP 对照核心：`mpn_preinv_mu_divappr_q` (mu_divappr_q.c L161-L309)

**GMP 修正 1 关键代码（L235-L264）**：
```c
r = rp[dn - in] - tp[dn];        // L235: 关键 underflow 探测
// ... 步骤 D: mpn_sub_n 得到 cy
r -= cy;                          // L254: 累加 borrow
while (r != 0) {                  // L255
  mpn_incr_u (qp, 1);             // L260: qblock += 1
  cy = mpn_sub_n (rp, rp, dp, dn); // L261: rp -= D
  r -= cy;                        // L262: r -= borrow (无符号下溢继续循环)
}
```

**GMP 修正 2（L265-L271）**：
```c
if (mpn_cmp (rp, dp, dn) >= 0) {  // L265
  mpn_incr_u (qp, 1);             // L268
  mpn_sub_n (rp, rp, dp, dn);     // L269
}
```

**关键洞察**：
1. GMP 修正 1 用**单 limb** `r = rp[dn-in] - tp[dn]`，不是窗口级 borrow
2. `r -= cy` 用**无符号语义**驱动循环（下溢到 B^64-1 后继续，真归零才退出）
3. 修正 1 处理"商偏大 1 或 2"，修正 2 处理"商偏小 1"
4. **没有末尾 +3 饱和**（仅 divappr 版本有，div_qr 版本无）

### moptm 当前实现 vs GMP 关键差异

| 维度 | GMP | moptm 当前 | bug 风险 |
|------|-----|-----------|---------|
| underflow 探测 | 单 limb `r = rp[dn-in] - tp[dn]` | 窗口级 `absSub` 返回 borrow | **高**：moptm 检测的是窗口级 underflow，但 GMP 是基于"高位 limb 差 + borrow 累加" |
| 修正 1 循环条件 | `while (r != 0)`，靠 `r -= cy` 无符号下溢驱动 | `while (underflow)` + 内部 carry 检测 | **高**：moptm 逻辑可能错过 r 不归零但 borrow 归零的情况 |
| 修正 1 终止 | r 真归零（无符号语义） | carry=1 且 high this_in 位全 0 | **高**：moptm 可能错误终止或永远不终止 |
| 修正 2 触发 | `if (rp >= dp)` 一次 | `while (rp >= dp)` 循环 | 中：moptm 多次循环可能引入额外错误 |
| 乘法内核 | `mpn_mul_n` + `mpn_mulmod_bnm1` | `fftMulPre` + `fftMulModBm1Pre` | 中：moptm 的 cyclic 重建逻辑可能有问题 |

### 推荐修复策略

**策略 A（最直接，推荐）**：完全照搬 GMP `mpn_preinv_mu_divappr_q` 修正逻辑
- 在 moptm 中实现单 limb `r = window[dn-in] - prod[dn]` 探测
- 用 `r -= borrow` 无符号语义驱动 while 循环
- 修正 2 改为 `if` 单次判断（不循环）

**策略 B**：修复当前 underflow 检测逻辑
- 保留窗口级 absSub borrow 检测
- 但修正 1 循环改用 GMP 的 `r -= cy` 无符号下溢驱动
- 风险：可能仍有未覆盖 case

## 下一步执行流程

1. 读取 moptm_fusion.cpp 的 absDivMu 主循环（L3282-L3424）
2. 对比 GMP `mpn_preinv_mu_divappr_q` 的 L182-L285
3. 找出 moptm 当前实现的偏差点
4. 实施修复（优先策略 A）
5. 上传 VM 编译 + 200 fuzz 验证
6. 若全 PASS：benchmark + 默认启用 + 更新 O3_DEV_LOG.md

## 索引使用约定

- 所有行号均为源文件实际行号（cat -n 格式）
- 函数边界含签名行至 `}` 结束行
- 关键算法步骤附带行号区间
- 修复 bug 时只读相关索引 MD，不读源文件（避免上下文爆炸）
