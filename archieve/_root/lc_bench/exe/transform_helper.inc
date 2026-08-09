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

        // OPT Phase1 radix-4: ×i 旋转, z*i = (-im, re)
        static inline __m128d rotate90(__m128d z) {
            return _mm_xor_pd(_mm_permute_pd(z, 0x1), _mm_set_pd(0.0, -0.0));
        }
        static inline __m256d rotate90x2(__m256d z) {
            return _mm256_xor_pd(_mm256_permute_pd(z, 0x5), _mm256_set_pd(0.0, -0.0, 0.0, -0.0));
        }

        void resize(std::uint32_t transformLength) {
            if (transformLength > length << 1) {
                std::uint32_t halfLog = detail::log2(transformLength) >> 1, halfSize = 1 << halfLog;
                __m128d* baseFactors = new __m128d[halfSize << 1]();
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
        void decimationInFrequencyRadix2(__m128d* dataArray, std::uint32_t transformSize) {
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

        void decimationInTimeRadix2(__m128d* dataArray, std::uint32_t transformSize) {
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

        // ===================== OPT Phase1: 融合 radix-4 =====================
        // 把本代码自己的两级 radix-2 蝶形做代数融合，而非套用教科书 radix-4。
        // 位反转旋转因子表满足 tw[b]=w^2, tw[2b]=w, tw[2b+1]=i*w  (w = tw[2b])，故
        //   DIF: X=A, Y=B*w, Z=C*w^2, T=D*w^3
        //        out[k]=(X+Z)+(Y+T)      out[k+q]=(X+Z)-(Y+T)
        //        out[k+2q]=(X-Z)+i(Y-T)  out[k+3q]=(X-Z)-i(Y-T)
        // 每 4 点 3 次复乘（radix-2 两级需 4 次），内存扫描次数减半，
        // 且输出顺序与 radix-2 逐元素一致 => pointwise / 位反转配对完全复用。
        void decimationInFrequency(__m128d* dataArray, std::uint32_t transformSize) {
            const std::uint32_t stageCount = detail::log2(transformSize);
            for (std::uint32_t q = transformSize >> 2; q; q >>= 2) {
                const std::uint32_t span = q << 2, groupCount = transformSize / span;
                for (std::uint32_t b = 0; b != groupCount; ++b) {
                    // w^2 == tw[b] 恒等式：直接查表，省一次复乘且比连乘更精确
                    const __m128d w = twiddleFactors[b << 1], w2 = twiddleFactors[b];
                    const __m128d w3 = complexMultiply(w2, w);
                    __m128d* g = dataArray + std::uintptr_t(b) * span;
                    const __m256d w1v = _mm256_set_m128d(w, w), w2v = _mm256_set_m128d(w2, w2), w3v = _mm256_set_m128d(w3, w3);
                    const __m256d w1s = _mm256_permute_pd(w1v, 0x5), w2s = _mm256_permute_pd(w2v, 0x5), w3s = _mm256_permute_pd(w3v, 0x5);
                    std::uint32_t k = 0;
                    for (; k + 2 <= q; k += 2) {
                        const __m256d a0 = _mm256_loadu_pd(reinterpret_cast<const double*>(g + k));
                        const __m256d a1 = _mm256_loadu_pd(reinterpret_cast<const double*>(g + k + q));
                        const __m256d a2 = _mm256_loadu_pd(reinterpret_cast<const double*>(g + k + 2 * q));
                        const __m256d a3 = _mm256_loadu_pd(reinterpret_cast<const double*>(g + k + 3 * q));
                        const __m256d Y = _mm256_fmaddsub_pd(_mm256_unpacklo_pd(a1, a1), w1v, _mm256_mul_pd(_mm256_unpackhi_pd(a1, a1), w1s));
                        const __m256d Z = _mm256_fmaddsub_pd(_mm256_unpacklo_pd(a2, a2), w2v, _mm256_mul_pd(_mm256_unpackhi_pd(a2, a2), w2s));
                        const __m256d T = _mm256_fmaddsub_pd(_mm256_unpacklo_pd(a3, a3), w3v, _mm256_mul_pd(_mm256_unpackhi_pd(a3, a3), w3s));
                        const __m256d e0 = _mm256_add_pd(a0, Z), e1 = _mm256_sub_pd(a0, Z);
                        const __m256d o0 = _mm256_add_pd(Y, T), io1 = rotate90x2(_mm256_sub_pd(Y, T));
                        _mm256_storeu_pd(reinterpret_cast<double*>(g + k), _mm256_add_pd(e0, o0));
                        _mm256_storeu_pd(reinterpret_cast<double*>(g + k + q), _mm256_sub_pd(e0, o0));
                        _mm256_storeu_pd(reinterpret_cast<double*>(g + k + 2 * q), _mm256_add_pd(e1, io1));
                        _mm256_storeu_pd(reinterpret_cast<double*>(g + k + 3 * q), _mm256_sub_pd(e1, io1));
                    }
                    for (; k != q; ++k) {
                        const __m128d X = g[k], Y = complexMultiply(g[k + q], w);
                        const __m128d Z = complexMultiply(g[k + 2 * q], w2), T = complexMultiply(g[k + 3 * q], w3);
                        const __m128d e0 = _mm_add_pd(X, Z), e1 = _mm_sub_pd(X, Z);
                        const __m128d o0 = _mm_add_pd(Y, T), io1 = rotate90(_mm_sub_pd(Y, T));
                        g[k] = _mm_add_pd(e0, o0), g[k + q] = _mm_sub_pd(e0, o0);
                        g[k + 2 * q] = _mm_add_pd(e1, io1), g[k + 3 * q] = _mm_sub_pd(e1, io1);
                    }
                }
                if (q == 1) break;
            }
            if (stageCount & 1) {  // 奇数级：剩下的 blockSize==1 那一级仍走 radix-2
                const __m128d e = dataArray[0], o = dataArray[1];
                dataArray[0] = _mm_add_pd(e, o), dataArray[1] = _mm_sub_pd(e, o);
                const __m128d* wp = twiddleFactors + 1;
                for (std::uint32_t base = 2; base != transformSize; base += 2, ++wp) {
                    const __m128d ee = dataArray[base], oo = complexMultiply(dataArray[base + 1], *wp);
                    dataArray[base] = _mm_add_pd(ee, oo), dataArray[base + 1] = _mm_sub_pd(ee, oo);
                }
            }
        }

        // 逆变换：DIF 的严格逆，共轭旋转因子
        //   s0=A+B, s1=A-B, s2=C+D, i3=i*(C-D)
        //   out[k]=s0+s2  out[k+q]=(s1-i3)*conj(w)
        //   out[k+2q]=(s0-s2)*conj(w^2)  out[k+3q]=(s1+i3)*conj(w^3)
        void decimationInTime(__m128d* dataArray, std::uint32_t transformSize) {
            const std::uint32_t stageCount = detail::log2(transformSize);
            if (stageCount & 1) {
                const __m128d e = dataArray[0], o = dataArray[1];
                dataArray[0] = _mm_add_pd(e, o), dataArray[1] = _mm_sub_pd(e, o);
                const __m128d* wp = twiddleFactors + 1;
                for (std::uint32_t base = 2; base != transformSize; base += 2, ++wp) {
                    const __m128d ee = dataArray[base], oo = dataArray[base + 1];
                    dataArray[base] = _mm_add_pd(ee, oo);
                    dataArray[base + 1] = complexMultiplyConjugate(_mm_sub_pd(ee, oo), *wp);
                }
            }
            for (std::uint32_t q = (stageCount & 1) ? 2u : 1u; q <= (transformSize >> 2); q <<= 2) {
                const std::uint32_t span = q << 2, groupCount = transformSize / span;
                for (std::uint32_t b = 0; b != groupCount; ++b) {
                    // w^2 == tw[b] 恒等式：直接查表，省一次复乘且比连乘更精确
                    const __m128d w = twiddleFactors[b << 1], w2 = twiddleFactors[b];
                    const __m128d w3 = complexMultiply(w2, w);
                    __m128d* g = dataArray + std::uintptr_t(b) * span;
                    const __m256d w1v = _mm256_set_m128d(w, w), w2v = _mm256_set_m128d(w2, w2), w3v = _mm256_set_m128d(w3, w3);
                    const __m256d w1l = _mm256_unpacklo_pd(w1v, w1v), w1h = _mm256_unpackhi_pd(w1v, w1v);
                    const __m256d w2l = _mm256_unpacklo_pd(w2v, w2v), w2h = _mm256_unpackhi_pd(w2v, w2v);
                    const __m256d w3l = _mm256_unpacklo_pd(w3v, w3v), w3h = _mm256_unpackhi_pd(w3v, w3v);
                    std::uint32_t k = 0;
                    for (; k + 2 <= q; k += 2) {
                        const __m256d A = _mm256_loadu_pd(reinterpret_cast<const double*>(g + k));
                        const __m256d B = _mm256_loadu_pd(reinterpret_cast<const double*>(g + k + q));
                        const __m256d C = _mm256_loadu_pd(reinterpret_cast<const double*>(g + k + 2 * q));
                        const __m256d D = _mm256_loadu_pd(reinterpret_cast<const double*>(g + k + 3 * q));
                        const __m256d s0 = _mm256_add_pd(A, B), s1 = _mm256_sub_pd(A, B);
                        const __m256d s2 = _mm256_add_pd(C, D), i3 = rotate90x2(_mm256_sub_pd(C, D));
                        const __m256d d1 = _mm256_sub_pd(s1, i3), d2 = _mm256_sub_pd(s0, s2), d3 = _mm256_add_pd(s1, i3);
                        _mm256_storeu_pd(reinterpret_cast<double*>(g + k), _mm256_add_pd(s0, s2));
                        _mm256_storeu_pd(reinterpret_cast<double*>(g + k + q),
                            _mm256_fmsubadd_pd(w1l, d1, _mm256_mul_pd(w1h, _mm256_permute_pd(d1, 0x5))));
                        _mm256_storeu_pd(reinterpret_cast<double*>(g + k + 2 * q),
                            _mm256_fmsubadd_pd(w2l, d2, _mm256_mul_pd(w2h, _mm256_permute_pd(d2, 0x5))));
                        _mm256_storeu_pd(reinterpret_cast<double*>(g + k + 3 * q),
                            _mm256_fmsubadd_pd(w3l, d3, _mm256_mul_pd(w3h, _mm256_permute_pd(d3, 0x5))));
                    }
                    for (; k != q; ++k) {
                        const __m128d A = g[k], B = g[k + q], C = g[k + 2 * q], D = g[k + 3 * q];
                        const __m128d s0 = _mm_add_pd(A, B), s1 = _mm_sub_pd(A, B);
                        const __m128d s2 = _mm_add_pd(C, D), i3 = rotate90(_mm_sub_pd(C, D));
                        g[k] = _mm_add_pd(s0, s2);
                        g[k + q] = complexMultiplyConjugate(_mm_sub_pd(s1, i3), w);
                        g[k + 2 * q] = complexMultiplyConjugate(_mm_sub_pd(s0, s2), w2);
                        g[k + 3 * q] = complexMultiplyConjugate(_mm_add_pd(s1, i3), w3);
                    }
                }
            }
        }
        // =================== end OPT Phase1: 融合 radix-4 ===================

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
using detail_TransformHelper = TransformHelper;
