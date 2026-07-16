// https://judge.yosupo.jp/problem/division_of_big_integers
// code from https://github.com/With-Sky/HyperInt-mini
//
// moptm (masonxiong_opt mixed) - hybrid version combining:
//   * masonxiong_opt base (AVX2/FMA complex FFT multiplication, Newton-Raphson division)
//   * div_work Core2 chunked division path (ported from HyperInt-mini)
//
// Author: AZZRCN (qazwsx233343@163.com)
// GitHub: https://github.com/AZZRCN
//
// Core2 chunked division: when blocks >= 3 (len1 >= len2*3), divide the dividend
// into len2-sized blocks from high to low. Each block uses a precomputed inverse
// (precision 2*len2+5) to produce a qhat estimate, corrected by a small loop.
// Measured -19% to -35% vs single-shot Newton division when blocks >= 3.
// When blocks == 2, Core2 is 40% slower (two 2n*n mults vs one 2n*n), so the
// threshold len1 >= len2*3 keeps blocks==2 on the Core1 single-shot path.

/**
 * @file Integer.h
 * @brief Header file for an efficient C++ arbitrary-precision integer arithmetic library.
 *
 * This header provides high-performance arbitrary-precision integer types and related algorithms,
 * supporting construction, arithmetic operations, and input/output for both unsigned and signed big integers.
 * It is suitable for scenarios requiring large number computations.
 *
 * @author [masonxiong](https://www.luogu.com.cn/user/446979), [yuygfgg](https://www.luogu.com.cn/user/251551)
 * @date 2025-9-04
 *
 * Optimized by [AZZRCN](https://github.com/AZZRCN) (qazwsx233343@163.com)
 *   - AVX2/FMA-accelerated complex FFT multiplication (__m256d butterfly)
 *   - Dedicated square path (saves one DIF + half pointwise multiply)
 *   - Fused Newton-Raphson division (merge shift+double+subtract)
 *   - Unbalanced multiplication split with DIF cache reuse
 *   - Size-class memory pool to reduce malloc/free overhead
 *   - Core2 chunked division path ported from HyperInt-mini (blocks>=3, -19%~-35%)
 *   - 2x faster than original, 1.45-3.18x faster than GMP 6.3.0 at 1M scale
 */

#ifndef INTEGER_H
#define INTEGER_H 20250904L

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#ifdef ENABLE_VALIDITY_CHECK
#define VALIDITY_CHECK(condition, errorType, message) \
    if (!(condition)) throw errorType(message);
#else
#define VALIDITY_CHECK(condition, errorType, message)
#endif

#ifdef PROFILE_DIVISION
#include <chrono>
#include <cstdio>
namespace profile_div {
    inline double& timer(int i) {
        static double timers[10] = {};
        return timers[i];
    }
    inline int& counter(int i) {
        static int counters[10] = {};
        return counters[i];
    }
    inline const char* names[] = {
        "div_inv", "div_qmul", "div_rshift", "div_qother", "div_correct",
        "inv_rshift", "inv_recurse", "inv_sqr", "inv_mul", "inv_fuse"
    };
    inline void reset() { for (int i = 0; i < 10; ++i) { timer(i) = 0; counter(i) = 0; } }
    inline void dump() {
        double total = 0;
        for (int i = 0; i < 10; ++i) total += timer(i);
        for (int i = 0; i < 10; ++i) {
            if (counter(i)) printf("  %-14s %8.3f ms  (%d calls, %6.3f avg)\n", names[i], timer(i), counter(i), timer(i)/counter(i));
        }
        printf("  %-14s %8.3f ms\n", "TOTAL", total);
    }
    struct ScopeTimer {
        int idx;
        std::chrono::high_resolution_clock::time_point t0;
        ScopeTimer(int i) : idx(i), t0(std::chrono::high_resolution_clock::now()) {}
        ~ScopeTimer() { timer(idx) += std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - t0).count(); ++counter(idx); }
    };
}
#define DIV_PROFILE_CONCAT_(a, b) a##b
#define DIV_PROFILE_CONCAT(a, b) DIV_PROFILE_CONCAT_(a, b)
#define DIV_PROFILE(idx) profile_div::ScopeTimer DIV_PROFILE_CONCAT(_pt_, __COUNTER__)(idx)
#endif

#if defined(__AVX2__)
#include <immintrin.h>
#elif defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

#ifndef __CONSTEXPR
#ifdef _GLIBCXX14_CONSTEXPR
#define __CONSTEXPR _GLIBCXX14_CONSTEXPR
#else
#define __CONSTEXPR constexpr
#endif

#endif

#ifndef LIKELY
#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#endif

namespace detail {
    constexpr std::uint32_t log2(std::uint32_t n) {
        VALIDITY_CHECK(n, std::invalid_argument, "log2 error: the provided integer is zero.");
#if defined(__GNUC__) && !defined(__clang__)
        return std::__lg(n);
#else
        return std::log2(n);
#endif
    }

    struct InputHelper {
        std::uint32_t table[0x10000];

        __CONSTEXPR InputHelper() : table() {
            for (std::uint32_t i = 48; i != 58; ++i)
                for (std::uint32_t j = 48; j != 58; ++j)
                    table[i << 8 | j] = (j & 15) * 10 + (i & 15);
        }

        std::uint32_t operator()(const char* value) const {
            const std::uint32_t high = static_cast<std::uint8_t>(value[1]);
            const std::uint32_t low = static_cast<std::uint8_t>(value[0]);
            return table[(high << 8) | low];
        }
    };

    struct OutputHelper {
        std::uint32_t table[10000];

        __CONSTEXPR OutputHelper() : table() {
            for (std::uint32_t *i = table, a = 48; a != 58; ++a)
                for (std::uint32_t b = 48; b != 58; ++b)
                    for (std::uint32_t c = 48; c != 58; ++c)
                        for (std::uint32_t d = 48; d != 58; ++d)
                            *i++ = a | (b << 8) | (c << 16) | (d << 24);
        }

        const char* operator()(std::uint32_t value) const {
            return reinterpret_cast<const char*>(table + value);
        }
    };

#if defined(__AVX2__)
    struct TransformHelper {
        __m128d* twiddleFactors;
        std::uint32_t length;

        TransformHelper() : twiddleFactors(new __m128d[1]()), length(1) {
            *twiddleFactors = _mm_set_pd(0.0, 1.0);
        }

        ~TransformHelper() noexcept {
            delete[] twiddleFactors;
        }

        static inline __m128d complexMultiply(__m128d first, __m128d second) {
            return _mm_fmaddsub_pd(_mm_unpacklo_pd(first, first), second, _mm_unpackhi_pd(first, first) * _mm_permute_pd(second, 1));
        }

        static inline __m128d complexMultiplyConjugate(__m128d first, __m128d second) {
            return _mm_fmsubadd_pd(_mm_unpacklo_pd(second, second), first, _mm_unpackhi_pd(second, second) * _mm_permute_pd(first, 1));
        }

        static inline __m128d complexMultiplySpecial(__m128d first, __m128d second) {
            return _mm_fmadd_pd(_mm_unpacklo_pd(first, first), second, _mm_unpackhi_pd(first, first) * _mm_permute_pd(second, 1));
        }

        static inline __m128d complexScalarMultiply(__m128d complex, double scalar) {
            return complex * _mm_set1_pd(scalar);
        }

        void resize(std::uint32_t transformLength) {
            if (transformLength > length << 1) {
                std::uint32_t halfLog = detail::log2(transformLength) >> 1, halfSize = 1 << halfLog;
                __m128d* baseFactors = new __m128d[halfSize << 1];
                const double angleStep = std::acos(-1.0) / halfSize, fineAngleStep = angleStep / halfSize;
                for (std::uint32_t i = 0, j = (halfSize * 3) >> 1, phaseAccumulator = 0; i != halfSize; phaseAccumulator -= halfSize - (j >> __builtin_ctz(++i))) {
                    std::complex<double> first = std::polar(1.0, phaseAccumulator * angleStep), second = std::polar(1.0, phaseAccumulator * fineAngleStep);
                    baseFactors[i] = _mm_set_pd(first.imag(), first.real()), baseFactors[i | halfSize] = _mm_set_pd(second.imag(), second.real());
                }
                __m128d* newFactors = reinterpret_cast<__m128d*>(std::memcpy(new __m128d[transformLength >> 1], twiddleFactors, length << 4));
                delete[] twiddleFactors, twiddleFactors = newFactors;
                for (std::uint32_t i = length; i != transformLength >> 1; ++i)
                    twiddleFactors[i] = complexMultiply(baseFactors[i & (halfSize - 1)], baseFactors[halfSize | (i >> halfLog)]);
                delete[] baseFactors, length = transformLength >> 1;
            }
        }

        // OPT: __m256d 蝶形，一次处理 2 个复数，指令数减半
        void decimationInFrequency(__m128d* dataArray, std::uint32_t transformSize) {
            for (std::uint32_t blockSize = transformSize >> 1, stepSize = transformSize; blockSize; stepSize = blockSize, blockSize >>= 1) {
                {
                    __m128d* end = dataArray + blockSize;
                    __m128d* p = dataArray;
                    for (; p + 2 <= end; p += 2) {
                        const __m256d e = _mm256_loadu_pd(reinterpret_cast<const double*>(p));
                        const __m256d o = _mm256_loadu_pd(reinterpret_cast<const double*>(p + blockSize));
                        _mm256_storeu_pd(reinterpret_cast<double*>(p), _mm256_add_pd(e, o));
                        _mm256_storeu_pd(reinterpret_cast<double*>(p + blockSize), _mm256_sub_pd(e, o));
                    }
                    if (p != end) {
                        const __m128d e = *p, o = p[blockSize];
                        *p = _mm_add_pd(e, o), p[blockSize] = _mm_sub_pd(e, o);
                    }
                }
                for (__m128d *blockStart = dataArray + stepSize, *twiddlePointer = twiddleFactors + 1; blockStart != dataArray + transformSize; blockStart += stepSize, ++twiddlePointer) {
                    const __m128d w = *twiddlePointer;
                    const __m256d w256 = _mm256_set_m128d(w, w);
                    const __m256d w_swap = _mm256_permute_pd(w256, 0x5);
                    __m128d* end = blockStart + blockSize;
                    __m128d* p = blockStart;
                    for (; p + 2 <= end; p += 2) {
                        const __m256d e = _mm256_loadu_pd(reinterpret_cast<const double*>(p));
                        const __m256d o = _mm256_loadu_pd(reinterpret_cast<const double*>(p + blockSize));
                        const __m256d o_lo = _mm256_unpacklo_pd(o, o);
                        const __m256d o_hi = _mm256_unpackhi_pd(o, o);
                        const __m256d o_mul = _mm256_fmaddsub_pd(o_lo, w256, _mm256_mul_pd(o_hi, w_swap));
                        _mm256_storeu_pd(reinterpret_cast<double*>(p), _mm256_add_pd(e, o_mul));
                        _mm256_storeu_pd(reinterpret_cast<double*>(p + blockSize), _mm256_sub_pd(e, o_mul));
                    }
                    if (p != end) {
                        const __m128d evenElement = *p, oddElement = complexMultiply(p[blockSize], w);
                        *p = _mm_add_pd(evenElement, oddElement), p[blockSize] = _mm_sub_pd(evenElement, oddElement);
                    }
                }
            }
        }

        void decimationInTime(__m128d* dataArray, std::uint32_t transformSize) {
            for (std::uint32_t blockSize = 1, stepSize = 2; blockSize != transformSize; blockSize = stepSize, stepSize <<= 1) {
                {
                    __m128d* end = dataArray + blockSize;
                    __m128d* p = dataArray;
                    for (; p + 2 <= end; p += 2) {
                        const __m256d e = _mm256_loadu_pd(reinterpret_cast<const double*>(p));
                        const __m256d o = _mm256_loadu_pd(reinterpret_cast<const double*>(p + blockSize));
                        _mm256_storeu_pd(reinterpret_cast<double*>(p), _mm256_add_pd(e, o));
                        _mm256_storeu_pd(reinterpret_cast<double*>(p + blockSize), _mm256_sub_pd(e, o));
                    }
                    if (p != end) {
                        const __m128d e = *p, o = p[blockSize];
                        *p = _mm_add_pd(e, o), p[blockSize] = _mm_sub_pd(e, o);
                    }
                }
                for (__m128d *blockStart = dataArray + stepSize, *twiddlePointer = twiddleFactors + 1; blockStart != dataArray + transformSize; blockStart += stepSize, ++twiddlePointer) {
                    const __m128d w = *twiddlePointer;
                    const __m256d w256 = _mm256_set_m128d(w, w);
                    const __m256d w_lo = _mm256_unpacklo_pd(w256, w256);
                    const __m256d w_hi = _mm256_unpackhi_pd(w256, w256);
                    __m128d* end = blockStart + blockSize;
                    __m128d* p = blockStart;
                    for (; p + 2 <= end; p += 2) {
                        const __m256d e = _mm256_loadu_pd(reinterpret_cast<const double*>(p));
                        const __m256d o = _mm256_loadu_pd(reinterpret_cast<const double*>(p + blockSize));
                        const __m256d diff = _mm256_sub_pd(e, o);
                        const __m256d diff_swap = _mm256_permute_pd(diff, 0x5);
                        const __m256d odd_result = _mm256_fmsubadd_pd(w_lo, diff, _mm256_mul_pd(w_hi, diff_swap));
                        _mm256_storeu_pd(reinterpret_cast<double*>(p), _mm256_add_pd(e, o));
                        _mm256_storeu_pd(reinterpret_cast<double*>(p + blockSize), odd_result);
                    }
                    if (p != end) {
                        const __m128d evenElement = *p, oddElement = p[blockSize];
                        *p = _mm_add_pd(evenElement, oddElement), p[blockSize] = complexMultiplyConjugate(_mm_sub_pd(evenElement, oddElement), w);
                    }
                }
            }
        }

