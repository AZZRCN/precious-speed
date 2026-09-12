// ============================================================================
//  cyclic_archived.cpp  —  HEX 除法 cyclic (mod B^m-1) 路径封存档案
// ============================================================================
//
//  封存日期: 2026-08-15
//  封存原因: 半尺寸 cyclic 在「逐块精确商」算法下结构性不可修复; 全尺寸 cyclic
//            速度 == 线性 (零收益); 线性 FFT 路径已实测超过 GMP 9.3%。
//  现状: 生产源码 div_base16.cpp 已 #define DISABLE_2NXN_CYCLIC + allow_cyclic=false,
//        cyclic 代码全部编译期排除、运行期永不触发。本文件为参考存档, 不编译。
//
// ----------------------------------------------------------------------------
//  根因 (决定性, GMP 真身实测收口)
// ----------------------------------------------------------------------------
//  * 半尺寸 cyclic 卷积 cyclic_m ≈ (len2+in)/2 (FFT 收益来源), 但 qhat*divisor
//    有 len2+this_in 位, wrap 次数 k ≈ B^(len2+in-cyclic_m)。对中等规模
//    (len2=65 limbs, in≈同量级) k ≈ 65535。
//  * absDivMu 的 r-method 用单 limb 判据 r = window[len2]-tprod[len2] 与 ±1 unwrap
//    修正, 只能吃 k∈{-1,0,1}。k≈65535 时 r 彻底失真 → qhat 与余数全错。
//  * 实测 (burnikel_ziegler_bound seed=2): cyc_bad=488, 且 both_wrong=488
//    (q 和 r 一起错, q 偏 ~0xA0000) → 无法靠"末尾精确算 R" 救 (归一化要循环 4 万次)。
//  * GMP 的 mpn_mu_div_qr 能用 cyclic 是因为其 tn≈dn+1 (每步只算 1 limb 商, 无 wrap);
//    我们每步算 in limb 商, 必须 m>=in+len2 才无 wrap —— 那又等于线性 FFT。
//  * 全尺寸 cyclic (m=in+len2) 实测与线性指令数完全相同 (差 27 条 / 3.87 亿), 零收益。
//  * GMP 真身 (gmp_div_test.cpp, 同 400k/206k limbs, perf instructions:u, 纯内核):
//        我们线性 FFT     386,773,403
//        GMP mpn_mu_div_qr 426,210,053   → 我们快 9.3%
//    => cyclic 不是提速捷径; 提速只在 FFT 内核本身 (见 perf_bottleneck.md)。
//
// ----------------------------------------------------------------------------
//  提取的 cyclic 代码 (来自 div_base16.cpp 封存前快照, 行号为封存前)
// ----------------------------------------------------------------------------

// ---- [A] cyclic 卷积原语: mod B^m-1 (封存前 L4016-4072) ----
// fftMulModBm1Pre: fftMulModBm1 的预计算 DFT 版本 (b 的 DFT 预先计算)
// b_dft 长度 = m, 由 prepareDFT(b, b_dft, m) 预计算
static void fftMulModBm1Pre(View a, const double *b_dft, size_t b_len, size_t m, Span out)
{
    assert(is_2pow(m) || is_fft3(m) || is_fft5(m));
    assert(out.size >= m);
    size_t a_len = count_true_length(a.ptr, a.size);
    if (a_len == 0) {
        std::fill_n(out.ptr, m, Limb(0));
        if (out.size > m) std::fill_n(out.ptr + m, out.size - m, Limb(0));
        return;
    }
    assert(a_len <= m);

    thread_local AlignedVec32<double> tv;
    if (tv.capacity() < m) tv.reserve(m);
    double *v = tv.data();

    copyU16ToF64AndFill(a.ptr, v, a_len, m);

    transform::fft::rdif(v, m);
    transform::fft::rdot(v, b_dft, m);
    transform::fft::ridit(v, m);

    // 进位传播 (cyclic mod B^m-1) — 同 fftMulModBm1
    uint64_t carry = carryPropSeg(v, out.ptr, m);
    while (carry > 0) {
        bool wrapped = true;
        for (size_t j = 0; j < m && carry > 0; j++) {
            uint64_t s = uint64_t(out[j]) + carry;
            uint64_t q = divBASE(s);
            out[j] = Limb(s - q * BASE);
            carry = q;
            if (carry == 0) { wrapped = false; break; }
        }
        if (wrapped && carry > 0) {
            if (carry == 1) {
                Limb c = 1;
                for (size_t j = 0; j < m; j++) {
                    if (out[j] + c < BASE) { out[j] = Limb(out[j] + c); c = 0; break; }
                    else { out[j] = Limb(out[j] + c - BASE); }
                }
                carry = 0;
            }
        }
    }
    if (out.size > m) std::fill_n(out.ptr + m, out.size - m, Limb(0));
}

