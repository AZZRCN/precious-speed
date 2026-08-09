# -*- coding: utf-8 -*-
"""D38 = D37 + absDivBasicCore 内层 absMul1+absSub 融合成 absSubMul1 (submul_1).

动机 (callgrind, D37):
  r_nearly_zero_01: absMul1 86.5M(32.94%) + absSub 63.6M(24.21%) = 57.15% 的 Ir,
  绝大部分来自 absDivBasicCore 的 "prod = d*qhat (写 tprod); acc -= prod (读 tprod)"
  两遍写法。融合成一遍后省掉:
    1) tprod 的全长 store  (16 limb/块 一条 vmovdqu)
    2) tprod 的全长 load   (同上)
    3) 一整轮循环的 loop overhead / 指针递增 / 边界判断
  乘法涟漪链与减法借位链都是 32 位标量 SWAR, 可在同一块内串行接力, 不必落内存。
"""
import io, os, re, sys

SRC = r'D:\precious_speed\best\div_D37.cpp'
DST = r'D:\precious_speed\best\div_D38.cpp'

raw = io.open(SRC, encoding='utf-8', newline='').read()
CRLF = raw.count('\r\n') > 0
s = raw.replace('\r\n', '\n')
orig_len = len(raw)

# ---------------------------------------------------------------- 1) 插入 absSubMul1
ANCHOR = '        static Limb absDiv1(View in, Limb x, Span out)'
assert s.count(ANCHOR) == 1, 'absDiv1 anchor not unique'

KERNEL = r'''        // ============================ D38: submul_1 ============================
        // acc -= in * x, 就地。要求 acc.size >= in.size + 1 (顶端 limb 吸收乘法进位)。
        // 返回 true <=> acc 原值 < in*x (即 absDivBasicCore 里 qhat 估大)。
        //
        // 与 "absMul1 写 tprod + absSub 读 tprod" 逐位等价, 但只遍历一次:
        // 乘法侧算出规范化的 res (= (in*x) 的第 i..i+15 个 limb) 后直接留在 YMM 里,
        // 立刻与 acc 做 SWAR 借位减法, tprod 的 store/load 全部消失。
        // 两条 SWAR 链 (乘法涟漪 cin/cout, 减法借位 cin2/cout2) 都是 32 位标量,
        // 互不干扰, 各自跨块传递 1 bit。
        static bool absSubMul1(Span acc, View in, Limb x)
        {
            static_assert(sizeof(Limb) == 2 && BASE == 10000,
                          "absSubMul1 AVX2 path assumes uint16 limbs in base 1e4");
            assert(acc.size >= in.size + 1);
            const size_t n = in.size;
            size_t i = 0;
            uint32_t carry_q = 0;   // 乘法: 上一 limb 的高位 q
            uint32_t carry_rip = 0; // 乘法: 上一 limb 的涟漪进位
            uint32_t borrow = 0;    // 减法: 跨块借位
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
                for (; i + 15 < n; i += 16)
                {
                    // ---- 乘法侧: res = (in*x) 的 limb i..i+15 (与 absMul1 逐位一致) ----
                    __m256i a16 = _mm256_loadu_si256(
                        reinterpret_cast<const __m256i *>(in.ptr + i));
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
                    __m256i res = _mm256_sub_epi16(
                        _mm256_add_epi16(t, _mm256_srli_epi16(vcin, 15)),
                        _mm256_and_si256(vcout, vbase16));
                    // ---- 减法侧: acc[i..] -= res (与 absSub_avx2 逐位一致) ----
                    __m256i av = _mm256_loadu_si256(
                        reinterpret_cast<const __m256i *>(acc.ptr + i));
                    __m256i t2 = _mm256_sub_epi16(_mm256_add_epi16(av, vbase16), res);
                    __m256i gm2 = _mm256_cmpgt_epi16(vbase16, t2); // acc < res
                    __m256i pm2 = _mm256_cmpeq_epi16(t2, vbase16); // acc == res
                    uint32_t G2 = _pext_u32(uint32_t(_mm256_movemask_epi8(gm2)), 0x55555555u);
                    uint32_t P2 = _pext_u32(uint32_t(_mm256_movemask_epi8(pm2)), 0x55555555u);
                    uint32_t A2 = G2, B2 = G2 | P2;
                    uint32_t s2 = A2 + B2 + borrow;
                    uint32_t cin2 = s2 ^ A2 ^ B2;
                    uint32_t cout2 = cin2 >> 1;
                    borrow = (s2 >> 16) & 1u;
                    __m256i vcin2 = _mm256_cmpeq_epi16(
                        _mm256_and_si256(_mm256_set1_epi16(short(cin2)), vbit), vbit);
                    __m256i vcout2 = _mm256_cmpeq_epi16(
                        _mm256_and_si256(_mm256_set1_epi16(short(cout2)), vbit), vbit);
                    __m256i r2 = _mm256_sub_epi16(
                        _mm256_sub_epi16(t2, _mm256_srli_epi16(vcin2, 15)),
                        _mm256_andnot_si256(vcout2, vbase16));
                    _mm256_storeu_si256(reinterpret_cast<__m256i *>(acc.ptr + i), r2);
                }
            }
            Limb mcarry = Limb(carry_q + carry_rip);
            Limb bw = Limb(borrow);
            for (; i < n; i++)
            {
                Limb2 prod = Limb2(in[i]) * x + mcarry;
                Limb m = Limb(prod % BASE);
                mcarry = Limb(prod / BASE);
                acc[i] = sub_half<Limb>(acc[i], Limb(m + bw), BASE, bw);
            }
            // 顶端 limb 吸收乘法进位 mcarry (<= BASE-1) 与借位 bw
            acc[i] = sub_half<Limb>(acc[i], Limb(mcarry + bw), BASE, bw);
            i++;
            // 若 acc 还有更高位 (absDivBasicCore 里没有), 继续传播借位
            for (; i < acc.size && bw; i++)
            {
                acc[i] = sub_half<Limb>(acc[i], bw, BASE, bw);
            }
            return bw != 0;
        }

'''
s = s.replace(ANCHOR, KERNEL + ANCHOR, 1)

