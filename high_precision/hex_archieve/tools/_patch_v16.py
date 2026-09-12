import io
p = 'work/mul/v16.cpp'
s = io.open(p, encoding='utf-8').read()

# 唯一且稳健的锚点: 命名空间闭合行. 注入的混合 radix 机械放在命名空间内最后.
marker = "}  // namespace fft\n"
assert marker in s, "namespace close marker not found"

machinery = r'''    // ============ 混合 radix 顶层蝶形 (移植自 best/div_verifield 前人的实数FFT; 本路径走全复数) ============
    // 数学完全照搬前人 dif3Stage/idit3Stage/dif5Stage/idit5Stage; 旋转因子 base = -2pi/(3m) 或 -2pi/(5m),
    // 即标准复数 DFT 的 W_{3m}^c / W_{5m}^{jc} (前人 FFTTable3/5 的 FACTOR=1,2 / 1..4), 非实数半长约定.
    // 精度: fft_ceil_tiers 选的 lm' 恒 <= 原 2 幂 lm, 故 2^(2k)*lm <= 2^48 预算天然不破.
    static const double SQRT3_DIV2 = 0.866025403784438646763723170752936;
    static const double R5_C1 = 0.309016994374947424102293417183;  // cos(2pi/5)
    static const double R5_S1 = 0.951056516295153572116439333379;  // sin(2pi/5)
    static const double R5_C2 = -0.809016994374947424102293417183; // cos(4pi/5)
    static const double R5_S2 = 0.587785252292473129078558064009;  // sin(4pi/5)
    static inline cpx mkc(double re, double im) { return _mm_set_pd(im, re); }
    static inline double cre(cpx z) { return _mm_cvtsd_f64(z); }
    static inline double cim(cpx z) { return _mm_cvtsd_f64(_mm_unpackhi_pd(z, z)); }

    struct MR_Tw {                       // 顶层蝶形旋转因子缓存 (按 m 复用)
        std::vector<cpx> t1, t2, t3, t4;
        size_t cached_m = 0;
        void ensure3(size_t m) {
            if (cached_m == m && !t1.empty()) return;
            t1.resize(m); t2.resize(m);
            const double base = -3.14159265358979323846 * 2.0 / (3.0 * (double)m); // -2pi/(3m)
            for (size_t c = 0; c < m; ++c) {
                double a1 = base * (double)c, a2 = base * (2.0 * (double)c);
                t1[c] = mkc(std::cos(a1), std::sin(a1));   // W_{3m}^c
                t2[c] = mkc(std::cos(a2), std::sin(a2));   // W_{3m}^{2c}
            }
            cached_m = m; t3.clear(); t4.clear();
        }
        void ensure5(size_t m) {
            if (cached_m == m && !t1.empty()) return;
            t1.resize(m); t2.resize(m); t3.resize(m); t4.resize(m);
            const double base = -3.14159265358979323846 * 2.0 / (5.0 * (double)m); // -2pi/(5m)
            for (size_t c = 0; c < m; ++c) {
                double a = base * (double)c;
                t1[c] = mkc(std::cos(a),      std::sin(a));      // W_{5m}^c
                t2[c] = mkc(std::cos(2*a),    std::sin(2*a));    // W_{5m}^{2c}
                t3[c] = mkc(std::cos(3*a),    std::sin(3*a));    // W_{5m}^{3c}
                t4[c] = mkc(std::cos(4*a),    std::sin(4*a));    // W_{5m}^{4c}
            }
            cached_m = m;
        }
    };
    static MR_Tw g_mr_tw;

    // radix-3 DIF 顶层: 总复数 ts=3m, 块 i 在 (cpx*)b0 + i*m. 数学照搬前人 dif3Stage.
    static void dif3StageR(cpx* b0, u32 m) {
        g_mr_tw.ensure3(m);
        const cpx* p1 = g_mr_tw.t1.data();
        const cpx* p2 = g_mr_tw.t2.data();
        const double H = 0.5, S = SQRT3_DIV2;
        for (u32 c = 0; c < m; ++c) {
            cpx a0 = b0[c], a1 = b0[(size_t)m + c], a2 = b0[2*(size_t)m + c];
            cpx s = _mm_add_pd(a1, a2), d = _mm_sub_pd(a1, a2);
            cpx r0 = _mm_add_pd(a0, s);
            cpx h = _mm_sub_pd(a0, _mm_mul_pd(s, _mm_set1_pd(H)));
            cpx g = cscale(mulI(d), S);
            b0[c] = r0;
            b0[(size_t)m + c] = cmul(_mm_sub_pd(h, g), p1[c]);
            b0[2*(size_t)m + c] = cmul(_mm_add_pd(h, g), p2[c]);
        }
    }
    static void idit3StageR(cpx* b0, u32 m) {
        g_mr_tw.ensure3(m);
        const cpx* p1 = g_mr_tw.t1.data();
        const cpx* p2 = g_mr_tw.t2.data();
        const double H = 0.5, S = SQRT3_DIV2;
        for (u32 c = 0; c < m; ++c) {
            cpx t0 = b0[c], b1 = b0[(size_t)m + c], b2 = b0[2*(size_t)m + c];
            cpx x1 = cmulconj(b1, p1[c]), x2 = cmulconj(b2, p2[c]);
            cpx s = _mm_add_pd(x1, x2), d = _mm_sub_pd(x1, x2);
            cpx a0 = _mm_add_pd(t0, s);
            cpx h = _mm_sub_pd(t0, _mm_mul_pd(s, _mm_set1_pd(H)));
            cpx g = cscale(mulI(d), S);
            b0[c] = a0;
            b0[(size_t)m + c] = _mm_add_pd(h, g);
            b0[2*(size_t)m + c] = _mm_sub_pd(h, g);
        }
    }
    // radix-5 DIF 顶层: 总复数 ts=5m, 块 i 在 (cpx*)b0 + i*m. 数学照搬前人 dif5Stage.
    static void dif5StageR(cpx* b0, u32 m) {
        g_mr_tw.ensure5(m);
        const cpx* p1 = g_mr_tw.t1.data(); const cpx* p2 = g_mr_tw.t2.data();
        const cpx* p3 = g_mr_tw.t3.data(); const cpx* p4 = g_mr_tw.t4.data();
        const double c1 = R5_C1, s1 = R5_S1, c2 = R5_C2, s2 = R5_S2;
        for (u32 c = 0; c < m; ++c) {
            cpx a0=b0[c], a1=b0[(size_t)m+c], a2=b0[2*(size_t)m+c], a3=b0[3*(size_t)m+c], a4=b0[4*(size_t)m+c];
            cpx t1=_mm_add_pd(a1,a4), t2=_mm_add_pd(a2,a3), t3=_mm_sub_pd(a1,a4), t4=_mm_sub_pd(a2,a3);
            cpx u1 = mkc(cre(a0)+cre(t1)*c1+cre(t2)*c2, cim(a0)+cim(t1)*c1+cim(t2)*c2);
            cpx u2 = mkc(cre(a0)+cre(t1)*c2+cre(t2)*c1, cim(a0)+cim(t1)*c2+cim(t2)*c1);
            cpx v1 = mkc(cre(t3)*s1+cre(t4)*s2, cim(t3)*s1+cim(t4)*s2);
            cpx v2 = mkc(cre(t3)*s2-cre(t4)*s1, cim(t3)*s2-cim(t4)*s1);
            cpx A0 = _mm_add_pd(a0, _mm_add_pd(t1, t2));
            cpx A1 = mkc(cre(u1)+cim(v1), cim(u1)-cre(v1));   // u1 - i*v1
            cpx A4 = mkc(cre(u1)-cim(v1), cim(u1)+cre(v1));   // u1 + i*v1
            cpx A2 = mkc(cre(u2)+cim(v2), cim(u2)-cre(v2));   // u2 - i*v2
            cpx A3 = mkc(cre(u2)-cim(v2), cim(u2)+cre(v2));   // u2 + i*v2
            b0[c]=A0;
            b0[(size_t)m+c]=cmul(A1,p1[c]);
            b0[2*(size_t)m+c]=cmul(A2,p2[c]);
            b0[3*(size_t)m+c]=cmul(A3,p3[c]);
            b0[4*(size_t)m+c]=cmul(A4,p4[c]);
        }
    }
    static void idit5StageR(cpx* b0, u32 m) {
        g_mr_tw.ensure5(m);
        const cpx* p1 = g_mr_tw.t1.data(); const cpx* p2 = g_mr_tw.t2.data();
        const cpx* p3 = g_mr_tw.t3.data(); const cpx* p4 = g_mr_tw.t4.data();
        const double c1 = R5_C1, s1 = R5_S1, c2 = R5_C2, s2 = R5_S2;
        for (u32 c = 0; c < m; ++c) {
            cpx y0=b0[c], y1=b0[(size_t)m+c], y2=b0[2*(size_t)m+c], y3=b0[3*(size_t)m+c], y4=b0[4*(size_t)m+c];
            cpx x1=cmulconj(y1,p1[c]), x2=cmulconj(y2,p2[c]), x3=cmulconj(y3,p3[c]), x4=cmulconj(y4,p4[c]);
            cpx t1=_mm_add_pd(x1,x4), t2=_mm_add_pd(x2,x3), t3=_mm_sub_pd(x1,x4), t4=_mm_sub_pd(x2,x3);
            cpx u1 = mkc(cre(y0)+cre(t1)*c1+cre(t2)*c2, cim(y0)+cim(t1)*c1+cim(t2)*c2);
            cpx u2 = mkc(cre(y0)+cre(t1)*c2+cre(t2)*c1, cim(y0)+cim(t1)*c2+cim(t2)*c1);
            cpx v1 = mkc(cre(t3)*s1+cre(t4)*s2, cim(t3)*s1+cim(t4)*s2);
            cpx v2 = mkc(cre(t3)*s2-cre(t4)*s1, cim(t3)*s2-cim(t4)*s1);
            cpx a0 = _mm_add_pd(y0, _mm_add_pd(t1, t2));
            cpx a1 = mkc(cre(u1)-cim(v1), cim(u1)+cre(v1));   // u1 + i*v1
            cpx a4 = mkc(cre(u1)+cim(v1), cim(u1)-cre(v1));   // u1 - i*v1
            cpx a2 = mkc(cre(u2)-cim(v2), cim(u2)+cim(v2));   // u2 + i*v2
            cpx a3 = mkc(cre(u2)+cim(v2), cim(u2)-cim(v2));   // u2 - i*v2
            b0[c]=a0; b0[(size_t)m+c]=a1; b0[2*(size_t)m+c]=a2; b0[3*(size_t)m+c]=a3; b0[4*(size_t)m+c]=a4;
        }
    }
'''