        void frequencyDomainPointwiseMultiply(__m128d* firstArray, __m128d* secondArray, std::uint32_t transformSize) {
            const double normalizationFactor = 1.0 / transformSize, scalingFactor = normalizationFactor * 0.25;
            firstArray[0] = complexScalarMultiply(complexMultiplySpecial(firstArray[0], secondArray[0]), normalizationFactor);
            firstArray[1] = complexScalarMultiply(complexMultiply(firstArray[1], secondArray[1]), normalizationFactor);
            const __m128d conjugateMask = _mm_castsi128_pd(_mm_set_epi64x(std::int64_t(1ull << 63), 0));
            const __m128d negateMask = _mm_castsi128_pd(_mm_set_epi64x(std::int64_t(1ull << 63), std::int64_t(1ull << 63)));
            for (std::uint32_t blockStart = 2, blockEnd = 3; blockStart != transformSize; blockStart <<= 1, blockEnd <<= 1) {
                for (std::uint32_t forwardIndex = blockStart, backwardIndex = forwardIndex + blockStart - 1; forwardIndex != blockEnd; ++forwardIndex, --backwardIndex) {
                    const __m128d firstEven = _mm_add_pd(firstArray[forwardIndex], _mm_xor_pd(firstArray[backwardIndex], conjugateMask)), firstOdd = _mm_sub_pd(firstArray[forwardIndex], _mm_xor_pd(firstArray[backwardIndex], conjugateMask));
                    const __m128d secondEven = _mm_add_pd(secondArray[forwardIndex], _mm_xor_pd(secondArray[backwardIndex], conjugateMask)), secondOdd = _mm_sub_pd(secondArray[forwardIndex], _mm_xor_pd(secondArray[backwardIndex], conjugateMask));
                    const __m128d productA = _mm_sub_pd(complexMultiply(firstEven, secondEven), complexMultiply(complexMultiply(firstOdd, secondOdd), (forwardIndex & 1 ? _mm_xor_pd(twiddleFactors[forwardIndex >> 1], negateMask) : twiddleFactors[forwardIndex >> 1]))), productB = _mm_add_pd(complexMultiply(secondEven, firstOdd), complexMultiply(firstEven, secondOdd));
                    firstArray[forwardIndex] = complexScalarMultiply(_mm_add_pd(productA, productB), scalingFactor);
                    firstArray[backwardIndex] = _mm_xor_pd(complexScalarMultiply(_mm_sub_pd(productA, productB), scalingFactor), conjugateMask);
                }
            }
        }
        // OPT: 平方专用点乘。secondEven=firstEven, secondOdd=firstOdd，省一半点乘工作。
        // firstEven/firstOdd 已读到寄存器，写回 array[forward/backward] 不冲突。
        void frequencyDomainPointwiseSquare(__m128d* array, std::uint32_t transformSize) {
            const double normalizationFactor = 1.0 / transformSize, scalingFactor = normalizationFactor * 0.25;
            array[0] = complexScalarMultiply(complexMultiplySpecial(array[0], array[0]), normalizationFactor);
            array[1] = complexScalarMultiply(complexMultiply(array[1], array[1]), normalizationFactor);
            const __m128d conjugateMask = _mm_castsi128_pd(_mm_set_epi64x(std::int64_t(1ull << 63), 0));
            const __m128d negateMask = _mm_castsi128_pd(_mm_set_epi64x(std::int64_t(1ull << 63), std::int64_t(1ull << 63)));
            for (std::uint32_t blockStart = 2, blockEnd = 3; blockStart != transformSize; blockStart <<= 1, blockEnd <<= 1) {
                for (std::uint32_t forwardIndex = blockStart, backwardIndex = forwardIndex + blockStart - 1; forwardIndex != blockEnd; ++forwardIndex, --backwardIndex) {
                    const __m128d evenPart = _mm_add_pd(array[forwardIndex], _mm_xor_pd(array[backwardIndex], conjugateMask)), oddPart = _mm_sub_pd(array[forwardIndex], _mm_xor_pd(array[backwardIndex], conjugateMask));
                    const __m128d twiddle = (forwardIndex & 1 ? _mm_xor_pd(twiddleFactors[forwardIndex >> 1], negateMask) : twiddleFactors[forwardIndex >> 1]);
                    const __m128d evenSquared = complexMultiply(evenPart, evenPart), oddSquared = complexMultiply(oddPart, oddPart);
                    const __m128d productA = _mm_sub_pd(evenSquared, complexMultiply(oddSquared, twiddle));
                    const __m128d evenTimesOdd = complexMultiply(evenPart, oddPart);
                    const __m128d productB = _mm_add_pd(evenTimesOdd, evenTimesOdd);
                    array[forwardIndex] = complexScalarMultiply(_mm_add_pd(productA, productB), scalingFactor);
                    array[backwardIndex] = _mm_xor_pd(complexScalarMultiply(_mm_sub_pd(productA, productB), scalingFactor), conjugateMask);
                }
            }
        }
    };
#elif defined(__ARM_NEON__)
    struct TransformHelper {
        float64x2_t* twiddleFactors;
        std::uint32_t length;

        TransformHelper() : twiddleFactors(new float64x2_t[1]()), length(1) {
            *twiddleFactors = vsetq_lane_f64(0.0, vsetq_lane_f64(1.0, vdupq_n_f64(0.0), 0), 1);
        }

        ~TransformHelper() noexcept {
            delete[] twiddleFactors;
        }

        static inline float64x2_t complexMultiply(float64x2_t first, float64x2_t second) {
            float64x2_t term1 = vmulq_laneq_f64(second, first, 0);
            float64x2_t second_swapped = vextq_f64(second, second, 1);
            float64x2_t term2_raw = vmulq_laneq_f64(second_swapped, first, 1);
            const float64x2_t mask_neg_real = vsetq_lane_f64(1.0, vsetq_lane_f64(-1.0, vdupq_n_f64(0.0), 0), 1);
            return vfmaq_f64(term1, term2_raw, mask_neg_real);
        }

        static inline float64x2_t complexMultiplyConjugate(float64x2_t first, float64x2_t second) {
            float64x2_t second_swapped = vextq_f64(second, second, 1);
            float64x2_t term1 = vmulq_laneq_f64(second_swapped, first, 1);
            float64x2_t term2_raw = vmulq_laneq_f64(second, first, 0);
            const float64x2_t mask_add_real_sub_imag = vsetq_lane_f64(-1.0, vsetq_lane_f64(1.0, vdupq_n_f64(0.0), 0), 1);
            return vfmaq_f64(term1, term2_raw, mask_add_real_sub_imag);
        }

        static inline float64x2_t complexMultiplySpecial(float64x2_t first, float64x2_t second) {
            float64x2_t term1 = vmulq_laneq_f64(second, first, 0);
            float64x2_t second_rev = vextq_f64(second, second, 1);
            return vfmaq_laneq_f64(term1, second_rev, first, 1);
        }

        static inline float64x2_t complexScalarMultiply(float64x2_t complex, double scalar) {
            return vmulq_n_f64(complex, scalar);
        }

        void resize(std::uint32_t transformLength) {
            if (transformLength > length << 1) {
                std::uint32_t halfLog = detail::log2(transformLength) >> 1, halfSize = 1 << halfLog;
                float64x2_t* baseFactors = new float64x2_t[halfSize << 1]();
                const double angleStep = std::acos(-1.0) / halfSize, fineAngleStep = angleStep / halfSize;
                for (std::uint32_t i = 0, j = (halfSize * 3) >> 1, phaseAccumulator = 0; i != halfSize; phaseAccumulator -= halfSize - (j >> __builtin_ctz(++i))) {
                    std::complex<double> first = std::polar(1.0, phaseAccumulator * angleStep), second = std::polar(1.0, phaseAccumulator * fineAngleStep);
                    baseFactors[i] = vsetq_lane_f64(first.imag(), vsetq_lane_f64(first.real(), vdupq_n_f64(0.0), 0), 1);
                    baseFactors[i | halfSize] = vsetq_lane_f64(second.imag(), vsetq_lane_f64(second.real(), vdupq_n_f64(0.0), 0), 1);
                }
                float64x2_t* newFactors = new float64x2_t[transformLength >> 1];
                std::memcpy(newFactors, twiddleFactors, length * sizeof(float64x2_t));
                delete[] twiddleFactors;
                twiddleFactors = newFactors;
                for (std::uint32_t i = length; i != transformLength >> 1; ++i)
                    twiddleFactors[i] = complexMultiply(baseFactors[i & (halfSize - 1)], baseFactors[halfSize | (i >> halfLog)]);
                delete[] baseFactors;
                length = transformLength >> 1;
            }
        }

        void decimationInFrequency(float64x2_t* dataArray, std::uint32_t transformSize) {
            for (std::uint32_t blockSize = transformSize >> 1, stepSize = transformSize; blockSize; stepSize = blockSize, blockSize >>= 1) {
                for (float64x2_t* currentElement = dataArray; currentElement != dataArray + blockSize; ++currentElement) {
                    const float64x2_t evenElement = *currentElement, oddElement = currentElement[blockSize];
                    *currentElement = vaddq_f64(evenElement, oddElement);
                    currentElement[blockSize] = vsubq_f64(evenElement, oddElement);
                }
                for (float64x2_t *blockStart = dataArray + stepSize, *twiddlePointer = twiddleFactors + 1; blockStart != dataArray + transformSize; blockStart += stepSize, ++twiddlePointer) {
                    for (float64x2_t* currentElement = blockStart; currentElement != blockStart + blockSize; ++currentElement) {
                        const float64x2_t evenElement = *currentElement, oddElement = complexMultiply(currentElement[blockSize], *twiddlePointer);
                        *currentElement = vaddq_f64(evenElement, oddElement);
                        currentElement[blockSize] = vsubq_f64(evenElement, oddElement);
                    }
                }
            }
        }

        void decimationInTime(float64x2_t* dataArray, std::uint32_t transformSize) {
            for (std::uint32_t blockSize = 1, stepSize = 2; blockSize != transformSize; blockSize = stepSize, stepSize <<= 1) {
                for (float64x2_t* currentElement = dataArray; currentElement != dataArray + blockSize; ++currentElement) {
                    const float64x2_t evenElement = *currentElement, oddElement = currentElement[blockSize];
                    *currentElement = vaddq_f64(evenElement, oddElement);
                    currentElement[blockSize] = vsubq_f64(evenElement, oddElement);
                }
                for (float64x2_t *blockStart = dataArray + stepSize, *twiddlePointer = twiddleFactors + 1; blockStart != dataArray + transformSize; blockStart += stepSize, ++twiddlePointer) {
                    for (float64x2_t* currentElement = blockStart; currentElement != blockStart + blockSize; ++currentElement) {
                        const float64x2_t evenElement = *currentElement, oddElement = currentElement[blockSize];
                        *currentElement = vaddq_f64(evenElement, oddElement);
                        currentElement[blockSize] = complexMultiplyConjugate(vsubq_f64(evenElement, oddElement), *twiddlePointer);
                    }
                }
            }
        }

        void frequencyDomainPointwiseMultiply(float64x2_t* firstArray, float64x2_t* secondArray, std::uint32_t transformSize) {
            const double normalizationFactor = 1.0 / transformSize, scalingFactor = normalizationFactor * 0.25;
            firstArray[0] = complexScalarMultiply(complexMultiplySpecial(firstArray[0], secondArray[0]), normalizationFactor);
            firstArray[1] = complexScalarMultiply(complexMultiply(firstArray[1], secondArray[1]), normalizationFactor);
            const float64x2_t conjugateMask = vsetq_lane_f64(-1.0, vsetq_lane_f64(1.0, vdupq_n_f64(0.0), 0), 1);
            const float64x2_t negateMask = vdupq_n_f64(-1.0);
            for (std::uint32_t blockStart = 2, blockEnd = 3; blockStart != transformSize; blockStart <<= 1, blockEnd <<= 1) {
                for (std::uint32_t forwardIndex = blockStart, backwardIndex = forwardIndex + blockStart - 1; forwardIndex != blockEnd; ++forwardIndex, --backwardIndex) {
                    auto conj = [&](float64x2_t v) { return vmulq_f64(v, conjugateMask); };
                    const float64x2_t firstEven = vaddq_f64(firstArray[forwardIndex], conj(firstArray[backwardIndex])), firstOdd = vsubq_f64(firstArray[forwardIndex], conj(firstArray[backwardIndex]));
                    const float64x2_t secondEven = vaddq_f64(secondArray[forwardIndex], conj(secondArray[backwardIndex])), secondOdd = vsubq_f64(secondArray[forwardIndex], conj(secondArray[backwardIndex]));
                    const float64x2_t twiddle = (forwardIndex & 1 ? vmulq_f64(twiddleFactors[forwardIndex >> 1], negateMask) : twiddleFactors[forwardIndex >> 1]);
                    const float64x2_t productA = vsubq_f64(complexMultiply(firstEven, secondEven), complexMultiply(complexMultiply(firstOdd, secondOdd), twiddle)), productB = vaddq_f64(complexMultiply(secondEven, firstOdd), complexMultiply(firstEven, secondOdd));
                    firstArray[forwardIndex] = complexScalarMultiply(vaddq_f64(productA, productB), scalingFactor);
                    firstArray[backwardIndex] = conj(complexScalarMultiply(vsubq_f64(productA, productB), scalingFactor));
                }
            }
        }
    };
#else
    struct TransformHelper {
        std::complex<double>* twiddleFactors;
        std::uint32_t length;

