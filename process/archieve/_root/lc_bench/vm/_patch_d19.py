# -*- coding: utf-8 -*-
# D19 = D17 + 向量化 absMul1 + 向量化 count_true_length
import io, sys

p = 'D:/precious_speed/best/div_D19.cpp'
s = io.open(p, encoding='utf-8').read()

# ---------------- 1) 向量化 count_true_length ----------------
a = """    template <typename T>
    constexpr size_t count_true_length(const T array[], size_t length)
    {
        if (nullptr == array)
        {
            return 0;
        }
        // __builtin_expect: trailing zeros are rare for most inputs \u2014 hint unlikely
        while (length > 0 && HINT_UNLIKELY(array[length - 1] == 0))
        {
            length--;
        }
        return length;
    }"""
b = """    // D19: the scalar back-scan costs ~3 Ir per zero limb and shows up as 4.9M Ir
    // (100% scalar) inside removeLeadingZero on length_ratio_integer_03, because
    // the mu-division block loop keeps producing operands with long zero tails.
    // vptest clears 16 limbs at a time instead, ~0.25 Ir per limb.
    template <typename T>
    constexpr size_t count_true_length(const T array[], size_t length)
    {
        if (nullptr == array)
        {
            return 0;
        }
        if constexpr (sizeof(T) == 2)
        {
            if (!std::is_constant_evaluated())
            {
                while (length >= 16)
                {
                    __m256i v = _mm256_loadu_si256(
                        reinterpret_cast<const __m256i *>(array + length - 16));
                    if (!_mm256_testz_si256(v, v))
                    {
                        break;
                    }
                    length -= 16;
                }
            }
        }
        // __builtin_expect: trailing zeros are rare for most inputs \u2014 hint unlikely
        while (length > 0 && HINT_UNLIKELY(array[length - 1] == 0))
        {
            length--;
        }
        return length;
    }"""
assert s.count(a) == 1, ('count_true_length', s.count(a))
s = s.replace(a, b)

# ---------------- 2) 向量化 absMul1 ----------------
a = """        static Limb absMul1(View in, Limb x, Span out)
        {
            Limb carry = 0;
            for (size_t i = 0; i < in.size; i++)
            {
                Limb2 prod = Limb2(in[i]) * x + carry;
                out[i] = prod % BASE;
                carry = prod / BASE;
            }
            return carry;
        }"""
