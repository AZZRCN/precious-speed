#!/usr/bin/env python3
"""对 add.cpp 应用 SIMD 超优化 (5 个任务)"""
import sys

PATH = r'd:\precious_speed\add.cpp'

with open(PATH, 'rb') as f:
    content = f.read()

def replace_once(old, new, desc):
    global content
    old_b = old.encode('utf-8') if isinstance(old, str) else old
    new_b = new.encode('utf-8') if isinstance(new, str) else new
    # 统一为 CRLF
    old_b = old_b.replace(b'\n', b'\r\n').replace(b'\r\r\n', b'\r\n')
    new_b = new_b.replace(b'\n', b'\r\n').replace(b'\r\r\n', b'\r\n')
    count = content.count(old_b)
    print(f'[{desc}] found: {count}')
    if count == 1:
        content = content.replace(old_b, new_b)
        print(f'[{desc}] DONE')
        return True
    elif count == 0:
        print(f'[{desc}] NOT FOUND')
        return False
    else:
        print(f'[{desc}] MULTIPLE MATCHES: {count}')
        return False

# ===== 任务 1: absSub_avx2 直接 store =====
old1 = """                __m256i r = _mm256_sub_epi32(_mm256_add_epi32(a, bias_vec), b);
                alignas(32) uint32_t tmp[8];
                _mm256_store_si256(reinterpret_cast<__m256i *>(tmp), r);
                // 串行 borrow 传播: 8 步, 每步处理 2 个 limb (lo/hi), 无分支掩码
                uint32_t bw = borrow;
                for (int k = 0; k < 8; k++)
                {
                    uint32_t lo = tmp[k] & 0xFFFF;
                    uint32_t hi = tmp[k] >> 16;
                    lo -= bw;
                    uint32_t blo = lo < 10000;
                    lo -= (1u - blo) * 10000u;
                    hi -= blo;
                    uint32_t bhi = hi < 10000;
                    hi -= (1u - bhi) * 10000u;
                    bw = bhi;
                    tmp[k] = lo | (hi << 16);
                }
                borrow = static_cast<Limb>(bw);
                __m256i out_vec = _mm256_load_si256(reinterpret_cast<__m256i *>(tmp));
                _mm256_storeu_si256(reinterpret_cast<__m256i *>(out.ptr + i), out_vec);
            }"""

new1 = """                __m256i r = _mm256_sub_epi32(_mm256_add_epi32(a, bias_vec), b);
                // 直接 store 到 out, 省去 tmp 缓冲区 (absSub 直接 store 优化)
                _mm256_storeu_si256(reinterpret_cast<__m256i *>(out.ptr + i), r);
                uint32_t *p32 = reinterpret_cast<uint32_t *>(out.ptr + i);
                uint32_t bw = borrow;
                for (int k = 0; k < 8; k++)
                {
                    uint32_t lo = p32[k] & 0xFFFF;
                    uint32_t hi = p32[k] >> 16;
                    lo -= bw;
                    uint32_t blo = lo < 10000;
                    lo -= (1u - blo) * 10000u;
                    hi -= blo;
                    uint32_t bhi = hi < 10000;
                    hi -= (1u - bhi) * 10000u;
                    bw = bhi;
                    p32[k] = lo | (hi << 16);
                }
                borrow = static_cast<Limb>(bw);
            }"""

replace_once(old1, new1, 'absSub direct store')