# ------------------------------------------------- 2) 改写 absDivBasicCore 的调用点
OLD_CALL = '''                Limb qhat = Limb(qh);
                Span prod_span(tprod.data(), len2 + 1);
                prod_span[len2] = absMul1(divisor, qhat, prod_span);
                if (prod_span[len2] == 0)
                {
                    prod_span.size = len2;
                }
                Span dividend_span(dividend + quot_idx);
                // 先减再看借位: 借位 <=> qhat 估大 (D3 之后概率 ~2/BASE)
                bool bf = absSub(dividend_span, prod_span, dividend_span);'''

NEW_CALL = '''                Limb qhat = Limb(qh);
                Span dividend_span(dividend + quot_idx);
                assert(dividend_span.size == len2 + 1);
                // D38: 融合 absMul1 + absSub 为单遍 submul_1, 中间积不再落内存。
                // 先减再看借位: 借位 <=> qhat 估大 (D3 之后概率 ~2/BASE)
                bool bf = absSubMul1(dividend_span, divisor, qhat);'''

assert s.count(OLD_CALL) == 1, 'absDivBasicCore call-site not unique (%d)' % s.count(OLD_CALL)
s = s.replace(OLD_CALL, NEW_CALL, 1)

# ------------------------------------------------- 3) 去掉不再使用的 tprod 声明
OLD_TP = '''            thread_local std::vector<Limb> tprod;
            if (tprod.capacity() < len2 + 1) tprod.reserve(len2 + 1);
'''
assert s.count(OLD_TP) == 1, 'tprod decl not unique'
s = s.replace(OLD_TP, '', 1)

out = s.replace('\n', '\r\n') if CRLF else s
io.open(DST, 'w', encoding='utf-8', newline='').write(out)
print('D37 %d bytes -> D38 %d bytes (+%d)  crlf=%s' % (orig_len, len(out), len(out) - orig_len, CRLF))

# ------------------------------------------------- 4) 自检
chk = io.open(DST, encoding='utf-8', newline='').read()
assert chk.count('static bool absSubMul1') == 1
assert chk.count('absSubMul1(dividend_span, divisor, qhat)') == 1
# absDivBasicCore 函数体内不得再引用 tprod (tprod 是别处也在用的局部名, 只能按函数体查)
body = chk[chk.index('static void absDivBasicCore'):]
body = body[:body.index('static void absInvNewton')]
assert 'tprod' not in body, 'tprod still referenced inside absDivBasicCore!'
assert 'absMul1(divisor' not in body and 'absSub(dividend_span' not in body
print('self-check ok (absDivBasicCore body clean, %d bytes)' % len(body))
