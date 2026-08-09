#!/usr/bin/env python3
"""任务 #4: fftMul 和 fftSqr carry chain 向量化"""
import sys

PATH = r'd:\precious_speed\add.cpp'

# 用 newline=None 读取, \r\n 会被转为 \n
with open(PATH, 'r', encoding='utf-8') as f:
    content = f.read()

def rep(old, new, desc):
    global content
    c = content.count(old)
    print(f'[{desc}] found: {c}')
    if c == 1:
        content = content.replace(old, new)
        print(f'[{desc}] DONE')
        return True
    elif c > 1:
        print(f'[{desc}] MULTIPLE: {c}')
    else:
        print(f'[{desc}] NOT FOUND')
    return False

# fftMul carry chain
old_fftMul = """            uint64_t carry = 0;
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
            }
            for (; i < conv_len; i++)
            {
                carry += uint64_t(v1[i] + 0.5);
                uint64_t q = divBASE(carry);
                out[i] = Limb(carry - q * BASE);
                carry = q;
            }
            out[conv_len] = Limb(carry);"""

new_fftMul = """            // AVX-512DQ: cvttpd_epi64 向量化 double->int64
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
            }
            for (; i < conv_len; i++)
            {
                carry += uint64_t(v1[i] + 0.5);
                uint64_t q = divBASE(carry);
                out[i] = Limb(carry - q * BASE);
                carry = q;
            }
            out[conv_len] = Limb(carry);"""

rep(old_fftMul, new_fftMul, 'fftMul carry')

# fftSqr carry chain
old_fftSqr = """            uint64_t carry = 0;
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
            }
            for (; i < conv_len; i++)
            {
                carry += uint64_t(v[i] + 0.5);
                uint64_t q = divBASE(carry);
                out[i] = Limb(carry - q * BASE);
                carry = q;
            }
            out[conv_len] = Limb(carry);"""

new_fftSqr = """            // AVX-512DQ: cvttpd_epi64 向量化
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
            }
            for (; i < conv_len; i++)
            {
                carry += uint64_t(v[i] + 0.5);
                uint64_t q = divBASE(carry);
                out[i] = Limb(carry - q * BASE);
                carry = q;
            }
            out[conv_len] = Limb(carry);"""

rep(old_fftSqr, new_fftSqr, 'fftSqr carry')

# 写入 (newline=None 自动转 \r\n)
with open(PATH, 'w', encoding='utf-8') as f:
    f.write(content)

print('\n=== Done ===')