s = s.replace(marker, machinery + marker, 1)

old_len = '''static inline u32 fft_len_for(size_t u, int k) {
    const u32 c = (u32)((u * 64 + (size_t)k - 1) / (size_t)k);
    return (c & (c - 1)) ? (2u << (31 - __builtin_clz(c))) : c;
}'''
new_len = '''// 返回 >= c 的最小可用 FFT 长度(doubles): 2^k / 3*2^(k-2) / 5*2^(k-3).
// 混合档位恒 <= next_pow2(c), 故 2^(2k)*lm <= 2^48 精度预算天然不破.
static inline u32 fft_ceil_tiers(u32 c) {
    u32 p = (c & (c - 1)) ? (2u << (31 - __builtin_clz(c))) : c;  // next_pow2(c)
    u32 best = p;
    if (c != p) {
        u32 h = (p >> 2) * 3;          // 3*2^(k-2)
        if (h >= c && h < best) best = h;
        u32 h5 = (p >> 3) * 5;         // 5*2^(k-3)
        if (h5 >= c && h5 < best) best = h5;
    }
    return best;
}
static inline u32 fft_len_for(size_t u, int k) {
    const u32 c = (u32)((u * 64 + (size_t)k - 1) / (size_t)k);
    return fft_ceil_tiers(c);
}'''
assert old_len in s, "fft_len_for not found"
s = s.replace(old_len, new_len, 1)