// ---- [B] absDivMu 的 cyclic 决策 + GMP 同层全尺寸强制 (封存前 L5663-5723 摘要) ----
//   bool use_cyclic = false;
//   #ifndef DISABLE_2NXN_CYCLIC
//     size_t cyclic_m = max(fft_ceil_cycm(len2+1), fft_ceil_cycm((len2+in)/2+1)); // 半尺寸
//     const size_t cyclic_m_gate = (GATE_POW2) ? max(int_ceil2(len2+1), int_ceil2((len2+in)/2+1))
//                                            : cyclic_m;
//     use_cyclic = (cyclic_m_gate < in+len2) && allow_cyclic;   // 半尺寸 wrap 收益判定
//     if (allow_cyclic) {                 // GMP 同层正确 cyclic: 强制全尺寸 (无 wrap)
//         cyclic_m = fft_ceil_cycm(in+len2+1);
//         use_cyclic = true;
//     }
//   #endif
//   注: 半尺寸 use_cyclic 触发 [C] 的 unwrap + r-method; 全尺寸 use_cyclic 走同块但
//        len2+this_in<=cyclic_m 使 unwrap 自动跳过, r-method 退化为 ±1 调整 (==线性)。

// ---- [C] 块循环内的 2NXN cyclic: unwrap 修正 + r-method (封存前 L5962-6190 摘要) ----
//   #ifndef DISABLE_2NXN_CYCLIC
//   if (use_cyclic) {
//       Span prod_mod_span(tprod.data(), cyclic_m);
//       fftMulModBm1Pre(qhat_span, divisor_dft_mod_buf.data(), len2, cyclic_m, prod_mod_span);
//       if (len2 + this_in > cyclic_m) {            // 半尺寸: 需 unwrap
//           size_t wn = len2 + this_in - cyclic_m;
//           Span prod_low_wn(prod_mod_span.ptr, wn);
//           View rp_high_wn(window.ptr + (len2 + this_in - wn), wn);
//           bool borrow = absSub(prod_low_wn, rp_high_wn, prod_low_wn);
//           Span prod_rest(prod_mod_span.ptr + wn, cyclic_m - wn);
//           if (borrow) borrow = absSub1(prod_rest, 1, prod_rest);
//           // GMP 同层精确修正: cx = (window[len2..] < tprod[len2..]) ; _err = cx - borrow ∈{-1,0,1}
//           size_t _cmp_len = cyclic_m - len2;
//           bool _cx = (absCompare(View(window.ptr+len2,_cmp_len),
//                                   View(prod_mod_span.ptr+len2,_cmp_len)) < 0);
//           int _err = (int)_cx - (int)borrow;
//           if (_err > 0) absAdd1(prod_mod_span, Limb(_err), prod_mod_span);
//           else if (_err < 0) absSub1(prod_mod_span, Limb(-_err), prod_mod_span);
//       }
//   } else { fftMulPre(...); }                       // 线性精确卷积 (生产路径)
//
//   // r-method (GMP mu_divappr_q.c L235-271): 仅半尺寸 cyclic 走此, 结构性失效点
//   if (use_cyclic) {
//       int32_t r = int32_t(window[len2]) - int32_t(tprod[len2]);   // 单 limb 判据
//       ... absSub(window, tprod) ...; r -= cy;
//       int corr_cnt = 0;
//       while (r != 0 && corr_cnt < 10) {            // ±1 修正循环, k≈65535 时修不回
//           if (r > 0) { absAdd1(qhat,1); absSub(tprod,divisor); r -= b; }
//           else       { absSub1(qhat,1); absAdd(tprod,divisor);  r += carry; }
//           corr_cnt++;
//       }
//   }
//   #endif
//
// ----------------------------------------------------------------------------
//  结论: 半尺寸 cyclic 的 wrap 毒化单 limb r 判据 → 结构性失效, 治不了;
//        全尺寸 cyclic == 线性 (零收益)。封存, 专心优化 FFT 内核 (perf_bottleneck.md)。
// ============================================================================
