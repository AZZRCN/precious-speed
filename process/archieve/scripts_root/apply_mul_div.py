#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""将add.cpp中的超优化实现应用到mul.cpp和div.cpp"""
import sys

def read_file(path):
    with open(path, 'rb') as f:
        data = f.read()
    # 统一为LF处理
    return data.replace(b'\r\n', b'\n').decode('utf-8')

def write_file(path, content):
    # 转回CRLF写回
    data = content.encode('utf-8').replace(b'\n', b'\r\n')
    with open(path, 'wb') as f:
        f.write(data)

# =============================================================================
# 替换1: absSub_avx2 直接store优化
# =============================================================================
SUB_OLD = """                alignas(32) uint32_t tmp[8];
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
                __m256i out_vec = _mm256_load_si256(reinterpret_cast<const __m256i *>(tmp));
                _mm256_storeu_si256(reinterpret_cast<__m256i *>(out.ptr + i), out_vec);"""

SUB_NEW = """                _mm256_storeu_si256(reinterpret_cast<__m256i *>(out.ptr + i), r);
                // 串行 borrow 传播: 8 步, 每步处理 2 个 limb (lo/hi), 无分支掩码
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
                borrow = static_cast<Limb>(bw);"""

# =============================================================================
# 替换2: absMul1 SIMD向量化
# =============================================================================
MUL1_OLD = """        static Limb absMul1(View in, Limb x, Span out)
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

MUL1_NEW = """        static Limb absMul1(View in, Limb x, Span out)
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

# =============================================================================
# 替换3: absDiv1 SIMD搬运 + 64位Barrett
# =============================================================================
DIV1_OLD = """        static Limb absDiv1(View in, Limb x, Span out)
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

DIV1_NEW = """        static Limb absDiv1(View in, Limb x, Span out)
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

# =============================================================================
# 替换4: absDivBasicCore qhat优化
# =============================================================================
DIVBC_OLD = """            Limb divisor_high = divisor[len2 - 1];
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
                }"""

DIVBC_NEW = """            Limb divisor_high = divisor[len2 - 1];
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
                }"""

# =============================================================================
# 替换5a: fftMul carry chain 向量化 (v1版本)
# =============================================================================
FFT_MUL_OLD = """            uint64_t carry = 0;
            size_t i = 0;
            for (; i + 7 < conv_len; i += 8)
            {
                // __builtin_prefetch: v1[] can be 4MB (500k MUL), exceeds L2 (1MB/core).
                //   Prefetch 2 batches ahead (128 bytes) to overlap L3 latency (~40 cycles)
                //   with carry chain serial dependency (~24 cycles per 8× iteration).
                HINT_PREFETCH(v1 + i + 16, 0, 0);
                HINT_PREFETCH(v1 + i + 24, 0, 0);
                // Barrett: q=divBASE(s); out=s-q*BASE; next_s=q+v[i+1]
                // was: s1 = s0 / BASE + ...; out[i] = s0 % BASE; carry = s7 / BASE;
                uint64_t s0 = carry + uint64_t(v1[i]   + 0.5);
                uint64_t q0 = divBASE(s0);
                uint64_t s1 = q0 + uint64_t(v1[i+1] + 0.5);
                uint64_t q1 = divBASE(s1);
                uint64_t s2 = q1 + uint64_t(v1[i+2] + 0.5);
                uint64_t q2 = divBASE(s2);
                uint64_t s3 = q2 + uint64_t(v1[i+3] + 0.5);
                uint64_t q3 = divBASE(s3);
                uint64_t s4 = q3 + uint64_t(v1[i+4] + 0.5);
                uint64_t q4 = divBASE(s4);
                uint64_t s5 = q4 + uint64_t(v1[i+5] + 0.5);
                uint64_t q5 = divBASE(s5);
                uint64_t s6 = q5 + uint64_t(v1[i+6] + 0.5);
                uint64_t q6 = divBASE(s6);
                uint64_t s7 = q6 + uint64_t(v1[i+7] + 0.5);
                uint64_t q7 = divBASE(s7);
                out[i]   = Limb(s0 - q0 * BASE);
                out[i+1] = Limb(s1 - q1 * BASE);
                out[i+2] = Limb(s2 - q2 * BASE);
                out[i+3] = Limb(s3 - q3 * BASE);
                out[i+4] = Limb(s4 - q4 * BASE);
                out[i+5] = Limb(s5 - q5 * BASE);
                out[i+6] = Limb(s6 - q6 * BASE);
                out[i+7] = Limb(s7 - q7 * BASE);
                carry = q7;
            }"""

