# -*- coding: utf-8 -*-
"""D30 = D27(大页 arena) + 软件流水化的融合 submul_1。

D29 已证明「融合成单趟」是逐字节恒等且能砍指令的 (rnz_01 -4.5%, medium_01 -18.7%),
但 perf 显示 IPC 从 2.22 塌到 1.74 / 从 2.04 塌到 1.35 —— 因为乘法 SWAR 链
(~43 cyc 产出 m) 与减法 SWAR 链 (~24 cyc) 在同一迭代里首尾相接, 单迭代关键路径
~67 cyc 却只有 ~50 条指令, 要 6+ 迭代同时在飞才填得满流水, 超出 Zen3 的 ROB。

D30 把两条链错开一个迭代 (软件流水):
    第 k 轮:  算 m_k (乘法链)  ||  减 m_{k-1} (减法链, 数据上周期就绪)
两条链互不依赖, 单迭代关键路径降到 max(43, 24), 需要在飞的迭代数减半。

用 -DDIV_FUSED_SUBMUL=0 退回原三趟实现做 A/B。
"""
import io, os

SRC = os.path.join(os.path.dirname(__file__), '..', 'best', 'div_D27.cpp')
DST = os.path.join(os.path.dirname(__file__), '..', 'best', 'div_D30.cpp')

s = io.open(SRC, encoding='utf-8', errors='surrogateescape').read()
n0 = len(s)

# ---------------------------------------------------------------- [A] 开关宏
ANCH_GUARD = '#ifndef INV_NEWTON_BASE_THRESHOLD'
assert s.count(ANCH_GUARD) == 1, 'guard anchor'
s = s.replace(ANCH_GUARD, '''#ifndef DIV_FUSED_SUBMUL
#define DIV_FUSED_SUBMUL 1
#endif
''' + ANCH_GUARD, 1)
print('[A] DIV_FUSED_SUBMUL switch inserted')

# ------------------------------------------------- [B] 插入流水化 absSubMul1
ANCH_FN = '        static void absDivBasicCore(Span dividend, View divisor, Span quotient)\n'
assert s.count(ANCH_FN) == 1, 'absDivBasicCore anchor'