        TransformHelper() : twiddleFactors(new std::complex<double>[1]()), length(1) {
            *twiddleFactors = {1.0, 0.0};
        }

        ~TransformHelper() noexcept {
            delete[] twiddleFactors;
        }

        static inline std::complex<double> complexMultiply(std::complex<double> first, std::complex<double> second) {
            return first * second;
        }

        static inline std::complex<double> complexMultiplyConjugate(std::complex<double> first, std::complex<double> second) {
            return first * std::conj(second);
        }

        std::complex<double> complexMultiplySpecial(
            const std::complex<double>& first,
            const std::complex<double>& second) {
            double r1 = first.real();
            double i1 = first.imag();
            double r2 = second.real();
            double i2 = second.imag();

            double result_real = r1 * r2 + i1 * i2;

            double result_imag = r1 * i2 + i1 * r2;

            return std::complex<double>(result_real, result_imag);
        }

        static inline std::complex<double> complexScalarMultiply(std::complex<double> complex, double scalar) {
            return complex * scalar;
        }

        void resize(std::uint32_t transformLength) {
            if (transformLength > length << 1) {
                std::uint32_t halfLog = detail::log2(transformLength) >> 1, halfSize = 1 << halfLog;
                std::complex<double>* baseFactors = new std::complex<double>[halfSize << 1]();
                const double angleStep = std::acos(-1.0) / halfSize, fineAngleStep = angleStep / halfSize;
                for (std::uint32_t i = 0, j = (halfSize * 3) >> 1, phaseAccumulator = 0; i != halfSize; phaseAccumulator -= halfSize - (j >> __builtin_ctz(++i))) {
                    baseFactors[i] = std::polar(1.0, phaseAccumulator * angleStep);
                    baseFactors[i | halfSize] = std::polar(1.0, phaseAccumulator * fineAngleStep);
                }
                std::complex<double>* newFactors = new std::complex<double>[transformLength >> 1];
                std::memcpy(newFactors, twiddleFactors, length * sizeof(std::complex<double>));
                delete[] twiddleFactors;
                twiddleFactors = newFactors;
                for (std::uint32_t i = length; i != transformLength >> 1; ++i)
                    twiddleFactors[i] = baseFactors[i & (halfSize - 1)] * baseFactors[halfSize | (i >> halfLog)];
                delete[] baseFactors;
                length = transformLength >> 1;
            }
        }

        void decimationInFrequency(std::complex<double>* dataArray, std::uint32_t transformSize) {
            for (std::uint32_t blockSize = transformSize >> 1, stepSize = transformSize; blockSize; stepSize = blockSize, blockSize >>= 1) {
                for (std::complex<double>* currentElement = dataArray; currentElement != dataArray + blockSize; ++currentElement) {
                    const std::complex<double> evenElement = *currentElement, oddElement = currentElement[blockSize];
                    *currentElement = evenElement + oddElement;
                    currentElement[blockSize] = evenElement - oddElement;
                }
                for (std::complex<double>*blockStart = dataArray + stepSize, *twiddlePointer = twiddleFactors + 1; blockStart != dataArray + transformSize; blockStart += stepSize, ++twiddlePointer) {
                    for (std::complex<double>* currentElement = blockStart; currentElement != blockStart + blockSize; ++currentElement) {
                        const std::complex<double> evenElement = *currentElement, oddElement = currentElement[blockSize] * *twiddlePointer;
                        *currentElement = evenElement + oddElement;
                        currentElement[blockSize] = evenElement - oddElement;
                    }
                }
            }
        }

        void decimationInTime(std::complex<double>* dataArray, std::uint32_t transformSize) {
            for (std::uint32_t blockSize = 1, stepSize = 2; blockSize != transformSize; blockSize = stepSize, stepSize <<= 1) {
                for (std::complex<double>* currentElement = dataArray; currentElement != dataArray + blockSize; ++currentElement) {
                    const std::complex<double> evenElement = *currentElement, oddElement = currentElement[blockSize];
                    *currentElement = evenElement + oddElement;
                    currentElement[blockSize] = evenElement - oddElement;
                }
                for (std::complex<double>*blockStart = dataArray + stepSize, *twiddlePointer = twiddleFactors + 1; blockStart != dataArray + transformSize; blockStart += stepSize, ++twiddlePointer) {
                    for (std::complex<double>* currentElement = blockStart; currentElement != blockStart + blockSize; ++currentElement) {
                        const std::complex<double> evenElement = *currentElement, oddElement = currentElement[blockSize];
                        *currentElement = evenElement + oddElement;
                        currentElement[blockSize] = (evenElement - oddElement) * std::conj(*twiddlePointer);
                    }
                }
            }
        }

        void frequencyDomainPointwiseMultiply(std::complex<double>* firstArray, std::complex<double>* secondArray, std::uint32_t transformSize) {
            const double normalizationFactor = 1.0 / transformSize, scalingFactor = normalizationFactor * 0.25;
            firstArray[0] = complexScalarMultiply(complexMultiplySpecial(firstArray[0], secondArray[0]), normalizationFactor);
            firstArray[1] = complexScalarMultiply(complexMultiply(firstArray[1], secondArray[1]), normalizationFactor);
            for (std::uint32_t blockStart = 2, blockEnd = 3; blockStart != transformSize; blockStart <<= 1, blockEnd <<= 1) {
                for (std::uint32_t forwardIndex = blockStart, backwardIndex = forwardIndex + blockStart - 1; forwardIndex != blockEnd; ++forwardIndex, --backwardIndex) {
                    const std::complex<double> firstEven = firstArray[forwardIndex] + std::conj(firstArray[backwardIndex]), firstOdd = firstArray[forwardIndex] - std::conj(firstArray[backwardIndex]);
                    const std::complex<double> secondEven = secondArray[forwardIndex] + std::conj(secondArray[backwardIndex]), secondOdd = secondArray[forwardIndex] - std::conj(secondArray[backwardIndex]);
                    const std::complex<double> twiddle = (forwardIndex & 1 ? -twiddleFactors[forwardIndex >> 1] : twiddleFactors[forwardIndex >> 1]);
                    const std::complex<double> productA = firstEven * secondEven - firstOdd * secondOdd * twiddle, productB = secondEven * firstOdd + firstEven * secondOdd;
                    firstArray[forwardIndex] = (productA + productB) * scalingFactor;
                    firstArray[backwardIndex] = std::conj((productA - productB) * scalingFactor);
                }
            }
        }
    };
#endif

    static __CONSTEXPR InputHelper I = {};
    static __CONSTEXPR OutputHelper O = {};
    static thread_local TransformHelper T = {};
    class DigitAllocator {
        static constexpr int NumBuckets = 24;
        struct Node { Node* next; };
        static thread_local Node* buckets[NumBuckets];
        static int bucketIdx(std::uint32_t size) {
            if (size <= 1) return 0;
            int idx = 32 - __builtin_clz(size - 1);
            return idx < NumBuckets ? idx : NumBuckets - 1;
        }
    public:
        static std::uint32_t* allocate(std::uint32_t size) {
            if (size == 0) size = 1;
            int idx = bucketIdx(size);
            std::uint32_t bucketSize = std::uint32_t(1) << idx;
            std::uint32_t* p;
            if (buckets[idx]) {
                Node* n = buckets[idx];
                buckets[idx] = n->next;
                p = reinterpret_cast<std::uint32_t*>(n);
            } else {
                p = static_cast<std::uint32_t*>(::operator new(std::size_t(bucketSize) * sizeof(std::uint32_t)));
            }
            // 不清零：所有调用者要么 memcpy 全量、要么 construct 写满 [0,length)、
            // 要么在读取前显式初始化（如除法 quotient.digits[i]=0 先于 +=）。
            // 仅 default ctor / move fallback 手动 *digits=0。
            return p;
        }
        static void deallocate(std::uint32_t* p, std::uint32_t size) {
            if (!p) return;
            int idx = bucketIdx(size == 0 ? 1 : size);
            Node* n = reinterpret_cast<Node*>(p);
            n->next = buckets[idx];
            buckets[idx] = n;
        }
    };
    thread_local DigitAllocator::Node* DigitAllocator::buckets[DigitAllocator::NumBuckets] = {};

} // namespace detail

class UnsignedInteger;
class SignedInteger;

class UnsignedInteger {
    static constexpr std::uint32_t Base = 100000000;
    static constexpr std::uint32_t TransformLimit = 4194304;
    // OPT: 96 为 BF/FFT 交叉点。nd>96 时 FFT(~5.5μs) 优于 BF(4~8μs)。
    // 原 128 导致 1K 规模(nd=125)走 BF(8.3μs) 而非 FFT(5.5μs)，慢 1.5x。
    static constexpr std::uint32_t BruteforceThreshold = 96;
    // OPT: 不平衡拆分的最小 other 长度。小于此值时 4n FFT 仍 fit L2，拆分开销 > 收益。
    static constexpr std::uint32_t UnbalancedThreshold = 4096;

public:
    // OPT: 改为 public 以支持 LC 提交的 mmap+oBuffer 零拷贝 I/O（与原作者一致）
    std::uint32_t *digits, length, capacity;

  protected:
    UnsignedInteger(std::uint32_t initialLength, std::uint32_t initialCapacity) : digits(detail::DigitAllocator::allocate(initialCapacity)), length(initialLength), capacity(initialCapacity) {}

    void resize(std::uint32_t newLength) {
        if (newLength > capacity) {
            std::uint32_t oldCapacity = capacity;
            std::uint32_t* newMemory = reinterpret_cast<std::uint32_t*>(std::memcpy(detail::DigitAllocator::allocate(newLength), digits, length << 2));
            detail::DigitAllocator::deallocate(digits, oldCapacity), digits = newMemory, capacity = newLength;
        }
        length = newLength;
    }

  public:
    // OPT: 改为 public 以支持 LC 提交的 mmap+oBuffer 零拷贝 I/O（与原作者一致）
    void construct(const char* value, std::uint32_t stringLength) {
        std::uint32_t* currentDigit = digits + length - 1;
        switch (stringLength & 7) {
            case 0:
                ++currentDigit;
                break;
            case 1:
                *currentDigit = *value & 15;
                break;
            case 2:
                *currentDigit = detail::I(value);
                break;
            case 3:
                *currentDigit = (*value & 15) * 100 + detail::I(value + 1);
                break;
            case 4:
                *currentDigit = detail::I(value) * 100 + detail::I(value + 2);
                break;
            case 5:
                *currentDigit = (*value & 15) * 10000 + detail::I(value + 1) * 100 + detail::I(value + 3);
                break;
            case 6:
                *currentDigit = detail::I(value) * 10000 + detail::I(value + 2) * 100 + detail::I(value + 4);
                break;
            case 7:
                *currentDigit = (*value & 15) * 1000000 + detail::I(value + 1) * 10000 + detail::I(value + 3) * 100 + detail::I(value + 5);
                break;
        }
        for (const char* position = value + (stringLength & 7); currentDigit != digits; *--currentDigit = detail::I(position) * 1000000 + detail::I(position + 2) * 10000 + detail::I(position + 4) * 100 + detail::I(position + 6), position += 8);
        for (; length > 1 && !digits[length - 1]; --length);
    }

  protected:
    std::int32_t reverseCompare(const UnsignedInteger& other) const {
        constexpr std::uint32_t BlockSize = 64;
        const std::uint32_t remaining = length % BlockSize, *firstBlockPointer = digits + length - remaining, *secondBlockPointer = other.digits + length - remaining;
        if (std::memcmp(firstBlockPointer, secondBlockPointer, remaining << 2))
            for (const std::uint32_t *firstElementPointer = firstBlockPointer + remaining, *secondElementPointer = secondBlockPointer + remaining; firstElementPointer != firstBlockPointer;)
                if (*--firstElementPointer != *--secondElementPointer)
                    return std::int32_t(*firstElementPointer - *secondElementPointer);
        while (firstBlockPointer != digits)
            if (std::memcmp(firstBlockPointer -= BlockSize, secondBlockPointer -= BlockSize, BlockSize << 2))
                for (const std::uint32_t *firstElementPointer = firstBlockPointer + BlockSize, *secondElementPointer = secondBlockPointer + BlockSize; firstElementPointer != firstBlockPointer;)
                    if (*--firstElementPointer != *--secondElementPointer)
                        return std::int32_t(*firstElementPointer - *secondElementPointer);
        return 0;
    }

