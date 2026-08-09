# -*- coding: utf-8 -*-
"""D37 = D36 + basicMul 内层 addmul_1 的 SWAR/AVX2 向量化。

背景 (callgrind, D34):
  basicMul  bz_02 43.4M Ir = 14.27% (#1 热点),  rnz_01 36.6M = 12.83% (#3)
  它的内层是经典 addmul_1:  acc[j] += in[j]*x
  原实现 100% 标量: 每 limb = 1 imul + 2 add + 一次 magic 除法(imul+shr)
  + imul + sub + load/store ~= 11 Ir, 且带一条串行进位链。
  而同构的 mul_1 早在 D19 就被向量化成 ~3.4 Ir/limb (absMul1) —— 这个坑一直没补。

向量化推导:
  p[j] = in[j]*x < BASE^2                      (无依赖, 8 路 32 位)
  q[j] = p/BASE, r[j] = p%BASE                 (magic 乘法倒数, 同 absMul1)
  t[j] = r[j] + q[j-1] + acc[j] <= 3*BASE-3 = 29997 < 32768   (16 位安全)
  g[j] = floor(t/BASE) in {0,1,2},  m[j] = t - g*BASE in [0,BASE)
  真值递推:  out[j] = (m[j] + d[j]) mod BASE,  d[j+1] = g[j] + [m[j]+d[j] >= BASE]
  * 关键: [m+d >= BASE] 要求 m 顶到 BASE-1/BASE-2, 概率 ~1.4e-4。
    快路径直接令该项恒 0 => d[j] = g[j-1], 即"进位 = g 右移一格",
    完全没有串行链; 再用一次 vptest 检测是否真出现 u>=BASE (命中率 ~0.2%/块),
    命中时该 16-limb 块退标量重算 (逐字节等价), 期望每次 basicMul 触发 ~1 次。
  * 块间进位用完整标量 carry (= q[15]+g[15] <= BASE-1), 插进下一块的 qsh[0]。

顺带 (安全的去零化): buf 被逐行完整覆写 (第 0 行写 [0,n2], 第 i 行写 [i,i+n2]),
覆盖 [0, n1+n2-1] 全域, 故 resize -> reserve 安全。

预期: basicMul 内层 ~11 Ir/limb -> ~3.3 Ir/limb (约 3.3x)。
"""
import io

SRC = r'D:\precious_speed\best\div_D36.cpp'
DST = r'D:\precious_speed\best\div_D37.cpp'

s = io.open(SRC, 'r', encoding='utf-8', errors='surrogateescape').read()
orig = s

# ---------------------------------------------------------------- 1) 新内核
ANCHOR = """            return carry;
        }
        static Limb absDiv1(View in, Limb x, Span out)
"""
assert s.count(ANCHOR) == 1, 'absMul1 tail anchor not unique: %d' % s.count(ANCHOR)