# ===== 任务 2: absMul1 SIMD =====
old2 = """        static Limb absMul1(View in, Limb x, Span out)
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

new2 = """        static Limb absMul1(View in, Limb x, Span out)
        {
            if (x == 0) { std::memset(out.ptr, 0, in.size * sizeof(Limb)); return 0; }
            // SIMD 向量化: 16 limb 并行乘法 + 串行 carry 传播 (32 位 Barrett)
            static constexpr uint32_t BARRETT_M32 = 429497;  // ceil(2^32 / 10000)
            auto divBASE32 = [](uint64_t s, uint64_t &q) -> Limb {
                q = (s * BARRETT_M32) >> 32;
                if (q * BASE > s) q--;
                return Limb(s - q * BASE);
            };
            Limb carry = 0;
            size_t i = 0;
            const __m256i vx = _mm256_set1_epi32(x);
            for (; i + 15 < in.size; i += 16)
            {
                __m128i in_lo = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in.ptr + i));
                __m256i a0 = _mm256_cvtepu16_epi32(in_lo);
                __m128i in_hi = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in.ptr + i + 8));
                __m256i a1 = _mm256_cvtepu16_epi32(in_hi);
                __m256i p0 = _mm256_mullo_epi32(a0, vx);
                __m256i p1 = _mm256_mullo_epi32(a1, vx);
                alignas(32) uint32_t prods[16];
                _mm256_store_si256(reinterpret_cast<__m256i*>(prods),     p0);
                _mm256_store_si256(reinterpret_cast<__m256i*>(prods + 8), p1);
                for (int k = 0; k < 16; ++k) {
                    uint64_t s = uint64_t(prods[k]) + carry;
                    uint64_t q;
                    out[i + k] = divBASE32(s, q);
                    carry = Limb(q);
                }
            }
            for (; i < in.size; ++i) {
                uint64_t s = uint64_t(in[i]) * x + carry;
                uint64_t q;
                out[i] = divBASE32(s, q);
                carry = Limb(q);
            }
            return carry;
        }"""

replace_once(old2, new2, 'absMul1 SIMD')

# ===== 任务 3: absDiv1 SIMD =====
old3 = """        static Limb absDiv1(View in, Limb x, Span out)
        {
            Limb rem = 0;
            size_t i = in.size;
            while (i > 0)
            {
                i--;
                Limb2 prod = Limb2(in[i]) + Limb2(rem) * BASE;
                out[i] = prod / x;
                rem = prod % x;
            }
            return rem;
        }"""

new3 = """        static Limb absDiv1(View in, Limb x, Span out)
        {
            if (x == 1) {
                if (out.ptr != in.ptr) std::memcpy(out.ptr, in.ptr, in.size * sizeof(Limb));
                return 0;
            }
            // SIMD 搬运 + 64 位 Barrett (ceil 乘数 M = ceil(2^64/x))
            uint64_t M = (uint64_t)((((unsigned __int128)1 << 64) + x - 1) / x);
            Limb rem = 0;
            size_t i = in.size;
            while (i >= 8) {
                i -= 8;
                __m128i in16 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in.ptr + i));
                __m256i in32 = _mm256_cvtepu16_epi32(in16);
                alignas(32) uint32_t tmp_in[8], tmp_out[8];
                _mm256_store_si256(reinterpret_cast<__m256i*>(tmp_in), in32);
                for (int k = 7; k >= 0; --k) {
                    uint64_t prod = uint64_t(tmp_in[k]) + uint64_t(rem) * BASE;
                    uint64_t q = (uint64_t)((unsigned __int128)prod * M >> 64);
                    if ((uint64_t)q * x > prod) q--;
                    tmp_out[k] = (uint32_t)q;
                    rem = Limb(prod - q * x);
                }
                __m256i r32 = _mm256_load_si256(reinterpret_cast<const __m256i*>(tmp_out));
                __m128i r16 = _mm256_cvtepi32_epi16(r32);
                _mm_storeu_si128(reinterpret_cast<__m128i*>(out.ptr + i), r16);
            }
            while (i > 0) {
                --i;
                uint64_t prod = uint64_t(in[i]) + uint64_t(rem) * BASE;
                uint64_t q = (uint64_t)((unsigned __int128)prod * M >> 64);
                if ((uint64_t)q * x > prod) q--;
                out[i] = Limb(q);
                rem = Limb(prod - q * x);
            }
            return rem;
        }"""

replace_once(old3, new3, 'absDiv1 SIMD')

# ===== 任务 5: absDivBasicCore qhat 优化 =====
old5 = """        static void absDivBasicCore(Span dividend, View divisor, Span quotient)
        {
            if (dividend.size <= divisor.size)
            {
                return;
            }
            assert(divisor.size > 0);
            size_t len1 = dividend.size, len2 = divisor.size;
            Limb divisor_high = divisor[len2 - 1];
            assert(divisor_high >= HALF_BASE);
            size_t quot_idx = len1 - len2;
            
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
                dividend.size = len1;"""

new5 = """        static void absDivBasicCore(Span dividend, View divisor, Span quotient)
        {
            if (dividend.size <= divisor.size)
            {
                return;
            }
            assert(divisor.size > 0);
            size_t len1 = dividend.size, len2 = divisor.size;
            Limb divisor_high = divisor[len2 - 1];
            assert(divisor_high >= HALF_BASE);
            // qhat 优化: 用 ceil 乘数 M = ceil(2^64/divisor_high) 替代除法
            uint64_t M = (uint64_t)((((unsigned __int128)1 << 64) + divisor_high - 1) / divisor_high);
            size_t quot_idx = len1 - len2;
            
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
                    uint64_t high = (uint64_t)high1 * BASE + high2;
                    uint64_t q = (uint64_t)(((unsigned __int128)high * M) >> 64);
                    if (q >= BASE) q = BASE - 1;
                    if ((uint64_t)q * divisor_high > high) q--;
                    qhat = (Limb)q;
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
                dividend.size = len1;"""

replace_once(old5, new5, 'absDivBasicCore qhat')

with open(PATH, 'wb') as f:
    f.write(content)

print('\n=== All replacements complete ===')