    std::pair<UnsignedInteger, UnsignedInteger> bruteforceDivisionAndModulus(const UnsignedInteger& divisor) const {
        if (*this < divisor)
            return std::make_pair(UnsignedInteger(), *this);
        UnsignedInteger quotient(length - divisor.length + 1, length - divisor.length + 1), remainder = *this;
        quotient.length = length - divisor.length + 1;
        auto getEstimatedValue = [](const std::uint32_t* digitArray, std::uint32_t highIndex, std::uint32_t arrayLength) -> std::uint64_t {
            return std::uint64_t(10) * Base * ((highIndex + 1) < arrayLength ? digitArray[highIndex + 1] : 0) + std::uint64_t(10) * digitArray[highIndex] + (highIndex ? digitArray[highIndex - 1] : 0) / (Base / 10);
        };
        for (std::uint32_t currentPosition = length - divisor.length; ~currentPosition; --currentPosition) {
            std::uint32_t partialQuotient = 0;
            auto performSubtraction = [&]() {
                std::int64_t carry = 0;
                for (std::uint32_t digitIndex = 0; digitIndex != divisor.length; ++digitIndex) {
                    carry = carry - std::int64_t(partialQuotient) * divisor.digits[digitIndex] + remainder.digits[currentPosition + digitIndex];
                    remainder.digits[currentPosition + digitIndex] = std::uint32_t(carry % Base), carry /= Base;
                    if (remainder.digits[currentPosition + digitIndex] >= Base)
                        remainder.digits[currentPosition + digitIndex] += Base, --carry;
                }
                if (carry)
                    remainder.digits[currentPosition + divisor.length] += std::uint32_t(carry);
                quotient.digits[currentPosition] += partialQuotient;
            };
            for (quotient.digits[currentPosition] = 0; (partialQuotient = std::uint32_t(getEstimatedValue(remainder.digits, currentPosition + divisor.length - 1, length) / (getEstimatedValue(divisor.digits, divisor.length - 1, divisor.length) + 1))); performSubtraction());
            partialQuotient = 1;
            for (std::uint32_t digitIndex = divisor.length; digitIndex--;)
                if (remainder.digits[digitIndex + currentPosition] != divisor.digits[digitIndex] && (partialQuotient = divisor.digits[digitIndex] < remainder.digits[digitIndex + currentPosition], true))
                    break;
            if (partialQuotient)
                performSubtraction();
        }
        for (; quotient.length > 1 && !quotient.digits[quotient.length - 1]; --quotient.length);
        for (; remainder.length > 1 && !remainder.digits[remainder.length - 1]; --remainder.length);
        return std::make_pair(std::move(quotient), std::move(remainder));
    }

    UnsignedInteger rightShift(std::uint32_t shiftAmount) const {
        if (shiftAmount >= length)
            return UnsignedInteger();
        UnsignedInteger result(length - shiftAmount, length - shiftAmount);
        std::memcpy(result.digits, digits + shiftAmount, result.length << 2);
        for (; result.length > 1 && !result.digits[result.length - 1]; --result.length);
        return result;
    }

    // OPT: 取低 k 位 digit（用于不平衡乘法拆分）
    UnsignedInteger lower(std::uint32_t k) const {
        if (k >= length) return *this;
        UnsignedInteger result(k, k);
        std::memcpy(result.digits, digits, k << 2);
        for (; result.length > 1 && !result.digits[result.length - 1]; --result.length);
        return result;
    }

    UnsignedInteger leftShift(std::uint32_t shiftAmount) const {
        if (length == 0 || shiftAmount == 0)
            return *this;
        UnsignedInteger result(length + shiftAmount, length + shiftAmount);
        std::memset(result.digits, 0, shiftAmount << 2), std::memcpy(result.digits + shiftAmount, digits, length << 2);
        return result;
    }

    UnsignedInteger computeInverse(std::uint32_t precisionBits) const {
        if (length < BruteforceThreshold || precisionBits < length + BruteforceThreshold) {
            UnsignedInteger numerator(precisionBits + 1, precisionBits + 1);
            std::memset(numerator.digits, 0, precisionBits << 2), numerator.digits[precisionBits] = 1;
            return numerator.bruteforceDivisionAndModulus(*this).first;
        }
        const std::uint32_t halfPrecision = (precisionBits - length + 5) >> 1, shiftBack = halfPrecision > length ? 0 : length - halfPrecision;
#ifdef PROFILE_DIVISION
        UnsignedInteger t, ai, sq, sm;
        { DIV_PROFILE(5); t = rightShift(shiftBack); }
        { DIV_PROFILE(6); ai = t.computeInverse(halfPrecision + t.length); }
        { DIV_PROFILE(7); sq = ai.square(); }
        { DIV_PROFILE(8); sm = std::move(sq) *= *this; }
        { DIV_PROFILE(9);
        const std::uint32_t newPrecision = halfPrecision + t.length;
        const auto& approximateInverse = ai;
        const auto& sqMulResult = sm;
#else
        UnsignedInteger truncated = rightShift(shiftBack);
        const std::uint32_t newPrecision = halfPrecision + truncated.length;
        UnsignedInteger approximateInverse = truncated.computeInverse(newPrecision);
        UnsignedInteger sqMulResult = approximateInverse.square() * *this;
#endif
        // OPT: 融合 (a+a).leftShift(s) - sqMul.rightShift(t) 为单次操作
        // 省去 3 次临时对象分配 (a+a, leftShift, rightShift)
        const std::uint32_t shift1 = precisionBits - newPrecision - shiftBack;
        const std::uint32_t shift2 = 2 * (newPrecision + shiftBack) - precisionBits;
        // result = 2 * approximateInverse << shift1
        UnsignedInteger result(approximateInverse.length + shift1 + 1, approximateInverse.length + shift1 + 1);
        std::memset(result.digits, 0, shift1 << 2);
        {
            std::uint64_t carry = 0;
            for (std::uint32_t i = 0; i < approximateInverse.length; ++i) {
                carry += std::uint64_t(approximateInverse.digits[i]) << 1;
                result.digits[shift1 + i] = std::uint32_t(carry % Base), carry /= Base;
            }
            if (carry)
                result.digits[shift1 + approximateInverse.length] = std::uint32_t(carry);
            else
                --result.length;
        }
        // result -= sqMulResult >> shift2 (in-place, no temporary for the shift)
        if (sqMulResult.length > shift2) {
            const std::uint32_t sqShiftedLen = sqMulResult.length - shift2;
            std::uint32_t borrow = 0;
            std::uint32_t i = 0;
            for (; i < sqShiftedLen && i < result.length; ++i) {
                std::int64_t diff = std::int64_t(result.digits[i]) - std::int64_t(sqMulResult.digits[i + shift2]) - borrow;
                if (diff < 0) diff += Base, borrow = 1;
                else borrow = 0;
                result.digits[i] = std::uint32_t(diff);
            }
            for (; borrow && i < result.length; ++i) {
                if (result.digits[i] >= borrow)
                    result.digits[i] -= borrow, borrow = 0;
                else
                    result.digits[i] += Base - borrow, borrow = 1;
            }
        }
        for (; result.length > 1 && !result.digits[result.length - 1]; --result.length);
        return --result;
#ifdef PROFILE_DIVISION
        }
#endif
    }

  public:
    // OPT: 改为 public 以支持 LC 除法提交直接调用（与原作者 origin_div 一致，省一次乘法+减法）
    // 分块除法（移植自 div_work 的 absDivNewtonCore2）：blocks>=2 时从高位到低位分块，
    // 每块商长度 len2，被除数块长度 2*len2。复用 inv 避免一次性 A*inv 大乘法。
    // 对 div8（A 是 B 的 8 倍长）原方案要算 8n×n 一次大乘法，分块后变成 8 次 2n×n 小乘法。
    std::pair<UnsignedInteger, UnsignedInteger> divisionAndModulus(const UnsignedInteger& other) const {
        if (*this < other)
            return std::make_pair(UnsignedInteger(), *this);
        if (length < BruteforceThreshold || other.length < BruteforceThreshold)
            return bruteforceDivisionAndModulus(other);

        const std::uint32_t len1 = length, len2 = other.length;
        // Core1 路径：len1 < len2*3，用原 computeInverse + 一次性乘法
        // Core2 路径：len1 >= len2*3（blocks>=3），用分块除法
        // 实测 blocks=2 时 Core2 比Core1 慢 40%（2 次 2n×n 乘法 vs 1 次 2n×n），
        // blocks>=3 时 Core2 才有收益（n 次 2n×n vs 1 次 n*blocks×n）
        if (len1 < len2 * 3) {
            const std::uint32_t precisionBits = len1 - len2 + 5, shiftBack = precisionBits > len2 ? 0 : len2 - precisionBits;
            UnsignedInteger adjustedDivisor = other.rightShift(shiftBack);
            if (shiftBack)
                ++adjustedDivisor;
            const std::uint32_t inversePrecision = precisionBits + adjustedDivisor.length;
            const std::uint32_t totalShift = inversePrecision + shiftBack;
            const std::uint32_t truncShift = len2 - 1;
            UnsignedInteger inv = adjustedDivisor.computeInverse(inversePrecision);
            UnsignedInteger quotient;
            if (shiftBack == 0 && len2 > 1 && len1 >= len2 + BruteforceThreshold)
                quotient = (rightShift(truncShift) * inv).rightShift(totalShift - truncShift);
            else
                quotient = (*this * inv).rightShift(totalShift);
            UnsignedInteger qOther = quotient * other;
            while (qOther > *this)
                --quotient, qOther -= other;
            UnsignedInteger remainder = *this - std::move(qOther);
            for (; remainder >= other; ++quotient, remainder -= other);
            return std::make_pair(std::move(quotient), std::move(remainder));
        }

        // Core2 分块除法路径（len1 >= len2*2）
        // 移植自 div_work 的 absDivNewtonCore2：从高位到低位分块，
        // 每块商长度 len2，window = 前块余数.leftShift(len2) + 当前块（长度 2*len2）
        // inv ≈ Base^(2*len2+5) / other，使 computeInverse 走 Newton 迭代（len2>=96 时）
        // qhat = (window * inv) >> (2*len2+5)
        const std::uint32_t blockPrecision = len2 + 5;
        const std::uint32_t invPrecision = blockPrecision + len2; // = 2*len2 + 5
        UnsignedInteger inv = other.computeInverse(invPrecision);
        // inv ≈ Base^invPrecision / other，inv.length ≈ len2 + 6

        // OPT: 向上取整，确保所有被除数位都被覆盖。当 len1 不是 len2 整数倍时，
        // 最高块不足 len2 位，qhat=0，remainder 传递到下一块。
        const std::uint32_t blocks = (len1 + len2 - 1) / len2;
        UnsignedInteger quotient(len1 - len2 + 1, len1 - len2 + 1);
        std::memset(quotient.digits, 0, quotient.length << 2);

        // 从高位块向低位块处理。remainder 初始为 0，每块更新为 window - qhat*other
        UnsignedInteger remainder; // 初始 0

        for (std::int32_t i = static_cast<std::int32_t>(blocks) - 1; i >= 0; --i) {
            // 当前被除数块 [i*len2, (i+1)*len2)
            std::uint32_t blockStart = i * len2;
            std::uint32_t blockEnd = blockStart + len2;
            if (blockEnd > len1) blockEnd = len1;
            std::uint32_t blockLen = blockEnd - blockStart;

            // window = remainder.leftShift(len2) + block（长度 <= 2*len2）
            UnsignedInteger block(blockLen, blockLen);
            std::memcpy(block.digits, digits + blockStart, blockLen << 2);
            for (; block.length > 1 && !block.digits[block.length - 1]; --block.length);

            UnsignedInteger window = remainder.leftShift(len2);
            // window += block（block 长度 <= len2 <= window.length 或 window 为空）
            if (window.length == 1 && window.digits[0] == 0) {
                window = std::move(block);
            } else {
                // window 低位 [0, len2) 是 0（leftShift 填零），block 填入 [0, blockLen)
                for (std::uint32_t k = 0; k < block.length; ++k) window.digits[k] = block.digits[k];
                // 更新 length
                if (window.length < block.length) window.length = block.length;
                for (; window.length > 1 && !window.digits[window.length - 1]; --window.length);
            }

            // qhat = (window * inv) >> invPrecision
            UnsignedInteger prod = window * inv;
            UnsignedInteger qhat;
            if (prod.length > invPrecision) {
                qhat = prod.rightShift(invPrecision);
            } else {
                qhat = UnsignedInteger();
            }

            // 修正：qhat * other 应 <= window
            UnsignedInteger qOther = qhat * other;
            while (qOther > window) {
                --qhat;
                qOther -= other;
            }
            // 残余修正：window - qOther 可能仍 >= other
            remainder = window - std::move(qOther);
            while (remainder >= other) {
                ++qhat;
                remainder -= other;
            }

            // 写入商块到 quotient.digits[blockStart ...]
            std::uint32_t qhatLen = qhat.length;
            if (blockStart + qhatLen > quotient.length) qhatLen = quotient.length - blockStart;
            if (blockStart < quotient.length) {
                std::uint32_t fillLen = quotient.length - blockStart;
                if (fillLen > len2) fillLen = len2;
                std::memset(quotient.digits + blockStart, 0, fillLen << 2);
                std::memcpy(quotient.digits + blockStart, qhat.digits, qhatLen << 2);
            }
        }

        for (; quotient.length > 1 && !quotient.digits[quotient.length - 1]; --quotient.length);
        for (; remainder.length > 1 && !remainder.digits[remainder.length - 1]; --remainder.length);
        return std::make_pair(std::move(quotient), std::move(remainder));
    }