PRIM = r'''        // === D30: 软件流水化的融合 submul_1 —— np[0..n] -= dp[0..n-1] * x ===
        //
        // mulblk(i)      : 算出 dp[i..i+15]*x 的 16 个乘积 limb (推进 carry_q/carry_rip)
        // subblk(i, m)   : np[i..i+15] -= m                     (推进 borrow)
        // 两者在循环体里错开一个块 —— 减法用的是上一轮算好的 m, 与本轮乘法无依赖,
        // 于是两条 SWAR 链并行推进, 而不是串成 ~67 cyc 的长链 (D29 的 IPC 塌陷原因)。
        static bool absSubMul1(Limb *np, const Limb *dp, size_t n, Limb x)
        {
            static_assert(sizeof(Limb) == 2 && BASE == 10000,
                          "absSubMul1 AVX2 path assumes uint16 limbs in base 1e4");
            size_t i = 0;
            uint32_t carry_q = 0;   // 乘法: 上一 limb 的高位 q
            uint32_t carry_rip = 0; // 乘法: 上一 limb 的涟漪进位
            uint32_t borrow = 0;    // 减法: 借位
            if (n >= 16)
            {
                const __m256i vx = _mm256_set1_epi32(int(uint32_t(x)));
                const __m256i vmagic = _mm256_set1_epi32(109951163); // ceil(2^40 / 1e4)
                const __m256i vbase32 = _mm256_set1_epi32(int(BASE));
                const __m256i vbase16 = _mm256_set1_epi16(short(BASE));
                const __m256i vbm1 = _mm256_set1_epi16(short(BASE - 1));
                const __m256i vbit = _mm256_setr_epi16(
                    1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192,
                    short(16384), short(32768));

                auto mulblk = [&](size_t idx) -> __m256i
                {
                    __m256i a16 = _mm256_loadu_si256(
                        reinterpret_cast<const __m256i *>(dp + idx));
                    __m256i a0 = _mm256_cvtepu16_epi32(_mm256_castsi256_si128(a16));
                    __m256i a1 = _mm256_cvtepu16_epi32(_mm256_extracti128_si256(a16, 1));
                    __m256i p0 = _mm256_mullo_epi32(a0, vx);
                    __m256i p1 = _mm256_mullo_epi32(a1, vx);
                    auto divb = [&](__m256i p) -> __m256i
                    {
                        __m256i ev = _mm256_mul_epu32(p, vmagic);
                        __m256i od = _mm256_mul_epu32(_mm256_srli_epi64(p, 32), vmagic);
                        ev = _mm256_srli_epi64(ev, 40);
                        od = _mm256_srli_epi64(od, 40);
                        return _mm256_blend_epi32(ev, _mm256_slli_epi64(od, 32), 0xAA);
                    };
                    __m256i q0 = divb(p0), q1 = divb(p1);
                    __m256i r0 = _mm256_sub_epi32(p0, _mm256_mullo_epi32(q0, vbase32));
                    __m256i r1 = _mm256_sub_epi32(p1, _mm256_mullo_epi32(q1, vbase32));
                    __m256i q16 = _mm256_permute4x64_epi64(
                        _mm256_packus_epi32(q0, q1), 0xD8);
                    __m256i r16 = _mm256_permute4x64_epi64(
                        _mm256_packus_epi32(r0, r1), 0xD8);
                    __m256i qsh = _mm256_alignr_epi8(
                        q16, _mm256_permute2x128_si256(q16, q16, 0x08), 14);
                    qsh = _mm256_insert_epi16(qsh, int(carry_q), 0);
                    carry_q = uint32_t(uint16_t(_mm256_extract_epi16(q16, 15)));
                    __m256i t = _mm256_add_epi16(r16, qsh); // < 2*BASE < 32768
                    __m256i gm = _mm256_cmpgt_epi16(t, vbm1);
                    __m256i pm = _mm256_cmpeq_epi16(t, vbm1);
                    uint32_t G = _pext_u32(uint32_t(_mm256_movemask_epi8(gm)), 0x55555555u);
                    uint32_t P = _pext_u32(uint32_t(_mm256_movemask_epi8(pm)), 0x55555555u);
                    uint32_t A = G, B = G | P;
                    uint32_t sw = A + B + carry_rip;
                    uint32_t cin = sw ^ A ^ B;
                    uint32_t cout = cin >> 1;
                    carry_rip = (sw >> 16) & 1u;
                    __m256i vcin = _mm256_cmpeq_epi16(
                        _mm256_and_si256(_mm256_set1_epi16(short(cin)), vbit), vbit);
                    __m256i vcout = _mm256_cmpeq_epi16(
                        _mm256_and_si256(_mm256_set1_epi16(short(cout)), vbit), vbit);
                    return _mm256_sub_epi16(
                        _mm256_add_epi16(t, _mm256_srli_epi16(vcin, 15)),
                        _mm256_and_si256(vcout, vbase16));
                };

                auto subblk = [&](size_t idx, __m256i m)
                {
                    __m256i d = _mm256_loadu_si256(
                        reinterpret_cast<const __m256i *>(np + idx));
                    __m256i ts = _mm256_sub_epi16(_mm256_add_epi16(d, vbase16), m);
                    __m256i gs = _mm256_cmpgt_epi16(vbase16, ts); // ts <  BASE <=> d < m
                    __m256i ps = _mm256_cmpeq_epi16(ts, vbase16); // ts == BASE <=> d == m
                    uint32_t G2 = _pext_u32(uint32_t(_mm256_movemask_epi8(gs)), 0x55555555u);
                    uint32_t P2 = _pext_u32(uint32_t(_mm256_movemask_epi8(ps)), 0x55555555u);
                    uint32_t A2 = G2, B2 = G2 | P2;
                    uint32_t s2 = A2 + B2 + borrow;
                    uint32_t cin2 = s2 ^ A2 ^ B2;
                    uint32_t cout2 = cin2 >> 1;
                    borrow = (s2 >> 16) & 1u;
                    __m256i vcin2 = _mm256_cmpeq_epi16(
                        _mm256_and_si256(_mm256_set1_epi16(short(cin2)), vbit), vbit);
                    __m256i vcout2 = _mm256_cmpeq_epi16(
                        _mm256_and_si256(_mm256_set1_epi16(short(cout2)), vbit), vbit);
                    __m256i res = _mm256_sub_epi16(
                        _mm256_sub_epi16(ts, _mm256_srli_epi16(vcin2, 15)),
                        _mm256_andnot_si256(vcout2, vbase16));
                    _mm256_storeu_si256(reinterpret_cast<__m256i *>(np + idx), res);
                };

                // 软件流水: 先把块 0 的乘积算出来垫底
                size_t last = 0;
                __m256i mprev = mulblk(0);
                for (i = 16; i + 15 < n; i += 16)
                {
                    __m256i mcur = mulblk(i);   // 乘法链
                    subblk(i - 16, mprev);      // 减法链 (与上式无依赖)
                    mprev = mcur;
                    last = i;
                }
                subblk(last, mprev);            // 排空
                i = last + 16;
            }
            // ---- 标量尾部: 乘法进位与减法借位继续串
            Limb mcarry = Limb(carry_q + carry_rip);
            uint32_t bw = borrow;
            for (; i < n; i++)
            {
                Limb2 prod = Limb2(dp[i]) * x + mcarry;
                uint32_t ml = uint32_t(prod % BASE);
                mcarry = Limb(prod / BASE);
                uint32_t sub = ml + bw;
                uint32_t dv = np[i];
                if (dv >= sub) { np[i] = Limb(dv - sub); bw = 0; }
                else           { np[i] = Limb(dv + BASE - sub); bw = 1; }
            }
            // ---- 窗口顶 limb: 吃掉乘法最终进位
            uint32_t sub = uint32_t(mcarry) + bw;
            uint32_t dv = np[n];
            if (dv >= sub) { np[n] = Limb(dv - sub); return false; }
            np[n] = Limb(dv + BASE - sub);
            return true;
        }

        // qhat 估大后的 add-back: np[0..n] += dp[0..n-1], 返回是否进位出顶。
        // 概率约 2/BASE, 走纯标量足够。
        static bool absAddBackDiv(Limb *np, const Limb *dp, size_t n)
        {
            uint32_t carry = 0;
            for (size_t i = 0; i < n; i++)
            {
                uint32_t v = uint32_t(np[i]) + dp[i] + carry;
                if (v >= BASE) { np[i] = Limb(v - BASE); carry = 1; }
                else           { np[i] = Limb(v);        carry = 0; }
            }
            uint32_t v = uint32_t(np[n]) + carry;
            if (v >= BASE) { np[n] = Limb(v - BASE); return true; }
            np[n] = Limb(v);
            return false;
        }

'''
s = s.replace(ANCH_FN, PRIM + ANCH_FN, 1)
print('[B] pipelined absSubMul1 / absAddBackDiv inserted')