b = """        // === D19: SWAR \u8fdb\u4f4d\u94fe absMul1 (single-limb multiply) ===
        // \u539f\u7248\u662f\u7eaf\u6807\u91cf\u8fdb\u4f4d\u94fe: \u6bcf limb \u4e00\u6b21 imul + \u4e24\u6b21 magic \u9664\u6cd5 (~26 Ir/limb),
        // \u5728 lri_03 \u7684\u5f52\u4e00\u5316\u8def\u5f84 (dividend/divisor \u5404\u4e58 factor, \u5171 50 \u4e07 limb)
        // \u6298\u7b97 13.0M Ir, \u662f\u753b\u50cf\u91cc\u6700\u5927\u7684 100% \u6807\u91cf\u5757\u3002
        //
        // \u62c6\u89e3\u4e32\u884c\u4f9d\u8d56:  prod[i] = p[i] + carry[i],  p[i] = in[i]*x  (< BASE^2, \u65e0\u4f9d\u8d56)
        //   \u8bb0 p[i] = q[i]*BASE + r[i], \u5219
        //     out[i]     = (r[i] + carry[i]) mod BASE
        //     carry[i+1] = q[i] + [r[i] + carry[i] >= BASE]
        //   \u7531 q[i] <= BASE-2 \u4fdd\u8bc1 carry[i] <= BASE-1, \u6545 r[i]+carry[i] < 2*BASE,
        //   \u6d9f\u6f2a\u8fdb\u4f4d\u81f3\u591a 1 \u2014\u2014 \u4e8e\u662f t[i] = r[i] + q[i-1] \u53ef\u6574\u4f53\u5411\u91cf\u5316,
        //   \u53ea\u5269\u4e0b\u8fd9\u6761\u81f3\u591a-1 \u7684\u6d9f\u6f2a\u94fe, \u7528 D11 \u7684 SWAR \u4e00\u6b21\u7b97\u5b8c 16 limb:
        //     G = (t >= BASE) \u751f\u6210, P = (t == BASE-1) \u4f20\u64ad (\u4e8c\u8005\u4e92\u65a5),
        //     s = G + (G|P) + cin,  cin_bits = s ^ G ^ (G|P),  cout = cin_bits >> 1
        static Limb absMul1(View in, Limb x, Span out)
        {
            static_assert(sizeof(Limb) == 2 && BASE == 10000,
                          "absMul1 AVX2 path assumes uint16 limbs in base 1e4");
            size_t i = 0;
            uint32_t carry_q = 0;   // \u4e0a\u4e00 limb \u7684\u9ad8\u4f4d q
            uint32_t carry_rip = 0; // \u4e0a\u4e00 limb \u7684\u6d9f\u6f2a\u8fdb\u4f4d
            if (in.size >= 16)
            {
                const __m256i vx = _mm256_set1_epi32(int(uint32_t(x)));
                const __m256i vmagic = _mm256_set1_epi32(109951163); // ceil(2^40 / 1e4)
                const __m256i vbase32 = _mm256_set1_epi32(int(BASE));
                const __m256i vbase16 = _mm256_set1_epi16(short(BASE));
                const __m256i vbm1 = _mm256_set1_epi16(short(BASE - 1));
                const __m256i vbit = _mm256_setr_epi16(
                    1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192,
                    short(16384), short(32768));
                for (; i + 15 < in.size; i += 16)
                {
                    __m256i a16 = _mm256_loadu_si256(
                        reinterpret_cast<const __m256i *>(in.ptr + i));
                    __m256i a0 = _mm256_cvtepu16_epi32(_mm256_castsi256_si128(a16));
                    __m256i a1 = _mm256_cvtepu16_epi32(_mm256_extracti128_si256(a16, 1));
                    // p < (BASE-1)^2 < 1e8, 32 \u4f4d\u65e0\u6ea2\u51fa
                    __m256i p0 = _mm256_mullo_epi32(a0, vx);
                    __m256i p1 = _mm256_mullo_epi32(a1, vx);
                    // q = floor(p * ceil(2^40/BASE) / 2^40) \u2014\u2014 \u5bf9 p < 1e8 \u7cbe\u786e
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
                    // 32 -> 16 \u4f4d\u6253\u5305 (q, r \u5747\u5728 [0, BASE), \u4e0d\u4f1a\u9971\u548c)
                    __m256i q16 = _mm256_permute4x64_epi64(
                        _mm256_packus_epi32(q0, q1), 0xD8);
                    __m256i r16 = _mm256_permute4x64_epi64(
                        _mm256_packus_epi32(r0, r1), 0xD8);
                    // qsh[j] = q[j-1], \u6700\u4f4e\u4f4d\u586b\u5165\u4e0a\u4e00\u5757\u6b8b\u7559\u7684 carry_q
                    __m256i qsh = _mm256_alignr_epi8(
                        q16, _mm256_permute2x128_si256(q16, q16, 0x08), 14);
                    qsh = _mm256_insert_epi16(qsh, int(carry_q), 0);
                    carry_q = uint32_t(uint16_t(_mm256_extract_epi16(q16, 15)));
                    __m256i t = _mm256_add_epi16(r16, qsh); // < 2*BASE < 32768
                    __m256i gm = _mm256_cmpgt_epi16(t, vbm1); // t >= BASE
                    __m256i pm = _mm256_cmpeq_epi16(t, vbm1); // t == BASE-1
                    uint32_t G = _pext_u32(uint32_t(_mm256_movemask_epi8(gm)), 0x55555555u);
                    uint32_t P = _pext_u32(uint32_t(_mm256_movemask_epi8(pm)), 0x55555555u);
                    uint32_t A = G, B = G | P;
                    uint32_t sw = A + B + carry_rip;
                    uint32_t cin = sw ^ A ^ B; // bit j = \u8fdb\u5165 limb j \u7684\u8fdb\u4f4d
                    uint32_t cout = cin >> 1;  // bit j = limb j \u7684\u8fdb\u4f4d\u8f93\u51fa
                    carry_rip = (sw >> 16) & 1u;
                    __m256i vcin = _mm256_cmpeq_epi16(
                        _mm256_and_si256(_mm256_set1_epi16(short(cin)), vbit), vbit);
                    __m256i vcout = _mm256_cmpeq_epi16(
                        _mm256_and_si256(_mm256_set1_epi16(short(cout)), vbit), vbit);
                    __m256i res = _mm256_sub_epi16(
                        _mm256_add_epi16(t, _mm256_srli_epi16(vcin, 15)),
                        _mm256_and_si256(vcout, vbase16));
                    _mm256_storeu_si256(reinterpret_cast<__m256i *>(out.ptr + i), res);
                }
            }
            Limb carry = Limb(carry_q + carry_rip);
            for (; i < in.size; i++)
            {
                Limb2 prod = Limb2(in[i]) * x + carry;
                out[i] = prod % BASE;
                carry = prod / BASE;
            }
            return carry;
        }"""
assert s.count(a) == 1, ('absMul1', s.count(a))
s = s.replace(a, b)

io.open(p, 'w', encoding='utf-8', newline='').write(s)
print('D19 patched OK')