  public:
    UnsignedInteger() : digits(detail::DigitAllocator::allocate(1)), length(1), capacity(1) { *digits = 0; }

    UnsignedInteger(const UnsignedInteger& other) : digits(reinterpret_cast<std::uint32_t*>(std::memcpy(detail::DigitAllocator::allocate(other.length), other.digits, other.length << 2))), length(other.length), capacity(other.length) {}

    UnsignedInteger(UnsignedInteger&& other) noexcept : digits(other.digits), length(other.length), capacity(other.capacity) {
        other.digits = detail::DigitAllocator::allocate(other.length = other.capacity = 1);
        *other.digits = 0;
    }

    UnsignedInteger(const SignedInteger& other);

    template <typename unsignedIntegral, typename std::enable_if<std::is_unsigned<unsignedIntegral>::value>::type* = nullptr>
    UnsignedInteger(unsignedIntegral value) : UnsignedInteger(0, (std::numeric_limits<unsignedIntegral>::digits10 + 7) >> 3) {
        while (digits[length++] = std::uint32_t(value) % Base, value /= unsignedIntegral(Base));
    }

    template <typename signedIntegral, typename std::enable_if<std::is_signed<signedIntegral>::value && !std::is_floating_point<signedIntegral>::value>::type* = nullptr>
    UnsignedInteger(signedIntegral value) : UnsignedInteger(0, (std::numeric_limits<signedIntegral>::digits10 + 7) >> 3) {
        VALIDITY_CHECK(value >= 0, std::invalid_argument, "UnsignedInteger constructor error: the provided signed integer value = " + std::to_string(value) + " is negative. UnsignedInteger can only represent non-negative integers.")
        while (digits[length++] = std::uint32_t(value) % Base, value /= signedIntegral(Base));
    }

    template <typename floatingPoint, typename std::enable_if<std::is_floating_point<floatingPoint>::value>::type* = nullptr>
    UnsignedInteger(floatingPoint value) : UnsignedInteger(0, (std::numeric_limits<floatingPoint>::max_exponent10 + 7) >> 3) {
        VALIDITY_CHECK(value >= 0, std::invalid_argument, "UnsignedInteger constructor error: the provided floating point value = " + std::to_string(value) + " is negative. UnsignedInteger can only represent non-negative integers.")
        VALIDITY_CHECK(std::isfinite(value), std::invalid_argument, "UnsignedInteger constructor error: the provided floating point value = " + std::to_string(value) + " is not finite.")
        while (digits[length++] = std::uint32_t(std::fmod(value, Base)), (value = std::floor(value / Base)));
    }

    UnsignedInteger(const char* value) {
        VALIDITY_CHECK(value, std::invalid_argument, "UnsignedInteger constructor error: the provided C-style string is a null pointer.")
        const std::uint32_t stringLength = std::uint32_t(std::strlen(value));
        digits = detail::DigitAllocator::allocate(length = capacity = (stringLength + 7) >> 3);
        VALIDITY_CHECK(stringLength, std::invalid_argument, "UnsignedInteger constructor error: the provided C-style string is empty. UnsignedInteger can only be constructed from non-empty strings containing only digits.")
        VALIDITY_CHECK(std::all_of(value, value + stringLength, [](char digit) -> bool { return std::isdigit(digit); }), std::invalid_argument, "UnsignedInteger constructor error: the provided C-style string value = "
                                                                                                                                                " + std::string(value) + "
                                                                                                                                                " contains non-digit characters. UnsignedInteger can only be constructed from non-empty strings containing only digits.")
        construct(value, stringLength);
    }

    UnsignedInteger(const std::string& value) : UnsignedInteger(std::uint32_t(value.size() + 7) >> 3, std::uint32_t(value.size() + 7) >> 3) {
        VALIDITY_CHECK(value.size(), std::invalid_argument, "UnsignedInteger constructor error: the provided string is empty. UnsignedInteger can only be constructed from non-empty strings containing only digits.")
        VALIDITY_CHECK(std::all_of(value.begin(), value.end(), [](char digit) -> bool { return std::isdigit(digit); }), std::invalid_argument, "UnsignedInteger constructor error: the provided string value = "
                                                                                                                                               " + value + "
                                                                                                                                               " contains non-digit characters. UnsignedInteger can only be constructed from non-empty strings containing only digits.")
        construct(value.data(), std::uint32_t(value.size()));
    }

    ~UnsignedInteger() noexcept {
        detail::DigitAllocator::deallocate(digits, capacity);
    }

    UnsignedInteger& operator=(const UnsignedInteger& other) {
        if (digits != other.digits) {
            if (capacity < other.length)
                detail::DigitAllocator::deallocate(digits, capacity), digits = detail::DigitAllocator::allocate(other.length), capacity = other.length;
            std::memcpy(digits, other.digits, (length = other.length) << 2);
        }
        return *this;
    }

    UnsignedInteger& operator=(UnsignedInteger&& other) noexcept {
        if (this != &other) {
            if (digits != other.digits) {
                detail::DigitAllocator::deallocate(digits, capacity);
                digits = other.digits;
                length = other.length;
                capacity = other.capacity;
                other.digits = detail::DigitAllocator::allocate(other.length = other.capacity = 1);
                *other.digits = 0;
            }
        }
        return *this;
    }

    UnsignedInteger& operator=(const SignedInteger& other);

    template <typename unsignedIntegral, typename std::enable_if<std::is_unsigned<unsignedIntegral>::value>::type* = nullptr>
    UnsignedInteger& operator=(unsignedIntegral value) {
        if (length = 0, capacity < (std::numeric_limits<unsignedIntegral>::digits10 + 7) >> 3)
            detail::DigitAllocator::deallocate(digits, capacity), digits = detail::DigitAllocator::allocate(capacity = (std::numeric_limits<unsignedIntegral>::digits10 + 7) >> 3);
        while (digits[length++] = std::uint32_t(value) % Base, value /= unsignedIntegral(Base));
        return *this;
    }

    template <typename signedIntegral, typename std::enable_if<std::is_signed<signedIntegral>::value && !std::is_floating_point<signedIntegral>::value>::type* = nullptr>
    UnsignedInteger& operator=(signedIntegral value) {
        VALIDITY_CHECK(value >= 0, std::invalid_argument, "UnsignedInteger operator= error: the provided signed integer value = " + std::to_string(value) + " is negative. UnsignedInteger can only represent non-negative integers.")
        if (length = 0, capacity < (std::numeric_limits<signedIntegral>::digits10 + 7) >> 3)
            detail::DigitAllocator::deallocate(digits, capacity), digits = detail::DigitAllocator::allocate(capacity = (std::numeric_limits<signedIntegral>::digits10 + 7) >> 3);
        while (digits[length++] = std::uint32_t(value) % Base, value /= signedIntegral(Base));
        return *this;
    }

    template <typename floatingPoint, typename std::enable_if<std::is_floating_point<floatingPoint>::value>::type* = nullptr>
    UnsignedInteger& operator=(floatingPoint value) {
        VALIDITY_CHECK(value >= 0, std::invalid_argument, "UnsignedInteger operator= error: the provided floating point value = " + std::to_string(value) + " is negative. UnsignedInteger can only represent non-negative integers.")
        VALIDITY_CHECK(std::isfinite(value), std::invalid_argument, "UnsignedInteger operator= error: the provided floating point value = " + std::to_string(value) + " is not finite.")
        if (length = 0, capacity < (std::numeric_limits<floatingPoint>::max_exponent10 + 7) >> 3)
            detail::DigitAllocator::deallocate(digits, capacity), digits = detail::DigitAllocator::allocate(capacity = (std::numeric_limits<floatingPoint>::max_exponent10 + 7) >> 3);
        while (digits[length++] = std::uint32_t(std::fmod(value, Base)), (value = std::floor(value / Base)));
        return *this;
    }

    UnsignedInteger& operator=(const char* value) {
        VALIDITY_CHECK(value, std::invalid_argument, "UnsignedInteger operator= error: the provided C-style string is a null pointer.")
        const std::uint32_t stringLength = std::uint32_t(std::strlen(value));
        VALIDITY_CHECK(stringLength, std::invalid_argument, "UnsignedInteger operator= error: the provided C-style string is empty. UnsignedInteger can only be constructed from non-empty strings containing only digits.")
        VALIDITY_CHECK(std::all_of(value, value + stringLength, [](char digit) -> bool { return std::isdigit(digit); }), std::invalid_argument, "UnsignedInteger operator= error: the provided C-style string value = "
                                                                                                                                                " + std::string(value) + "
                                                                                                                                                " contains non-digit characters. UnsignedInteger can only be constructed from non-empty strings containing only digits.")
        if (capacity < (length = (stringLength + 7) >> 3))
            detail::DigitAllocator::deallocate(digits, capacity), digits = detail::DigitAllocator::allocate(capacity = length);
        return construct(value, stringLength), *this;
    }

    UnsignedInteger& operator=(const std::string& value) {
        VALIDITY_CHECK(value.size(), std::invalid_argument, "UnsignedInteger operator= error: the provided string is empty. UnsignedInteger can only be constructed from non-empty strings containing only digits.")
        VALIDITY_CHECK(std::all_of(value.begin(), value.end(), [](char digit) -> bool { return std::isdigit(digit); }), std::invalid_argument, "UnsignedInteger operator= error: the provided string value = "
                                                                                                                                               " + value + "
                                                                                                                                               " contains non-digit characters. UnsignedInteger can only be constructed from non-empty strings containing only digits.")
        if (capacity < (length = std::uint32_t(value.size() + 7) >> 3))
            detail::DigitAllocator::deallocate(digits, capacity), digits = detail::DigitAllocator::allocate(capacity = length);
        return construct(value.data(), std::uint32_t(value.size())), *this;
    }

    friend std::istream& operator>>(std::istream& stream, UnsignedInteger& destination) {
        std::string buffer;
        return stream >> buffer, destination = buffer, stream;
    }

    friend std::ostream& operator<<(std::ostream& stream, const UnsignedInteger& source) {
        return stream << static_cast<const char*>(source);
    }

    template <typename unsignedIntegral, typename std::enable_if<std::is_unsigned<unsignedIntegral>::value>::type* = nullptr>
    operator unsignedIntegral() const {
        unsignedIntegral result = 0;
        // OPT: B=10^8=2^8*5^8, B^k ≡ 0 (mod 2^(8*sizeof(T))) 当 k≥sizeof(T)。
        // 超过 sizeof(T) 个的高位 digit 对结果无贡献，跳过。
        std::uint32_t* start = (length > sizeof(unsignedIntegral)) ? digits + length - sizeof(unsignedIntegral) : digits;
        for (std::uint32_t* i = digits + length; i != start; result = result * unsignedIntegral(Base) + unsignedIntegral(*--i));
        return result;
    }

    template <typename signedIntegral, typename std::enable_if<std::is_signed<signedIntegral>::value && !std::is_floating_point<signedIntegral>::value>::type* = nullptr>
    operator signedIntegral() const {
        using UnsignedT = typename std::make_unsigned<signedIntegral>::type;
        UnsignedT result = 0;
        std::uint32_t* start = (length > sizeof(UnsignedT)) ? digits + length - sizeof(UnsignedT) : digits;
        for (std::uint32_t* i = digits + length; i != start; result = result * UnsignedT(Base) + UnsignedT(*--i));
        return static_cast<signedIntegral>(result);
    }

    template <typename floatingPoint, typename std::enable_if<std::is_floating_point<floatingPoint>::value>::type* = nullptr>
    operator floatingPoint() const {
        floatingPoint result = 0;
        // OPT: 大数时 result 溢出为 infinity，后续 infinity*B+d=infinity 是空操作，提前退出。
        for (std::uint32_t* i = digits + length; i != digits; ) {
            result = result * floatingPoint(Base) + floatingPoint(*--i);
            if (UNLIKELY(!std::isfinite(result))) break;
        }
        return result;
    }

    explicit operator const char*() const {
        thread_local char* result = nullptr;
        thread_local std::uint32_t resultLength = 0;
        if (!length) {
            if (resultLength < 2)
                delete[] result, result = new char[resultLength = 2];
            result[0] = '0';
            result[1] = 0;
            return result;
        }
        if (resultLength < (length << 3 | 7))
            delete[] result, result = new char[resultLength = length << 3 | 7];
        char* resultPointer = result + 8;
        std::uint32_t *i = digits + length, numberLength = 0;
        for (std::uint32_t number = *--i; *--resultPointer = char(48 | number % 10), ++numberLength, number /= 10;);
        for (std::memmove(result, resultPointer, numberLength), resultPointer = result + numberLength; i-- != digits; std::memcpy(resultPointer, detail::O(*i / 10000), 4), resultPointer += 4, std::memcpy(resultPointer, detail::O(*i % 10000), 4), resultPointer += 4);
        return *resultPointer = 0, result;
    }

    operator std::string() const {
        if (!length)
            return "0";
        std::uint32_t* i = digits + length;
        std::string result = std::to_string(*--i);
        for (result.reserve(length << 3); i-- != digits; result.append(detail::O(*i / 10000), 4), result.append(detail::O(*i % 10000), 4));
        return result;
    }

