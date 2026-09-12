#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""回退所有AVX-512DQ/VL指令为AVX2等价或原版标量"""
import sys

def read_file(path):
    with open(path, 'rb') as f:
        return f.read().replace(b'\r\n', b'\n').decode('utf-8')

def write_file(path, content):
    with open(path, 'wb') as f:
        f.write(content.encode('utf-8').replace(b'\n', b'\r\n'))

# =============================================================================
# 回退1: fftMul carry chain (cvttpd_epi64 → 原版标量)
# 匹配所有4个变体: fftMul(v1), fftSqr(v), fftMulPre(v), fftMulModBm1Pre(v, m)
# =============================================================================

# fftMul (使用 v1 和 conv_len)
FFT_MUL_VEC = """            // AVX-512DQ: cvttpd_epi64 向量化 double->int64
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

FFT_MUL_ORIG = """            uint64_t carry = 0;
            size_t i = 0;
            for (; i + 7 < conv_len; i += 8)
            {
                // __builtin_prefetch: v1[] can be 4MB (500k MUL), exceeds L2 (1MB/core).
                //   Prefetch 2 batches ahead (128 bytes) to overlap L3 latency (~40 cycles)
                //   with carry chain serial dependency (~24 cycles per 8x iteration).
                HINT_PREFETCH(v1 + i + 16, 0, 0);
                HINT_PREFETCH(v1 + i + 24, 0, 0);
                // Barrett: q=divBASE(s); out=s-q*BASE; next_s=q+v[i+1]
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

# fftSqr/fftMulPre (使用 v 和 conv_len)
FFT_SQR_VEC = """            // AVX-512DQ: cvttpd_epi64 向量化
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

FFT_SQR_ORIG = """            uint64_t carry = 0;
            size_t i = 0;
            for (; i + 7 < conv_len; i += 8)
            {
                // __builtin_prefetch: overlap L3 latency with carry chain computation
                HINT_PREFETCH(v + i + 16, 0, 0);
                HINT_PREFETCH(v + i + 24, 0, 0);
                // Barrett: q=divBASE(s); out=s-q*BASE; next_s=q+v[i+1]
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

# fftMulModBm1Pre (使用 v 和 m)
FFT_MODBM1_VEC = """            // 进位传播 (cyclic mod B^m-1) — 同 fftMulModBm1
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

FFT_MODBM1_ORIG = """            // 进位传播 (cyclic mod B^m-1) — 同 fftMulModBm1
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

# =============================================================================
# 回退2: absDiv1 的 _mm256_cvtepi32_epi16 → 标量 store
# =============================================================================
DIV1_CVT_VEC = """                __m256i r32 = _mm256_load_si256(reinterpret_cast<const __m256i*>(tmp_out));
                __m128i r16 = _mm256_cvtepi32_epi16(r32);
                _mm_storeu_si128(reinterpret_cast<__m128i*>(out.ptr + i), r16);"""

DIV1_CVT_SCALAR = """                for (int k = 0; k < 8; ++k)
                    out.ptr[i + k] = Limb(tmp_out[k]);"""

# =============================================================================
# 回退3: add.cpp str32to8limbs 的 _mm256_cvtepi32_epi16 → AVX2 packus等价
# =============================================================================
# 需要先读取add.cpp中str32to8limbs的AVX-512VL版本，然后回退为AVX2版本
# 这个函数只在add.cpp中存在，mul/div没有

# 所有回退对
REVERTS = [
    ("fftMul carry", FFT_MUL_VEC, FFT_MUL_ORIG),
    ("fftSqr+fftMulPre carry", FFT_SQR_VEC, FFT_SQR_ORIG),
    ("fftMulModBm1Pre carry", FFT_MODBM1_VEC, FFT_MODBM1_ORIG),
    ("absDiv1 cvtepi32_epi16", DIV1_CVT_VEC, DIV1_CVT_SCALAR),
]

def apply(path):
    print(f"\n=== Processing {path} ===")
    content = read_file(path)
    reverted = 0
    for name, vec, orig in REVERTS:
        count = content.count(vec)
        if count == 0:
            if orig in content:
                print(f"  [SKIP] {name}: already original")
            else:
                print(f"  [FAIL] {name}: vectorized pattern not found")
        else:
            content = content.replace(vec, orig)
            print(f"  [ OK ] {name}: reverted ({count})")
            reverted += count

    # 特殊处理: add.cpp 的 str32to8limbs (只有add.cpp有)
    if 'add.cpp' in path or 'add' in path:
        # 读取str32to8limbs的AVX-512VL版本并回退
        # 需要单独处理
        pass

    if reverted > 0:
        write_file(path, content)
        print(f"  Written: {path} ({reverted} reverts)")
    else:
        print(f"  No changes")
    return True

if __name__ == '__main__':
    for f in [r'd:\precious_speed\mul.cpp', r'd:\precious_speed\div.cpp', r'd:\precious_speed\add.cpp']:
        apply(f)