FFT_MUL_NEW = """            // AVX-512DQ: cvttpd_epi64 向量化 double->int64
            uint64_t carry = 0;
            size_t i = 0;
            const __m256d vhalf = _mm256_set1_pd(0.5);
            for (; i + 7 < conv_len; i += 8)
            {
                HINT_PREFETCH(v1 + i + 16, 0, 0);
                HINT_PREFETCH(v1 + i + 24, 0, 0);
                __m256d vlow  = _mm256_loadu_pd(v1 + i);
                __m256d vhigh = _mm256_loadu_pd(v1 + i + 4);
                vlow  = _mm256_add_pd(vlow,  vhalf);
                vhigh = _mm256_add_pd(vhigh, vhalf);
                __m256i i64low  = _mm256_cvttpd_epi64(vlow);
                __m256i i64high = _mm256_cvttpd_epi64(vhigh);
                alignas(32) int64_t vals[8];
                _mm256_store_si256(reinterpret_cast<__m256i*>(vals),     i64low);
                _mm256_store_si256(reinterpret_cast<__m256i*>(vals + 4), i64high);
                for (int k = 0; k < 8; ++k) {
                    uint64_t uv = vals[k] < 0 ? 0 : (uint64_t)vals[k];
                    uint64_t s = carry + uv;
                    uint64_t q = divBASE(s);
                    out[i + k] = Limb(s - q * BASE);
                    carry = q;
                }
            }"""

# =============================================================================
# 替换5b: fftSqr carry chain 向量化 (v版本)
# =============================================================================
FFT_SQR_OLD = """            uint64_t carry = 0;
            size_t i = 0;
            for (; i + 7 < conv_len; i += 8)
            {
                // __builtin_prefetch: overlap L3 latency with carry chain computation
                HINT_PREFETCH(v + i + 16, 0, 0);
                HINT_PREFETCH(v + i + 24, 0, 0);
                // Barrett: q=divBASE(s); out=s-q*BASE; next_s=q+v[i+1]
                // was: s1 = s0 / BASE + ...; out[i] = s0 % BASE; carry = s7 / BASE;
                uint64_t s0 = carry + uint64_t(v[i]   + 0.5);
                uint64_t q0 = divBASE(s0);
                uint64_t s1 = q0 + uint64_t(v[i+1] + 0.5);
                uint64_t q1 = divBASE(s1);
                uint64_t s2 = q1 + uint64_t(v[i+2] + 0.5);
                uint64_t q2 = divBASE(s2);
                uint64_t s3 = q2 + uint64_t(v[i+3] + 0.5);
                uint64_t q3 = divBASE(s3);
                uint64_t s4 = q3 + uint64_t(v[i+4] + 0.5);
                uint64_t q4 = divBASE(s4);
                uint64_t s5 = q4 + uint64_t(v[i+5] + 0.5);
                uint64_t q5 = divBASE(s5);
                uint64_t s6 = q5 + uint64_t(v[i+6] + 0.5);
                uint64_t q6 = divBASE(s6);
                uint64_t s7 = q6 + uint64_t(v[i+7] + 0.5);
                uint64_t q7 = divBASE(s7);
                out[i]   = Limb(s0 - q0 * BASE);
                out[i+1] = Limb(s1 - q1 * BASE);
                out[i+2] = Limb(s2 - q2 * BASE);
                out[i+3] = Limb(s3 - q3 * BASE);
                out[i+4] = Limb(s4 - q4 * BASE);
                out[i+5] = Limb(s5 - q5 * BASE);
                out[i+6] = Limb(s6 - q6 * BASE);
                out[i+7] = Limb(s7 - q7 * BASE);
                carry = q7;
            }"""

FFT_SQR_NEW = """            // AVX-512DQ: cvttpd_epi64 向量化
            uint64_t carry = 0;
            size_t i = 0;
            const __m256d vhalf = _mm256_set1_pd(0.5);
            for (; i + 7 < conv_len; i += 8)
            {
                HINT_PREFETCH(v + i + 16, 0, 0);
                HINT_PREFETCH(v + i + 24, 0, 0);
                __m256d vlow  = _mm256_loadu_pd(v + i);
                __m256d vhigh = _mm256_loadu_pd(v + i + 4);
                vlow  = _mm256_add_pd(vlow,  vhalf);
                vhigh = _mm256_add_pd(vhigh, vhalf);
                __m256i i64low  = _mm256_cvttpd_epi64(vlow);
                __m256i i64high = _mm256_cvttpd_epi64(vhigh);
                alignas(32) int64_t vals[8];
                _mm256_store_si256(reinterpret_cast<__m256i*>(vals),     i64low);
                _mm256_store_si256(reinterpret_cast<__m256i*>(vals + 4), i64high);
                for (int k = 0; k < 8; ++k) {
                    uint64_t uv = vals[k] < 0 ? 0 : (uint64_t)vals[k];
                    uint64_t s = carry + uv;
                    uint64_t q = divBASE(s);
                    out[i + k] = Limb(s - q * BASE);
                    carry = q;
                }
            }"""