old_mul = '''static void mul_fft(const u64* a, int na, const u64* b, int nb, u64* c) {
    const size_t u = (size_t)na + nb;
    const int k = pick_k(u);
    const u32 lm = fft_len_for(u, k);
    const bool same = (a == b) && (na == nb);
    const u32 ts = lm >> 1;
    // zero-hi: dan ge cao zuo shu xi shu shu <= lm/2 shi, shi shu huan chong shang ban zheng kuai wei ling,
    // ding ceng radix-4 ke zhi du xia ban (sheng 1/2 du liu liang + split sheng 1/2 qing ling).
    const bool deep = ts > (1u << FFT_LEAF_LOG);
    const size_t half = lm >> 1;
    const size_t ta = split_b2(a, FB, na, k, deep ? half : lm);
    const bool hiA = deep && ta <= half;
    // deep 且未命中 zero-hi 时 split 只清到 half(<ta) 等于没清, 需补清 [ta, lm)
    if (deep && !hiA && ta < lm) std::memset(FB + ta, 0, (lm - ta) * 8);
    size_t tb = ta; bool hiB = hiA;
    if (!same) {
        tb = split_b2(b, GB, nb, k, deep ? half : lm);
        hiB = deep && tb <= half;
        if (deep && !hiB && tb < lm) std::memset(GB + tb, 0, (lm - tb) * 8);
    }
    fft::resize(ts);
    if (hiA) fft::difRecZeroHi((fft::cpx*)FB, ts); else fft::difRec((fft::cpx*)FB, ts, 0);
    if (same) {
        fft::pointwiseSq((fft::cpx*)FB, ts);
    } else {
        if (hiB) fft::difRecZeroHi((fft::cpx*)GB, ts); else fft::difRec((fft::cpx*)GB, ts, 0);
        fft::pointwise((fft::cpx*)FB, (fft::cpx*)GB, ts);
    }
    fft::ditRec((fft::cpx*)FB, ts, 0);
    merge_b2(c, FB, u, k);
}'''
new_mul = '''static void mul_fft(const u64* a, int na, const u64* b, int nb, u64* c) {
    const size_t u = (size_t)na + nb;
    const int k = pick_k(u);
    const u32 lm = fft_len_for(u, k);
    const bool same = (a == b) && (na == nb);
    const u32 ts = lm >> 1;                 // 总复数点数
    const size_t ta = split_b2(a, FB, na, k, lm);
    if (ta < lm) std::memset(FB + ta, 0, (lm - ta) * 8);
    size_t tb = ta;
    if (!same) {
        tb = split_b2(b, GB, nb, k, lm);
        if (tb < lm) std::memset(GB + tb, 0, (lm - tb) * 8);
    }
    // ---- 混合 radix 路径 (全复数; 点乘复用现成 pointwise/pointwiseSq 的共轭对称归一化) ----
    // 调度与 fft_ceil_tiers 严格对应: 3 整除 <=> 3*2^(k-2) (m=ts/3 为 2 幂);
    // 5 整除 <=> 5*2^(k-3) (m=ts/5 为 2 幂); 否则纯 2 幂. 三者互斥且穷尽.
    if (lm % 3 == 0) {
        const u32 m = ts / 3;               // 每块复数数 (2 的幂)
        fft::resize(m);
        fft::dif3StageR((fft::cpx*)FB, m);
        fft::difRec((fft::cpx*)FB,                 m, 0);
        fft::difRec((fft::cpx*)(FB + 2*(size_t)m), m, 0);
        fft::difRec((fft::cpx*)(FB + 4*(size_t)m), m, 0);
        if (same) {
            fft::pointwiseSq((fft::cpx*)FB, ts);
        } else {
            fft::dif3StageR((fft::cpx*)GB, m);
            fft::difRec((fft::cpx*)GB,                 m, 0);
            fft::difRec((fft::cpx*)(GB + 2*(size_t)m), m, 0);
            fft::difRec((fft::cpx*)(GB + 4*(size_t)m), m, 0);
            fft::pointwise((fft::cpx*)FB, (fft::cpx*)GB, ts);
        }
        fft::ditRec((fft::cpx*)FB,                 m, 0);
        fft::ditRec((fft::cpx*)(FB + 2*(size_t)m), m, 0);
        fft::ditRec((fft::cpx*)(FB + 4*(size_t)m), m, 0);
        fft::idit3StageR((fft::cpx*)FB, m);
    } else if (lm % 5 == 0) {
        const u32 m = ts / 5;
        fft::resize(m);
        fft::dif5StageR((fft::cpx*)FB, m);
        for (int i = 0; i < 5; ++i) fft::difRec((fft::cpx*)(FB + 2*(size_t)m*i), m, 0);
        if (same) {
            fft::pointwiseSq((fft::cpx*)FB, ts);
        } else {
            fft::dif5StageR((fft::cpx*)GB, m);
            for (int i = 0; i < 5; ++i) fft::difRec((fft::cpx*)(GB + 2*(size_t)m*i), m, 0);
            fft::pointwise((fft::cpx*)FB, (fft::cpx*)GB, ts);
        }
        for (int i = 0; i < 5; ++i) fft::ditRec((fft::cpx*)(FB + 2*(size_t)m*i), m, 0);
        fft::idit5StageR((fft::cpx*)FB, m);
    } else {
        // ---- 纯 2 幂路径 (原 v13, 含 zero-hi 优化) ----
        const bool deep = ts > (1u << FFT_LEAF_LOG);
        const size_t half = lm >> 1;
        const bool hiA = deep && ta <= half;
        if (deep && !hiA && ta < lm) std::memset(FB + ta, 0, (lm - ta) * 8);
        const bool hiB = deep && tb <= half;
        if (deep && !hiB && tb < lm) std::memset(GB + tb, 0, (lm - tb) * 8);
        fft::resize(ts);
        if (hiA) fft::difRecZeroHi((fft::cpx*)FB, ts); else fft::difRec((fft::cpx*)FB, ts, 0);
        if (same) {
            fft::pointwiseSq((fft::cpx*)FB, ts);
        } else {
            if (hiB) fft::difRecZeroHi((fft::cpx*)GB, ts); else fft::difRec((fft::cpx*)GB, ts, 0);
            fft::pointwise((fft::cpx*)FB, (fft::cpx*)GB, ts);
        }
        fft::ditRec((fft::cpx*)FB, ts, 0);
    }
    merge_b2(c, FB, u, k);
}'''
assert old_mul in s, "mul_fft not found"
s = s.replace(old_mul, new_mul, 1)

io.open(p, 'w', encoding='utf-8', newline='\n').write(s)
print("v16.cpp patched OK")