    operator bool() const noexcept {
        return length != 1 || *digits;
    }

#if __cplusplus >= 202002L
    std::strong_ordering operator<=>(const UnsignedInteger& other) const {
        return length == other.length ? reverseCompare(other) <=> 0 : length <=> other.length;
    }
#endif

    [[nodiscard]] bool operator==(const UnsignedInteger& other) const noexcept {
        return length == other.length && !std::memcmp(digits, other.digits, length << 2);
    }

    [[nodiscard]] bool operator!=(const UnsignedInteger& other) const noexcept {
        return length != other.length || std::memcmp(digits, other.digits, length << 2);
    }

    [[nodiscard]] bool operator<(const UnsignedInteger& other) const noexcept {
        return length < other.length || (length == other.length && reverseCompare(other) < 0);
    }

    [[nodiscard]] bool operator>(const UnsignedInteger& other) const noexcept {
        return length > other.length || (length == other.length && reverseCompare(other) > 0);
    }

    [[nodiscard]] bool operator<=(const UnsignedInteger& other) const noexcept {
        return length < other.length || (length == other.length && reverseCompare(other) <= 0);
    }

    [[nodiscard]] bool operator>=(const UnsignedInteger& other) const noexcept {
        return length > other.length || (length == other.length && reverseCompare(other) >= 0);
    }

    UnsignedInteger& operator+=(const UnsignedInteger& other) {
        // 修复：总是预留一位进位空间（原代码仅当 length<=other.length 时 resize，
        // 当 length>other.length 且进位传到最高位使其>=Base 时，第二个循环因
        // thisEnd=digits+length-1 跳过最高位，导致 digits[length-1] 保持为 Base，
        // writeUnsigned 输出 9 位覆盖前一行换行符）
        const std::uint32_t maxLen = std::max(length, other.length);
        const std::uint32_t oldLength = length;
        resize(maxLen + 1), std::memset(digits + oldLength, 0, (length - oldLength) << 2);
        std::uint32_t *thisDigit = digits, *thisEnd = digits + length - 1;
        for (std::uint32_t *otherDigit = other.digits, *otherEnd = other.digits + other.length; otherDigit != otherEnd; ++thisDigit, ++otherDigit)
            if ((*thisDigit += *otherDigit) >= Base)
                *thisDigit -= Base, ++*(thisDigit + 1);
        for (; thisDigit != thisEnd && *thisDigit >= Base; *thisDigit -= Base, ++*++thisDigit);
        for (; length > 1 && !digits[length - 1]; --length);
        return *this;
    }

    // OPT: 直接构造 result，避免 UnsignedInteger(*this) 的拷贝
    [[nodiscard]] UnsignedInteger operator+(const UnsignedInteger& other) const {
        const UnsignedInteger& longer = length >= other.length ? *this : other;
        const UnsignedInteger& shorter = length >= other.length ? other : *this;
        UnsignedInteger result(longer.length + 1, longer.length + 1);
        std::memcpy(result.digits, longer.digits, longer.length << 2);
        result.digits[longer.length] = 0;
        std::uint32_t *r = result.digits, *rEnd = result.digits + result.length - 1;
        for (std::uint32_t *s = shorter.digits, *sEnd = shorter.digits + shorter.length; s != sEnd; ++r, ++s)
            if (UNLIKELY((*r += *s) >= Base))
                *r -= Base, ++*(r + 1);
        for (; r != rEnd && UNLIKELY(*r >= Base); *r -= Base, ++*++r);
        for (; result.length > 1 && !result.digits[result.length - 1]; --result.length);
        return result;
    }

    UnsignedInteger& operator++() {
        std::uint32_t *thisDigit = digits, *thisEnd = digits + length - 1;
        for (++*thisDigit; thisDigit != thisEnd && *thisDigit >= Base; *thisDigit -= Base, ++*++thisDigit);
        if (thisDigit == thisEnd && *thisDigit >= Base)
            resize(length + 1), digits[length - 2] -= Base, digits[length - 1] = 1;
        return *this;
    }

    UnsignedInteger operator++(int) {
        UnsignedInteger result = *this;
        return ++*this, result;
    }

    UnsignedInteger& operator-=(const UnsignedInteger& other) {
        VALIDITY_CHECK(
            *this >= other,
            std::invalid_argument,
            std::string("UnsignedInteger subtraction error: attempted to subtract a larger UnsignedInteger ") +
                other.operator std::string() +
                " from a smaller one " +
                this->operator std::string() +
                ".")
        std::uint32_t *thisDigit = digits, *thisEnd = digits + length;
        for (std::uint32_t *otherDigit = other.digits, *otherEnd = other.digits + other.length; otherDigit != otherEnd; ++thisDigit, ++otherDigit)
            if ((*thisDigit -= *otherDigit) >= Base)
                *thisDigit += Base, --*(thisDigit + 1);
        for (; thisDigit != thisEnd && *thisDigit >= Base; *thisDigit += Base, --*++thisDigit);
        for (; length > 1 && !digits[length - 1]; --length);
        return *this;
    }

    // OPT: 直接构造 result，避免 UnsignedInteger(*this) 的拷贝
    [[nodiscard]] UnsignedInteger operator-(const UnsignedInteger& other) const {
        VALIDITY_CHECK(
            *this >= other,
            std::invalid_argument,
            std::string("UnsignedInteger subtraction error: attempted to subtract a larger UnsignedInteger ") +
                other.operator std::string() +
                " from a smaller one " +
                this->operator std::string() +
                ".")
        UnsignedInteger result(length, length);
        std::memcpy(result.digits, digits, length << 2);
        std::uint32_t *r = result.digits, *rEnd = result.digits + result.length;
        for (std::uint32_t *o = other.digits, *oEnd = other.digits + other.length; o != oEnd; ++r, ++o)
            if (UNLIKELY((*r -= *o) >= Base))
                *r += Base, --*(r + 1);
        for (; r != rEnd && UNLIKELY(*r >= Base); *r += Base, --*++r);
        for (; result.length > 1 && !result.digits[result.length - 1]; --result.length);
        return result;
    }

    UnsignedInteger& operator--() {
        VALIDITY_CHECK(bool(*this), std::invalid_argument, "UnsignedInteger decrement error: value is already zero.")
        std::uint32_t *thisDigit = digits, *thisEnd = digits + length - 1;
        for (--*thisDigit; thisDigit != thisEnd && *thisDigit >= Base; *thisDigit += Base, --*++thisDigit);
        return *this;
    }

    UnsignedInteger operator--(int) {
        UnsignedInteger result = *this;
        return --*this, result;
    }