# =============================================================================
# 替换5c: fftMulPre carry chain 向量化 (v版本, 无PROFILE)
# 注意: fftMulPre的carry chain与fftSqr结构相同，但前面没有PROFILE标记
# 需要单独匹配
# =============================================================================
FFT_PRE_OLD = """            uint64_t carry = 0;
            size_t i = 0;
            for (; i + 7 < conv_len; i += 8)
            {
                HINT_PREFETCH(v + i + 16, 0, 0);
                HINT_PREFETCH(v + i + 24, 0, 0);
                uint64_t s0 = carry + uint64_t(v[i]   + 0.5);
                uint64_t q0 = divBASE(s0);
                uint64_t s1 = q0 + uint64_t(v[i+1] + 0.5);
                uint64_t q1 = divBASE(s1);
                uint64_t s2 = q1 + uint64_t(v[i+2] + 0.5);
                uint64_t q2 = divBASE(s2);
                uint64_t s3 = q2 + uint64_t(v[i+3] + 0.5);
                uint64_t q3 = divBASE(s3);
                uint64_t s4 = q3 + uint64_t(v[i+4] + 0.5);
                uint64_t q4 = divBASE(s4);
                uint64_t s5 = q4 + uint64_t(v[i+5] + 0.5);
                uint64_t q5 = divBASE(s5);
                uint64_t s6 = q5 + uint64_t(v[i+6] + 0.5);
                uint64_t q6 = divBASE(s6);
                uint64_t s7 = q6 + uint64_t(v[i+7] + 0.5);
                uint64_t q7 = divBASE(s7);
                out[i]   = Limb(s0 - q0 * BASE);
                out[i+1] = Limb(s1 - q1 * BASE);
                out[i+2] = Limb(s2 - q2 * BASE);
                out[i+3] = Limb(s3 - q3 * BASE);
                out[i+4] = Limb(s4 - q4 * BASE);
                out[i+5] = Limb(s5 - q5 * BASE);
                out[i+6] = Limb(s6 - q6 * BASE);
                out[i+7] = Limb(s7 - q7 * BASE);
                carry = q7;
            }"""

FFT_PRE_NEW = """            // AVX-512DQ: cvttpd_epi64 向量化
            uint64_t carry = 0;
            size_t i = 0;
            const __m256d vhalf = _mm256_set1_pd(0.5);
            for (; i + 7 < conv_len; i += 8)
            {
                HINT_PREFETCH(v + i + 16, 0, 0);
                HINT_PREFETCH(v + i + 24, 0, 0);
                __m256d vlow  = _mm256_loadu_pd(v + i);
                __m256d vhigh = _mm256_loadu_pd(v + i + 4);
                vlow  = _mm256_add_pd(vlow,  vhalf);
                vhigh = _mm256_add_pd(vhigh, vhalf);
                __m256i i64low  = _mm256_cvttpd_epi64(vlow);
                __m256i i64high = _mm256_cvttpd_epi64(vhigh);
                alignas(32) int64_t vals[8];
                _mm256_store_si256(reinterpret_cast<__m256i*>(vals),     i64low);
                _mm256_store_si256(reinterpret_cast<__m256i*>(vals + 4), i64high);
                for (int k = 0; k < 8; ++k) {
                    uint64_t uv = vals[k] < 0 ? 0 : (uint64_t)vals[k];
                    uint64_t s = carry + uv;
                    uint64_t q = divBASE(s);
                    out[i + k] = Limb(s - q * BASE);
                    carry = q;
                }
            }"""

# =============================================================================
# 替换5d: fftMulModBm1Pre carry chain 向量化
# fftMulModBm1Pre使用m而不是conv_len，且变量名是v
# =============================================================================
FFT_MODBM1_PRE_OLD = """            // 进位传播 (cyclic mod B^m-1) — 同 fftMulModBm1
            uint64_t carry = 0;
            size_t i = 0;
            for (; i + 7 < m; i += 8)
            {
                // __builtin_prefetch: overlap L3 latency with carry chain computation
                HINT_PREFETCH(v + i + 16, 0, 0);
                HINT_PREFETCH(v + i + 24, 0, 0);
                uint64_t s0 = carry + uint64_t(v[i]   + 0.5);
                uint64_t q0 = divBASE(s0);
                uint64_t s1 = q0 + uint64_t(v[i+1] + 0.5);
                uint64_t q1 = divBASE(s1);
                uint64_t s2 = q1 + uint64_t(v[i+2] + 0.5);
                uint64_t q2 = divBASE(s2);
                uint64_t s3 = q2 + uint64_t(v[i+3] + 0.5);
                uint64_t q3 = divBASE(s3);
                uint64_t s4 = q3 + uint64_t(v[i+4] + 0.5);
                uint64_t q4 = divBASE(s4);
                uint64_t s5 = q4 + uint64_t(v[i+5] + 0.5);
                uint64_t q5 = divBASE(s5);
                uint64_t s6 = q5 + uint64_t(v[i+6] + 0.5);
                uint64_t q6 = divBASE(s6);
                uint64_t s7 = q6 + uint64_t(v[i+7] + 0.5);
                uint64_t q7 = divBASE(s7);
                out[i]   = Limb(s0 - q0 * BASE);
                out[i+1] = Limb(s1 - q1 * BASE);
                out[i+2] = Limb(s2 - q2 * BASE);
                out[i+3] = Limb(s3 - q3 * BASE);
                out[i+4] = Limb(s4 - q4 * BASE);
                out[i+5] = Limb(s5 - q5 * BASE);
                out[i+6] = Limb(s6 - q6 * BASE);
                out[i+7] = Limb(s7 - q7 * BASE);
                carry = q7;
            }"""