# --------------------------------------------------- [C] 改写 absDivBasicCore 主循环
OLD = '''            thread_local std::vector<Limb> tprod;
            if (tprod.size() < len2 + 1)
                tprod.resize(len2 + 1);
            while (quot_idx > 0)
            {
                quot_idx--;
                len1 = quot_idx + len2;
                Limb high1 = dividend[len1], high2 = dividend[len1 - 1], qhat = 0;
                
                if (high1 >= divisor_high)
                {
                    qhat = BASE - 1;
                }
                else
                {
                    Limb2 high = Limb2(high1) * BASE + high2;
                    qhat = high / divisor_high;
                }
                Span prod_span(tprod.data(), len2 + 1);
                prod_span[len2] = absMul1(divisor, qhat, prod_span);
                if (prod_span[len2] == 0)
                {
                    prod_span.size = len2;
                }
                Span dividend_span(dividend + quot_idx);
                int count = 0;
                while (absCompare(View(prod_span), View(dividend_span)) > 0)
                {
                    assert(count < 2);
                    count++;
                    auto bf = absSub(prod_span, divisor, prod_span);
                    qhat--;
                    assert(!bf);
                }
                auto bf = absSub(dividend_span, prod_span, dividend_span);
                assert(!bf);
                quotient[quot_idx] = qhat;
                dividend.size = len1;
            }
        }'''
assert s.count(OLD) == 1, 'absDivBasicCore loop anchor (count=%d)' % s.count(OLD)

NEW = '''#if DIV_FUSED_SUBMUL
            const Limb *dp = divisor.ptr;
            while (quot_idx > 0)
            {
                quot_idx--;
                len1 = quot_idx + len2;
                Limb high1 = dividend[len1], high2 = dividend[len1 - 1], qhat = 0;
                if (high1 >= divisor_high)
                {
                    qhat = BASE - 1;
                }
                else
                {
                    Limb2 high = Limb2(high1) * BASE + high2;
                    qhat = high / divisor_high;
                }
                // 窗口恒为 len2+1 limb: dividend[quot_idx .. quot_idx+len2]
                Limb *np = dividend.ptr + quot_idx;
                bool under = absSubMul1(np, dp, len2, qhat);
                if (__builtin_expect(under, 0))
                {
                    int adds = 0;
                    do
                    {
                        assert(adds < 2);
                        adds++;
                        qhat--;
                        under = !absAddBackDiv(np, dp, len2);
                    } while (under);
                }
                quotient[quot_idx] = qhat;
                dividend.size = len1;
            }
        }
#else
            thread_local std::vector<Limb> tprod;
            if (tprod.size() < len2 + 1)
                tprod.resize(len2 + 1);
            while (quot_idx > 0)
            {
                quot_idx--;
                len1 = quot_idx + len2;
                Limb high1 = dividend[len1], high2 = dividend[len1 - 1], qhat = 0;
                
                if (high1 >= divisor_high)
                {
                    qhat = BASE - 1;
                }
                else
                {
                    Limb2 high = Limb2(high1) * BASE + high2;
                    qhat = high / divisor_high;
                }
                Span prod_span(tprod.data(), len2 + 1);
                prod_span[len2] = absMul1(divisor, qhat, prod_span);
                if (prod_span[len2] == 0)
                {
                    prod_span.size = len2;
                }
                Span dividend_span(dividend + quot_idx);
                int count = 0;
                while (absCompare(View(prod_span), View(dividend_span)) > 0)
                {
                    assert(count < 2);
                    count++;
                    auto bf = absSub(prod_span, divisor, prod_span);
                    qhat--;
                    assert(!bf);
                }
                auto bf = absSub(dividend_span, prod_span, dividend_span);
                assert(!bf);
                quotient[quot_idx] = qhat;
                dividend.size = len1;
            }
        }
#endif'''
s = s.replace(OLD, NEW, 1)
print('[C] absDivBasicCore main loop fused (pipelined)')

io.open(DST, 'w', encoding='utf-8', errors='surrogateescape', newline='').write(s)
print('written %s : %d -> %d bytes' % (os.path.abspath(DST), n0, len(s)))