    UnsignedInteger& operator*=(const UnsignedInteger& other) {
        // OPT: 不平衡乘法拆分。当 *this 远长于 other 时，拆成多个 other.length 规模的乘法。
        // 例: 2n×n → 两个 n×n，FFT 长度从 4n 降到 2n。大规模时 4n 数据超 L2，拆分收益显著。
#if defined(__AVX2__)
        // OPT: 不平衡乘法拆分 + DIF(other) 缓存。2n×n 拆成两个 n×n，
        // FFT 长度从 4n 降到 2n；且只对 other 做一次 DIF，所有 full chunk 复用。
        // pointwise 只写 firstArray 不写 secondArray，故 cachedSecond 可安全复用。
        if (other.length >= UnbalancedThreshold && (length >= other.length * 2 || (length >= other.length * 3 / 2 && other.length >= 50000))) {
            const std::uint32_t chunkSize = other.length;
            UnsignedInteger result(length + other.length, length + other.length);
            std::memset(result.digits, 0, result.length << 2);
            const std::uint32_t fullResultLen = chunkSize * 2;
            const std::uint32_t transformLength = 2u << detail::log2(fullResultLen - 1);
            detail::T.resize(transformLength);
            thread_local __m128d *cachedFirst = nullptr, *cachedSecond = nullptr;
            thread_local std::uint32_t cachedSize = 0;
            if (cachedSize < transformLength)
                delete[] cachedFirst, delete[] cachedSecond,
                cachedFirst = new __m128d[transformLength],
                cachedSecond = new __m128d[transformLength],
                cachedSize = transformLength;
            // DIF(other) 只算一次，所有 full chunk 复用
            std::memset(cachedSecond + chunkSize, 0, (transformLength - chunkSize) * sizeof(__m128d));
            for (std::uint32_t i = 0; i != chunkSize; ++i) {
                const std::uint32_t hi = other.digits[i] / 10000u, lo = other.digits[i] - hi * 10000u;
                cachedSecond[i] = _mm_set_pd(static_cast<double>(hi), static_cast<double>(lo));
            }
            detail::T.decimationInFrequency(cachedSecond, transformLength);
            std::uint32_t pos = length;
            while (pos > 0) {
                std::uint32_t take = (pos >= chunkSize) ? chunkSize : pos;
                std::uint32_t offset = pos - take;
                if (take == chunkSize) {
                    // 快速路径：复用缓存的 DIF(other)，省去 chunk 构造/trim/重复 DIF
                    std::memset(cachedFirst + chunkSize, 0, (transformLength - chunkSize) * sizeof(__m128d));
                    for (std::uint32_t i = 0; i != chunkSize; ++i) {
                        const std::uint32_t hi = digits[offset + i] / 10000u, lo = digits[offset + i] - hi * 10000u;
                        cachedFirst[i] = _mm_set_pd(static_cast<double>(hi), static_cast<double>(lo));
                    }
                    detail::T.decimationInFrequency(cachedFirst, transformLength);
                    detail::T.frequencyDomainPointwiseMultiply(cachedFirst, cachedSecond, transformLength);
                    detail::T.decimationInTime(cachedFirst, transformLength);
                    std::uint64_t carry = 0;
                    std::uint32_t writePos = offset;
                    for (std::uint32_t i = 0; i != fullResultLen; ++i) {
                        __m128d v = cachedFirst[i];
                        double realPart = _mm_cvtsd_f64(v);
                        double imagPart = _mm_cvtsd_f64(_mm_unpackhi_pd(v, v));
                        carry += std::uint64_t(std::int64_t(realPart + 0.5) + std::int64_t(imagPart + 0.5) * 10000) + result.digits[writePos];
                        result.digits[writePos++] = std::uint32_t(carry % Base), carry /= Base;
                    }
                    while (carry && writePos < result.length) {
                        carry += result.digits[writePos];
                        result.digits[writePos++] = std::uint32_t(carry % Base), carry /= Base;
                    }
                } else if (take + chunkSize <= transformLength) {
                    // OPT: 末尾小块复用 cachedSecond 的 DIF(other)，省去重复 DIF
                    std::memset(cachedFirst + take, 0, (transformLength - take) * sizeof(__m128d));
                    for (std::uint32_t i = 0; i != take; ++i) {
                        const std::uint32_t hi = digits[offset + i] / 10000u, lo = digits[offset + i] - hi * 10000u;
                        cachedFirst[i] = _mm_set_pd(static_cast<double>(hi), static_cast<double>(lo));
                    }
                    detail::T.decimationInFrequency(cachedFirst, transformLength);
                    detail::T.frequencyDomainPointwiseMultiply(cachedFirst, cachedSecond, transformLength);
                    detail::T.decimationInTime(cachedFirst, transformLength);
                    std::uint64_t carry = 0;
                    std::uint32_t writePos = offset;
                    const std::uint32_t tailResultLen = take + chunkSize;
                    for (std::uint32_t i = 0; i != tailResultLen; ++i) {
                        __m128d v = cachedFirst[i];
                        double realPart = _mm_cvtsd_f64(v);
                        double imagPart = _mm_cvtsd_f64(_mm_unpackhi_pd(v, v));
                        carry += std::uint64_t(std::int64_t(realPart + 0.5) + std::int64_t(imagPart + 0.5) * 10000) + result.digits[writePos];
                        result.digits[writePos++] = std::uint32_t(carry % Base), carry /= Base;
                    }
                    while (carry && writePos < result.length) {
                        carry += result.digits[writePos];
                        result.digits[writePos++] = std::uint32_t(carry % Base), carry /= Base;
                    }
                } else {
                    // 末尾小块走通用路径（transformLength 不同，无法复用缓存）
                    UnsignedInteger chunk(take, take);
                    std::memcpy(chunk.digits, digits + offset, take << 2);
                    for (; chunk.length > 1 && !chunk.digits[chunk.length - 1]; --chunk.length);
                    UnsignedInteger part = chunk * other;
                    std::uint64_t carry = 0;
                    std::uint32_t writePos = offset;
                    for (std::uint32_t i = 0; i < part.length; ++i) {
                        carry += result.digits[writePos] + part.digits[i];
                        result.digits[writePos++] = std::uint32_t(carry % Base), carry /= Base;
                    }
                    while (carry && writePos < result.length) {
                        carry += result.digits[writePos];
                        result.digits[writePos++] = std::uint32_t(carry % Base), carry /= Base;
                    }
                }
                pos = offset;
            }
            for (; result.length > 1 && !result.digits[result.length - 1]; --result.length);
            return *this = std::move(result);
        }
#else
        if (other.length >= BruteforceThreshold && (length >= other.length * 2 || (length >= other.length * 3 / 2 && other.length >= 50000))) {
            const std::uint32_t chunkSize = other.length;
            UnsignedInteger result(length + other.length, length + other.length);
            std::memset(result.digits, 0, result.length << 2);
            std::uint32_t pos = length;
            while (pos > 0) {
                std::uint32_t take = (pos >= chunkSize) ? chunkSize : pos;
                std::uint32_t offset = pos - take;
                UnsignedInteger chunk(take, take);
                std::memcpy(chunk.digits, digits + offset, take << 2);
                for (; chunk.length > 1 && !chunk.digits[chunk.length - 1]; --chunk.length);
                UnsignedInteger part = chunk * other;
                std::uint64_t carry = 0;
                std::uint32_t writePos = offset;
                for (std::uint32_t i = 0; i < part.length; ++i) {
                    carry += result.digits[writePos] + part.digits[i];
                    result.digits[writePos++] = std::uint32_t(carry % Base), carry /= Base;
                }
                while (carry && writePos < result.length) {
                    carry += result.digits[writePos];
                    result.digits[writePos++] = std::uint32_t(carry % Base), carry /= Base;
                }
                pos = offset;
            }
            for (; result.length > 1 && !result.digits[result.length - 1]; --result.length);
            return *this = std::move(result);
        }
#endif
        if (length < BruteforceThreshold || other.length < BruteforceThreshold) {
            UnsignedInteger result(length + other.length - 1, length + other.length);
            std::uint64_t carry = 0;
            for (std::uint32_t i = 0; i != result.length; result.digits[i++] = std::uint32_t(carry % Base), carry /= Base)
                for (std::uint32_t j = (i >= length ? i - length + 1 : 0); j <= i && j < other.length; ++j)
                    carry += std::uint64_t(digits[i - j]) * other.digits[j];
            if (carry)
                result.digits[result.length] = std::uint32_t(carry), ++result.length;
            for (; result.length > 1 && !result.digits[result.length - 1]; --result.length);
            return *this = std::move(result);
        }
        VALIDITY_CHECK(length <= TransformLimit, std::invalid_argument, "UnsignedInteger multiplication error: left operand length (" + std::to_string(length) + ") exceeds Transform limit (" + std::to_string(TransformLimit) + ").")
        VALIDITY_CHECK(other.length <= TransformLimit, std::invalid_argument, "UnsignedInteger multiplication error: right operand length (" + std::to_string(other.length) + ") exceeds Transform limit (" + std::to_string(TransformLimit) + ").")
#if defined(__AVX2__) || defined(__ARM_NEON__)
        const std::uint32_t resultLength = length + other.length, transformLength = 2u << detail::log2(resultLength - 1);
#if defined(__AVX2__)
        thread_local __m128d *firstArray = nullptr, *secondArray = nullptr;
        thread_local std::uint32_t allocatedSize = 0;
        if (allocatedSize < transformLength)
            delete[] firstArray, delete[] secondArray, firstArray = new __m128d[transformLength], secondArray = new __m128d[transformLength], allocatedSize = transformLength;
        std::memset(firstArray + length, 0, (transformLength - length) * sizeof(__m128d)), std::memset(secondArray + other.length, 0, (transformLength - other.length) * sizeof(__m128d));
        // OPT 跨 I/O↔FFT：整数 div 替代浮点 floor+div，一次除法喂 hi/lo
        for (std::uint32_t i = 0; i != length; ++i) {
            const std::uint32_t hi = digits[i] / 10000u, lo = digits[i] - hi * 10000u;
            firstArray[i] = _mm_set_pd(static_cast<double>(hi), static_cast<double>(lo));
        }
        for (std::uint32_t i = 0; i != other.length; ++i) {
            const std::uint32_t hi = other.digits[i] / 10000u, lo = other.digits[i] - hi * 10000u;
            secondArray[i] = _mm_set_pd(static_cast<double>(hi), static_cast<double>(lo));
        }
        detail::T.resize(transformLength), detail::T.decimationInFrequency(firstArray, transformLength), detail::T.decimationInFrequency(secondArray, transformLength);
        detail::T.frequencyDomainPointwiseMultiply(firstArray, secondArray, transformLength), detail::T.decimationInTime(firstArray, transformLength);
#else
        thread_local float64x2_t *firstArray = nullptr, *secondArray = nullptr;
        thread_local std::uint32_t allocatedSize = 0;
        if (allocatedSize < transformLength)
            delete[] firstArray, delete[] secondArray, firstArray = new float64x2_t[transformLength](), secondArray = new float64x2_t[transformLength](), allocatedSize = transformLength;
        std::memset(firstArray, 0, transformLength * sizeof(float64x2_t)), std::memset(secondArray, 0, transformLength * sizeof(float64x2_t));
        for (std::uint32_t i = 0; i != length; ++i) {
            float64x2_t v = vdupq_n_f64(0.0);
            v = vsetq_lane_f64(static_cast<double>(digits[i] % 10000u), v, 0);
            v = vsetq_lane_f64(std::floor(static_cast<double>(digits[i]) / 10000.0), v, 1);
            firstArray[i] = v;
        }
        for (std::uint32_t i = 0; i != other.length; ++i) {
            float64x2_t v = vdupq_n_f64(0.0);
            v = vsetq_lane_f64(static_cast<double>(other.digits[i] % 10000u), v, 0);
            v = vsetq_lane_f64(std::floor(static_cast<double>(other.digits[i]) / 10000.0), v, 1);
            secondArray[i] = v;
        }
        detail::T.resize(transformLength), detail::T.decimationInFrequency(firstArray, transformLength), detail::T.decimationInFrequency(secondArray, transformLength);
        detail::T.frequencyDomainPointwiseMultiply(firstArray, secondArray, transformLength), detail::T.decimationInTime(firstArray, transformLength);
#endif
        UnsignedInteger result(resultLength, resultLength);
        std::uint64_t carry = 0;
#if defined(__AVX2__)
        for (std::uint32_t i = 0; i != resultLength; ++i) {
            __m128d v = firstArray[i];
            double realPart = _mm_cvtsd_f64(v);
            double imagPart = _mm_cvtsd_f64(_mm_unpackhi_pd(v, v));
            carry += std::uint64_t(std::int64_t(realPart + 0.5) + std::int64_t(imagPart + 0.5) * 10000);
            result.digits[i] = std::uint32_t(carry % Base), carry /= Base;
        }
#else
        for (std::uint32_t i = 0; i != resultLength; ++i) {
            float64x2_t v = firstArray[i];
            double realPart = vgetq_lane_f64(v, 0);
            double imagPart = vgetq_lane_f64(v, 1);
            carry += std::uint64_t(std::int64_t(realPart + 0.5) + std::int64_t(imagPart + 0.5) * 10000);
            result.digits[i] = std::uint32_t(carry % Base), carry /= Base;
        }
#endif
#else
        thread_local std::complex<double>*firstArray = nullptr, *secondArray = nullptr;
        thread_local std::uint32_t allocatedSize = 0;
        const std::uint32_t resultLength = length + other.length, transformLength = 2u << detail::log2(resultLength - 1);
        if (allocatedSize < transformLength)
            delete[] firstArray, delete[] secondArray, firstArray = new std::complex<double>[transformLength](), secondArray = new std::complex<double>[transformLength](), allocatedSize = transformLength;
        std::memset(firstArray, 0, transformLength * sizeof(std::complex<double>)), std::memset(secondArray, 0, transformLength * sizeof(std::complex<double>));
        for (std::uint32_t i = 0; i != length; ++i)
            firstArray[i] = {static_cast<double>(digits[i] % 10000u), std::floor(static_cast<double>(digits[i]) / 10000.0)};
        for (std::uint32_t i = 0; i != other.length; ++i)
            secondArray[i] = {static_cast<double>(other.digits[i] % 10000u), std::floor(static_cast<double>(other.digits[i]) / 10000.0)};
        detail::T.resize(transformLength), detail::T.decimationInFrequency(firstArray, transformLength), detail::T.decimationInFrequency(secondArray, transformLength);
        detail::T.frequencyDomainPointwiseMultiply(firstArray, secondArray, transformLength), detail::T.decimationInTime(firstArray, transformLength);
        UnsignedInteger result(resultLength, resultLength);
        std::uint64_t carry = 0;
        for (std::uint32_t i = 0; i != resultLength; ++i) {
            carry += std::uint64_t(std::int64_t(firstArray[i].real() + 0.5) + std::int64_t(firstArray[i].imag() + 0.5) * 10000);
            result.digits[i] = std::uint32_t(carry % Base), carry /= Base;
        }
#endif
        for (; carry && result.length != result.capacity; result.digits[result.length++] = std::uint32_t(carry % Base), carry /= Base)
            if (result.length == result.capacity)
                result.resize(result.capacity << 1);
        for (; result.length > 1 && !result.digits[result.length - 1]; --result.length);
        return *this = std::move(result);
    }

    // OPT: 平方专用路径。operator* 拦截 self-multiply 后调此方法，
    // AVX2 下只填一个数组、用 frequencyDomainPointwiseSquare，省一次 DIF + 一半点乘。
    UnsignedInteger square() const {
#if defined(__AVX2__)
        if (length < BruteforceThreshold) {
            UnsignedInteger r(*this);
            return std::move(r *= *this);
        }
        VALIDITY_CHECK(length <= TransformLimit, std::invalid_argument, "UnsignedInteger square error: operand length (" + std::to_string(length) + ") exceeds Transform limit (" + std::to_string(TransformLimit) + ").")
        const std::uint32_t resultLength = length * 2, transformLength = 2u << detail::log2(resultLength - 1);
        thread_local __m128d *sqArray = nullptr;
        thread_local std::uint32_t sqAllocated = 0;
        if (sqAllocated < transformLength)
            delete[] sqArray, sqArray = new __m128d[transformLength], sqAllocated = transformLength;
        std::memset(sqArray + length, 0, (transformLength - length) * sizeof(__m128d));
        for (std::uint32_t i = 0; i != length; ++i) {
            const std::uint32_t hi = digits[i] / 10000u, lo = digits[i] - hi * 10000u;
            sqArray[i] = _mm_set_pd(static_cast<double>(hi), static_cast<double>(lo));
        }
        detail::T.resize(transformLength);
        detail::T.decimationInFrequency(sqArray, transformLength);
        detail::T.frequencyDomainPointwiseSquare(sqArray, transformLength);
        detail::T.decimationInTime(sqArray, transformLength);
        UnsignedInteger result(resultLength, resultLength);
        std::uint64_t carry = 0;
        for (std::uint32_t i = 0; i != resultLength; ++i) {
            __m128d v = sqArray[i];
            double realPart = _mm_cvtsd_f64(v);
            double imagPart = _mm_cvtsd_f64(_mm_unpackhi_pd(v, v));
            carry += std::uint64_t(std::int64_t(realPart + 0.5) + std::int64_t(imagPart + 0.5) * 10000);
            result.digits[i] = std::uint32_t(carry % Base), carry /= Base;
        }
        for (; carry && result.length != result.capacity; result.digits[result.length++] = std::uint32_t(carry % Base), carry /= Base)
            if (result.length == result.capacity)
                result.resize(result.capacity << 1);
        for (; result.length > 1 && !result.digits[result.length - 1]; --result.length);
        return result;
#else
        UnsignedInteger r(*this);
        return std::move(r *= *this);
#endif
    }

    [[nodiscard]] UnsignedInteger operator*(const UnsignedInteger& other) const {
        if (this == &other) return square();
        return UnsignedInteger(*this) *= other;
    }

    UnsignedInteger& operator/=(const UnsignedInteger& other) {
        VALIDITY_CHECK(bool(other), std::invalid_argument, "UnsignedInteger division error: divisor is zero.")
        return *this = std::move(divisionAndModulus(other).first);
    }

    [[nodiscard]] UnsignedInteger operator/(const UnsignedInteger& other) const {
        return UnsignedInteger(*this) /= other;
    }

    UnsignedInteger& operator%=(const UnsignedInteger& other) {
        VALIDITY_CHECK(bool(other), std::invalid_argument, "UnsignedInteger modulus error: modulus is zero.")
        return *this = std::move(divisionAndModulus(other).second);
    }

    [[nodiscard]] UnsignedInteger operator%(const UnsignedInteger& other) const {
        return UnsignedInteger(*this) %= other;
    }
};

inline UnsignedInteger operator""_UI(const char* literal, std::size_t) {
    return UnsignedInteger(literal);
}

class SignedInteger {
public:
    // OPT: 改为 public 以支持 LC 提交的 mmap+oBuffer 零拷贝 I/O（与原作者一致）
    UnsignedInteger absolute;
    bool sign;

  protected:
    SignedInteger(const UnsignedInteger& initialAbsolute, bool initialSign) : absolute(initialAbsolute), sign(initialSign) {}

  public:
    friend class UnsignedInteger;
    SignedInteger() : absolute(), sign() {}

    SignedInteger(const SignedInteger& other) : absolute(other.absolute), sign(other.sign) {}

    SignedInteger(SignedInteger&& other) noexcept : absolute(std::move(other.absolute)), sign(other.sign) {}

    SignedInteger(const UnsignedInteger& other) : absolute(other), sign() {}

    template <typename unsignedIntegral, typename std::enable_if<std::is_unsigned<unsignedIntegral>::value>::type* = nullptr>
    SignedInteger(unsignedIntegral value) : absolute(value), sign() {}

    template <typename signedIntegral, typename std::enable_if<std::is_signed<signedIntegral>::value && !std::is_floating_point<signedIntegral>::value>::type* = nullptr>
    SignedInteger(signedIntegral value) : absolute(std::abs(value)), sign(value < 0) {}