FFT_MODBM1_PRE_NEW = """            // 进位传播 (cyclic mod B^m-1) — 同 fftMulModBm1
            // AVX-512DQ: cvttpd_epi64 向量化
            uint64_t carry = 0;
            size_t i = 0;
            const __m256d vhalf = _mm256_set1_pd(0.5);
            for (; i + 7 < m; i += 8)
            {
                HINT_PREFETCH(v + i + 16, 0, 0);
                HINT_PREFETCH(v + i + 24, 0, 0);
                __m256d vlow  = _mm256_loadu_pd(v + i);
                __m256d vhigh = _mm256_loadu_pd(v + i + 4);
                vlow  = _mm256_add_pd(vlow,  vhalf);
                vhigh = _mm256_add_pd(vhigh, vhalf);
                __m256i i64low  = _mm256_cvttpd_epi64(vlow);
                __m256i i64high = _mm256_cvttpd_epi64(vhigh);
                alignas(32) int64_t vals[8];
                _mm256_store_si256(reinterpret_cast<__m256i*>(vals),     i64low);
                _mm256_store_si256(reinterpret_cast<__m256i*>(vals + 4), i64high);
                for (int k = 0; k < 8; ++k) {
                    uint64_t uv = vals[k] < 0 ? 0 : (uint64_t)vals[k];
                    uint64_t s = carry + uv;
                    uint64_t q = divBASE(s);
                    out[i + k] = Limb(s - q * BASE);
                    carry = q;
                }
            }"""

# =============================================================================
# 替换5e: fftMulModBm1 carry chain 向量化 (使用va代替v)
# fftMulModBm1在add.cpp中未向量化，保持原样
# =============================================================================

# 所有替换对 (fftSqr carry 同时匹配 fftSqr 和 fftMulPre，两者向量化内容相同)
REPLACEMENTS = [
    ("absSub_avx2", SUB_OLD, SUB_NEW),
    ("absMul1", MUL1_OLD, MUL1_NEW),
    ("absDiv1", DIV1_OLD, DIV1_NEW),
    ("absDivBasicCore", DIVBC_OLD, DIVBC_NEW),
    ("fftMul carry", FFT_MUL_OLD, FFT_MUL_NEW),
    ("fftSqr+fftMulPre carry", FFT_SQR_OLD, FFT_SQR_NEW),
    ("fftMulModBm1Pre carry", FFT_MODBM1_PRE_OLD, FFT_MODBM1_PRE_NEW),
]

def apply(path):
    print(f"\n=== Processing {path} ===")
    content = read_file(path)
    fail_count = 0
    replaced_count = 0
    for name, old, new in REPLACEMENTS:
        count = content.count(old)
        if count == 0:
            # 尝试检查是否已经替换过
            if new in content:
                print(f"  [SKIP] {name}: already optimized")
            else:
                print(f"  [FAIL] {name}: pattern not found")
                fail_count += 1
        elif count == 1:
            content = content.replace(old, new)
            print(f"  [ OK ] {name}: replaced (1)")
            replaced_count += 1
        else:
            content = content.replace(old, new)
            print(f"  [ OK ] {name}: replaced ({count} occurrences)")
            replaced_count += count
    
    # 即使部分失败也写入已成功的替换
    if replaced_count > 0:
        write_file(path, content)
        print(f"  Written: {path} ({replaced_count} replacements)")
    else:
        print(f"  No replacements made")
    
    if fail_count > 0:
        print(f"  WARNING: {fail_count} patterns not found")
    return fail_count == 0

if __name__ == '__main__':
    ok = True
    for f in [r'd:\precious_speed\mul.cpp', r'd:\precious_speed\div.cpp']:
        ok &= apply(f)
    if ok:
        print("\n=== ALL DONE ===")
    else:
        print("\n=== SOME FAILED ===")
        sys.exit(1)
