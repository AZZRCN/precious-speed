#!/usr/bin/env python3
"""任务 #4: fftMul 和 fftSqr 的 carry chain 向量化 (cvttpd_epi64 + AVX-512DQ)"""
PATH = r'd:\precious_speed\add.cpp'

with open(PATH, 'r', encoding='utf-8', newline='') as f:
    content = f.read()

# fftMul carry chain (在 fftMul 函数中, 用 v1)
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
            out[conv_len] = Limb(carry);

            if (out.size > conv_len + 1)
            {
                std::fill_n(out.ptr + conv_len + 1, out.size - conv_len - 1, Limb(0));
            }
#ifdef PROFILE_MUL
            auto _t_carry1 = std::chrono::high_resolution_clock::now();
            double _dt_fft = std::chrono::duration<double, std::milli>(_t_carry0 - _t_fft0).count();
            double _dt_carry = std::chrono::duration<double, std::milli>(_t_carry1 - _t_carry0).count();
            fprintf(stderr, "  [prof] fftMul: FFT=%.3fms CARRY=%.3fms conv_len=%zu fl=%zu\\n", _dt_fft, _dt_carry, conv_len, float_len);
#endif"""

new_fftMul = """            // AVX-512DQ: cvttpd_epi64 向量化 double->int64, 替代标量 +0.5 截断
            _mm256_zeroupper();
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
            out[conv_len] = Limb(carry);

            if (out.size > conv_len + 1)
            {
                std::fill_n(out.ptr + conv_len + 1, out.size - conv_len - 1, Limb(0));
            }
#ifdef PROFILE_MUL
            auto _t_carry1 = std::chrono::high_resolution_clock::now();
            double _dt_fft = std::chrono::duration<double, std::milli>(_t_carry0 - _t_fft0).count();
            double _dt_carry = std::chrono::duration<double, std::milli>(_t_carry1 - _t_carry0).count();
            fprintf(stderr, "  [prof] fftMul: FFT=%.3fms CARRY=%.3fms conv_len=%zu fl=%zu\\n", _dt_fft, _dt_carry, conv_len, float_len);
#endif"""

c1 = content.count(old_fftMul)
print(f'fftMul carry chain: found {c1}')
if c1 == 1:
    content = content.replace(old_fftMul, new_fftMul)
    print('fftMul: DONE')

# fftSqr carry chain (用 v)
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
            out[conv_len] = Limb(carry);

            if (out.size > conv_len + 1)
            {
                std::fill_n(out.ptr + conv_len + 1, out.size - conv_len - 1, Limb(0));
            }
        }"""

new_fftSqr = """            // AVX-512DQ: cvttpd_epi64 向量化
            _mm256_zeroupper();
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
            out[conv_len] = Limb(carry);

            if (out.size > conv_len + 1)
            {
                std::fill_n(out.ptr + conv_len + 1, out.size - conv_len - 1, Limb(0));
            }
        }"""

c2 = content.count(old_fftSqr)
print(f'fftSqr carry chain: found {c2}')
if c2 == 1:
    content = content.replace(old_fftSqr, new_fftSqr)
    print('fftSqr: DONE')

with open(PATH, 'w', encoding='utf-8', newline='') as f:
    f.write(content)

print('\n=== fftMul/fftSqr carry chain replacement complete ===')