    template <typename floatingPoint, typename std::enable_if<std::is_floating_point<floatingPoint>::value>::type* = nullptr>
    SignedInteger(floatingPoint value) : absolute(std::abs(value)), sign(value < 0) {}

    SignedInteger(const char* value) {
        VALIDITY_CHECK(value, std::invalid_argument, "SignedInteger constructor error: the provided C-style string is a null pointer.")
        VALIDITY_CHECK(*value, std::invalid_argument, "SignedInteger constructor error: the provided C-style string is empty. SignedInteger can only be constructed from non-empty strings containing only digits (optionally prefixed with '-').")
        VALIDITY_CHECK(std::all_of(value + (*value == '-'), value + std::strlen(value), [](char digit) -> bool { return std::isdigit(digit); }), std::invalid_argument, "SignedInteger constructor error: the provided C-style string value = "
                                                                                                                                                                        " + std::string(value) + "
                                                                                                                                                                        " contains non-digit characters. SignedInteger can only be constructed from non-empty strings containing only digits (optionally prefixed with '-').")
        absolute = value + (sign = *value == '-'), sign = sign && bool(absolute);
    }

    SignedInteger(const std::string& value) {
        VALIDITY_CHECK(value.size(), std::invalid_argument, "SignedInteger constructor error: the provided string is empty. SignedInteger can only be constructed from non-empty strings containing only digits (optionally prefixed with '-').")
        VALIDITY_CHECK(std::all_of(value.begin() + (value.front() == '-'), value.end(), [](char digit) -> bool { return std::isdigit(digit); }), std::invalid_argument, "SignedInteger constructor error: the provided string value = "
                                                                                                                                                                        " + value + "
                                                                                                                                                                        " contains non-digit characters. SignedInteger can only be constructed from non-empty strings containing only digits (optionally prefixed with '-').")
        absolute = value.data() + (sign = value.front() == '-'), sign = sign && bool(absolute);
    }

    ~SignedInteger() = default;

    SignedInteger& operator=(const SignedInteger& other) {
        return absolute = other.absolute, sign = other.sign, *this;
    }

    SignedInteger& operator=(SignedInteger&& other) noexcept {
        return absolute = std::move(other.absolute), sign = other.sign, *this;
    }

    SignedInteger& operator=(const UnsignedInteger& other) {
        return absolute = other, sign = false, *this;
    }

    template <typename unsignedIntegral, typename std::enable_if<std::is_unsigned<unsignedIntegral>::value>::type* = nullptr>
    SignedInteger& operator=(unsignedIntegral value) {
        return absolute = value, sign = false, *this;
    }

    template <typename signedIntegral, typename std::enable_if<std::is_signed<signedIntegral>::value && !std::is_floating_point<signedIntegral>::value>::type* = nullptr>
    SignedInteger& operator=(signedIntegral value) {
        return absolute = std::abs(value), sign = value < 0, *this;
    }

    template <typename floatingPoint, typename std::enable_if<std::is_floating_point<floatingPoint>::value>::type* = nullptr>
    SignedInteger& operator=(floatingPoint value) {
        return absolute = std::abs(value), sign = value < 0, *this;
    }

    SignedInteger& operator=(const char* value) {
        VALIDITY_CHECK(value, std::invalid_argument, "SignedInteger operator= error: the provided C-style string is a null pointer.")
        VALIDITY_CHECK(*value, std::invalid_argument, "SignedInteger operator= error: the provided C-style string is empty. SignedInteger can only be assigned from non-empty strings containing only digits (optionally prefixed with '-').")
        VALIDITY_CHECK(std::all_of(value + (*value == '-'), value + std::strlen(value), [](char digit) -> bool { return std::isdigit(digit); }), std::invalid_argument, "SignedInteger operator= error: the provided C-style string value = "
                                                                                                                                                                        " + std::string(value) + "
                                                                                                                                                                        " contains non-digit characters. SignedInteger can only be assigned from non-empty strings containing only digits (optionally prefixed with '-').")
        return absolute = value + (sign = *value == '-'), sign = sign && bool(absolute), *this;
    }

    SignedInteger& operator=(const std::string& value) {
        VALIDITY_CHECK(value.size(), std::invalid_argument, "SignedInteger operator= error: the provided string is empty. SignedInteger can only be assigned from non-empty strings containing only digits (optionally prefixed with '-').")
        VALIDITY_CHECK(std::all_of(value.begin() + (value.front() == '-'), value.end(), [](char digit) -> bool { return std::isdigit(digit); }), std::invalid_argument, "SignedInteger operator= error: the provided string value = "
                                                                                                                                                                        " + value + "
                                                                                                                                                                        " contains non-digit characters. SignedInteger can only be assigned from non-empty strings containing only digits (optionally prefixed with '-').")
        return absolute = value.data() + (sign = value.front() == '-'), sign = sign && bool(absolute), *this;
    }

    friend std::istream& operator>>(std::istream& stream, SignedInteger& destination) {
        std::string buffer;
        return stream >> buffer, destination = buffer, stream;
    }

    friend std::ostream& operator<<(std::ostream& stream, const SignedInteger& source) {
        return stream << static_cast<const char*>(source);
    }

    template <typename unsignedIntegral, typename std::enable_if<std::is_unsigned<unsignedIntegral>::value>::type* = nullptr>
    operator unsignedIntegral() const {
        VALIDITY_CHECK(!sign, std::invalid_argument, "SignedInteger conversion error: Cannot convert negative SignedInteger to unsigned type.")
        return unsignedIntegral(absolute);
    }

    template <typename signedIntegral, typename std::enable_if<std::is_signed<signedIntegral>::value && !std::is_floating_point<signedIntegral>::value>::type* = nullptr>
    operator signedIntegral() const {
        using UnsignedT = typename std::make_unsigned<signedIntegral>::type;
        UnsignedT magnitude = static_cast<UnsignedT>(absolute);
        if (!sign) return static_cast<signedIntegral>(magnitude);
        UnsignedT twosComplement = UnsignedT(0) - magnitude;
        return static_cast<signedIntegral>(twosComplement);
    }

    template <typename floatingPoint, typename std::enable_if<std::is_floating_point<floatingPoint>::value>::type* = nullptr>
    operator floatingPoint() const {
        return sign ? -floatingPoint(absolute) : floatingPoint(absolute);
    }

    explicit operator const char*() const {
        thread_local char* result = nullptr;
        thread_local std::uint32_t resultLength = 0;
        const char* absoluteString = static_cast<const char*>(absolute);
        std::uint32_t absoluteLength = std::uint32_t(std::strlen(absoluteString));
        if (resultLength < absoluteLength + 5)
            delete[] result, result = new char[resultLength = absoluteLength + 5];
        char* digitStart = sign && bool(absolute) ? (*result = '-', result + 1) : result;
        std::memcpy(digitStart, absoluteString, absoluteLength), *(digitStart + absoluteLength) = 0;
        return result;
    }

    operator std::string() const {
        return sign && bool(absolute) ? "-" + absolute.operator std::string() : absolute.operator std::string();
    }

    operator bool() const noexcept {
        return bool(absolute);
    }

#if __cplusplus >= 202002L
    std::strong_ordering operator<=>(const SignedInteger& other) const {
        return sign != other.sign ? other.sign <=> sign : (sign ? other.absolute <=> absolute : absolute <=> other.absolute);
    }
#endif

    bool operator==(const SignedInteger& other) const {
        return sign == other.sign && absolute == other.absolute;
    }

    bool operator!=(const SignedInteger& other) const {
        return sign != other.sign || absolute != other.absolute;
    }

    bool operator<(const SignedInteger& other) const {
        return sign ? !other.sign || absolute > other.absolute : !other.sign && absolute < other.absolute;
    }

    bool operator>(const SignedInteger& other) const {
        return sign ? other.sign && absolute < other.absolute : other.sign || absolute > other.absolute;
    }

    bool operator<=(const SignedInteger& other) const {
        return sign ? !other.sign || absolute >= other.absolute : !other.sign && absolute <= other.absolute;
    }

    bool operator>=(const SignedInteger& other) const {
        return sign ? other.sign && absolute <= other.absolute : other.sign || absolute >= other.absolute;
    }

    SignedInteger& operator+=(const SignedInteger& other) {
        sign == other.sign ? absolute += other.absolute : (absolute < other.absolute ? sign = !sign, absolute = other.absolute - absolute : absolute -= other.absolute), sign = sign && bool(absolute);
        return *this;
    }

    SignedInteger operator+(const SignedInteger& other) const {
        return SignedInteger(*this) += other;
    }

    SignedInteger& operator-=(const SignedInteger& other) {
        sign != other.sign ? absolute += other.absolute : (absolute < other.absolute ? sign = !sign, absolute = other.absolute - absolute : absolute -= other.absolute), sign = sign && bool(absolute);
        return *this;
    }

    SignedInteger operator-(const SignedInteger& other) const {
        return SignedInteger(*this) -= other;
    }

    SignedInteger& operator*=(const SignedInteger& other) {
        absolute *= other.absolute, sign = (sign ^ other.sign) && bool(absolute);
        return *this;
    }

    SignedInteger operator*(const SignedInteger& other) const {
        return SignedInteger(*this) *= other;
    }

    SignedInteger& operator/=(const SignedInteger& other) {
        VALIDITY_CHECK(bool(other), std::invalid_argument, "SignedInteger division error: divisor is zero.")
        absolute /= other.absolute, sign ^= other.sign, sign = sign && bool(absolute);
        return *this;
    }

    SignedInteger operator/(const SignedInteger& other) const {
        return SignedInteger(*this) /= other;
    }

    SignedInteger& operator%=(const SignedInteger& other) {
        VALIDITY_CHECK(bool(other), std::invalid_argument, "SignedInteger modulus error: modulus is zero.")
        *this -= (*this / other) * other;
        return *this;
    }

    SignedInteger operator%(const SignedInteger& other) const {
        return SignedInteger(*this) %= other;
    }
};

inline SignedInteger operator""_SI(const char* literal, std::size_t) {
    return SignedInteger(literal);
}

inline UnsignedInteger::UnsignedInteger(const SignedInteger& other) : UnsignedInteger(other.absolute) {
    VALIDITY_CHECK(!other.sign, std::invalid_argument, "UnsignedInteger constructor error: the provided SignedInteger is negative. UnsignedInteger can only represent non-negative integers.")
}

inline UnsignedInteger& UnsignedInteger::operator=(const SignedInteger& other) {
    VALIDITY_CHECK(!other.sign, std::invalid_argument, "UnsignedInteger operator= error: the provided SignedInteger is negative. UnsignedInteger can only represent non-negative integers.")
    return *this = other.absolute;
}

#undef VALIDITY_CHECK
#undef __CONSTEXPR
#endif


#ifdef __linux__
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#endif

namespace {
    static char oBuffer[32 << 20], *oCursor = oBuffer;

    static const char* iCursor = []() -> const char* {
#ifdef __linux__
        struct stat status;
        fstat(STDIN_FILENO, &status);
        return reinterpret_cast<const char*>(mmap(nullptr, status.st_size, PROT_READ, MAP_PRIVATE, STDIN_FILENO, 0));
#else
        static char iBuffer[16 << 20];
        std::fread(iBuffer, sizeof(char), sizeof(iBuffer) / sizeof(char), stdin);
        return iBuffer;
#endif
    }();

    void readFromCursor(UnsignedInteger& value) {
        const char* start = iCursor;
        while (true) {
            std::uint64_t data;
            std::memcpy(&data, iCursor, sizeof(data));
            if ((data = (~data) & (data - 0x2121212121212121) & 0x8080808080808080)) {
                const std::uint32_t stringLength = std::uint32_t((iCursor += __builtin_ctzll(data) / int(sizeof(std::uint64_t))) - start);
                if (value.capacity < (value.length = (stringLength + 7) >> 3))
                    detail::DigitAllocator::deallocate(value.digits, value.capacity), value.digits = detail::DigitAllocator::allocate(value.capacity = value.length);
                value.construct(start, stringLength), ++iCursor;
                return;
            }
            iCursor += sizeof(std::uint64_t);
        }
    }

    void writeUnsigned(const UnsignedInteger& a) {
        char* rCursor = oCursor + 8;
        std::uint32_t* i = a.digits + a.length, numberLength = 0;
        for (std::uint32_t number = *--i; *--rCursor = char(48 | number % 10), ++numberLength, number /= 10;);
        for (std::memmove(oCursor, rCursor, numberLength), oCursor += numberLength; i-- != a.digits; std::memcpy(oCursor, detail::O(*i / 10000), 4), oCursor += 4, std::memcpy(oCursor, detail::O(*i % 10000), 4), oCursor += 4);
    }
}

int main() {
    static std::uint32_t t;
    static UnsignedInteger a, b;
    static bool sa, sb;
    while (*iCursor >= '0' && *iCursor <= '9') t = t * 10 + (*iCursor++ - '0');
    ++iCursor;
    do {
        iCursor += (sa = *iCursor == '-'), readFromCursor(a);
        iCursor += (sb = *iCursor == '-'), readFromCursor(b);
        a *= b;
        sa != sb && bool(a) && bool(b) && (*oCursor++ = '-');
        writeUnsigned(a);
        *oCursor++ = '\n';
    } while (--t);
    std::fwrite(oBuffer, sizeof(char), oCursor - oBuffer, stdout);
    return 0;
}