KERNEL = r"""            return carry;
        }
        // === D37: SWAR/AVX2 addmul_1  ——  acc[] += in[] * x, 返回进位 ===
        // basicMul 的内层原本 100% 标量 (bz_02 #1 热点 14.27%, rnz_01 #3 12.83%),
        // 每 limb ~11 Ir 且带串行进位链。本函数按与 D19 absMul1 同样的思路拆解,
        // 但多一路累加数 acc:
        //   p[j] = in[j]*x < BASE^2                       (无依赖, 8 路 32 位)
        //   q[j] = p/BASE, r[j] = p%BASE                  (magic 乘法倒数)
        //   t[j] = r[j] + q[j-1] + acc[j] <= 3*BASE-3 = 29997 < 32768  (16 位安全)
        //   g[j] = floor(t/BASE) in {0,1,2},  m[j] = t - g*BASE in [0,BASE)
        // 真值递推:
        //   out[j] = (m[j] + d[j]) mod BASE,  d[j+1] = g[j] + [m[j]+d[j] >= BASE]
        // ★ [m+d >= BASE] 需要 m 顶到 BASE-1/BASE-2, 概率 ~1.4e-4 =>
        //   快路径令该项恒 0, 于是 d[j] = g[j-1] —— 进位退化成"g 右移一格",
        //   完全消除串行链; 再用一次 vptest 检测是否真有 u >= BASE
        //   (命中率 ~0.2%/块), 命中则该块退标量重算, 逐字节等价。
        // 块间用完整标量 carry (= q[15] + g[15] <= BASE-1) 插进下一块 qsh[0]。
        static Limb absAddMul1(View in, Limb x, Span acc)
        {
            static_assert(sizeof(Limb) == 2 && BASE == 10000,
                          "absAddMul1 AVX2 path assumes uint16 limbs in base 1e4");
            size_t i = 0;
            uint32_t carry = 0; // 进入 limb i 的完整进位, <= BASE-1
            if (in.size >= 16)
            {
                const __m256i vx = _mm256_set1_epi32(int(uint32_t(x)));
                const __m256i vmagic = _mm256_set1_epi32(109951163); // ceil(2^40 / 1e4)
                const __m256i vbase32 = _mm256_set1_epi32(int(BASE));
                const __m256i vbase16 = _mm256_set1_epi16(short(BASE));
                const __m256i vbm1 = _mm256_set1_epi16(short(BASE - 1));
                const __m256i v2bm1 = _mm256_set1_epi16(short(2 * BASE - 1));
                const __m256i vzero = _mm256_setzero_si256();
                for (; i + 15 < in.size; i += 16)
                {
                    __m256i a16 = _mm256_loadu_si256(
                        reinterpret_cast<const __m256i *>(in.ptr + i));
                    __m256i a0 = _mm256_cvtepu16_epi32(_mm256_castsi256_si128(a16));
                    __m256i a1 = _mm256_cvtepu16_epi32(_mm256_extracti128_si256(a16, 1));
                    __m256i p0 = _mm256_mullo_epi32(a0, vx); // p < 1e8, 32 位无溢出
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
                    // qsh[j] = q[j-1]; permute2x128(...,0x08) 把低 128 位清零,
                    // 故 alignr 之后 qsh[0] 恒为 0 —— 可以直接 OR 进块间 carry。
                    __m256i qsh = _mm256_alignr_epi8(
                        q16, _mm256_permute2x128_si256(q16, q16, 0x08), 14);
                    qsh = _mm256_or_si256(
                        qsh, _mm256_castsi128_si256(_mm_cvtsi32_si128(int(carry))));
                    uint32_t q_last = uint32_t(uint16_t(_mm256_extract_epi16(q16, 15)));
                    __m256i b16 = _mm256_loadu_si256(
                        reinterpret_cast<const __m256i *>(acc.ptr + i));
                    __m256i t = _mm256_add_epi16(_mm256_add_epi16(r16, qsh), b16);
                    __m256i ge1 = _mm256_cmpgt_epi16(t, vbm1);  // t >= BASE
                    __m256i ge2 = _mm256_cmpgt_epi16(t, v2bm1); // t >= 2*BASE
                    // g = (-ge1) + (-ge2) in {0,1,2}
                    __m256i g = _mm256_sub_epi16(_mm256_sub_epi16(vzero, ge1), ge2);
                    __m256i m = _mm256_sub_epi16(t, _mm256_mullo_epi16(g, vbase16));
                    // gsh[j] = g[j-1], gsh[0] = 0 (块间进位已并入 qsh[0])
                    __m256i gsh = _mm256_alignr_epi8(
                        g, _mm256_permute2x128_si256(g, g, 0x08), 14);
                    __m256i u = _mm256_add_epi16(m, gsh); // <= BASE+1
                    __m256i chk = _mm256_cmpgt_epi16(u, vbm1);
                    if (_mm256_testz_si256(chk, chk))
                    {
                        uint32_t g_last =
                            uint32_t(uint16_t(_mm256_extract_epi16(g, 15)));
                        _mm256_storeu_si256(
                            reinterpret_cast<__m256i *>(acc.ptr + i), u);
                        carry = q_last + g_last; // <= BASE-1
                    }
                    else
                    {
                        // 罕见 (~0.2%/块): 本块真出现 m+d >= BASE 的涟漪, 退标量重算
                        uint32_t c = carry;
                        for (size_t j = i; j < i + 16; j++)
                        {
                            uint32_t v = uint32_t(in[j]) * uint32_t(x)
                                       + uint32_t(acc[j]) + c;
                            acc[j] = Limb(v % BASE);
                            c = v / BASE;
                        }
                        carry = c;
                    }
                }
            }
            for (; i < in.size; i++)
            {
                uint32_t v = uint32_t(in[i]) * uint32_t(x) + uint32_t(acc[i]) + carry;
                acc[i] = Limb(v % BASE);
                carry = v / BASE;
            }
            return Limb(carry);
        }
        static Limb absDiv1(View in, Limb x, Span out)
"""
s = s.replace(ANCHOR, KERNEL, 1)

# ---------------------------------------------------------------- 2) basicMul
OLD_MUL = """            thread_local std::vector<Limb> buf;
            size_t buf_size = in1.size + in2.size;
            if (buf.size() < buf_size)
                buf.resize(buf_size);
            Limb carry = 0, x = in1[0];
            for (size_t j = 0; j < in2.size; j++)
            {
                Limb2 prod = Limb2(in2[j]) * x + carry;
                buf[j] = prod % BASE;
                carry = prod / BASE;
            }
            buf[in2.size] = carry;
            for (size_t i = 1; i < in1.size; i++)
            {
                x = in1[i], carry = 0;
                for (size_t j = 0; j < in2.size; j++)
                {
                    Limb2 prod = Limb2(in2[j]) * x + carry + buf[i + j];
                    buf[i + j] = prod % BASE;
                    carry = prod / BASE;
                }
                buf[i + in2.size] = carry;
            }
            std::copy(buf.begin(), buf.begin() + buf_size, out.begin());
"""
assert s.count(OLD_MUL) == 1, 'basicMul body not unique: %d' % s.count(OLD_MUL)

NEW_MUL = """            // D37: 内层改用向量化 mul_1 / addmul_1。
            // buf 被逐行完整覆写 (行 0 写 [0,n2], 行 i 写 [i,i+n2]), 覆盖
            // [0, n1+n2-1] 全域, 故 resize -> reserve (省 memset) 安全。
            thread_local std::vector<Limb> buf;
            size_t buf_size = in1.size + in2.size;
            if (buf.capacity() < buf_size)
                buf.reserve(buf_size);
            Limb *bp = buf.data();
            // 第 0 行: 纯 mul_1, 直接复用 D19 已向量化的 absMul1
            bp[in2.size] = absMul1(in2, in1[0], Span(bp, in2.size));
            // 其余各行: addmul_1
            for (size_t i = 1; i < in1.size; i++)
            {
                bp[i + in2.size] = absAddMul1(in2, in1[i], Span(bp + i, in2.size));
            }
            std::copy_n(bp, buf_size, out.begin());
"""
s = s.replace(OLD_MUL, NEW_MUL, 1)

assert s != orig
io.open(DST, 'w', encoding='utf-8', errors='surrogateescape').write(s)
print('wrote %s  (%d -> %d bytes)' % (DST, len(orig), len(s)))
