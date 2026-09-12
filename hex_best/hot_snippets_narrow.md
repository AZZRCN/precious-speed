# HEX div — 热点片段 + 指令集（窄口径）

> 用途：供给在线 LLM 局部探究 `instructions:u` 的理论极限。
> 口径：只给热点代码 + ISA + 当前 cache miss 实测。**不解释片段上下文、不做算法级改动、仅局部优化。**

## 0. 平台 / 指令集 / 度量（事实）

- 目标 CPU：AMD EPYC 7B13 = Zen3 (Milan)。编译 `-march=znver3 -mtune=znver3 -O3 -std=c++20`。
- ISA：x86-64，**AVX2（256-bit）**，**FMA3**。**无 AVX-512**（7B13 无 512 单元）。
- 吞吐（Zen3 每核）：FMA 2/cyc（=4 DP·FMA/cyc）；load 2×256b/cyc；store ~1×256b/cyc；端口 4 ALU + 3 AGU + 2 FPU。
- Cache：L1D 32KB/核（4cyc）；L2 512KB/核；L3 32MB/CCD（8 核共享）。dTLB 64 项，L2 TLB 2048 项。
- **度量（唯一真值）**：`perf stat -e instructions:u`。墙钟 / cycles / IPC 不作收口判据（x86-64 指令数 Intel/AMD 一致，VM 实测即 7B13 真值）。
- 基线：P0 = **449.0M 指令**（300-case 基准，-2.74% vs 461.6M）；单 `one_case`（REPS=1, 400k/206k limbs）= **426.36M 指令**。
- 守口：9gen×Nseed oracle 全过 + `instructions:u` 不高于基线。

## 1. 当前 CPU cache miss 状况（perf, REPS=10, 单 one_case）

| 事件 | 计数 |
|---|---|
| instructions:u | 3,591,801,357 |
| L1-dcache-loads | 667,459,984 |
| **L1-dcache-load-misses** | **147,151,289 (miss rate 22.05%)** |
| cache-references (perf 通用映射) | 80,013,612 |
| cache-misses (perf 通用映射) | 29,027,571 |
| dTLB-load-misses | 205,794 (可忽略) |
| branch-misses | 1,252,938 |

cache-miss 按函数分布（perf record -e cache-misses, self%）：

| 函数 | cache-miss self% |
|---|---|
| copyU16ToF64AndFill | 18.35% |
| difC4 | 14.89% |
| carryPropSeg | 10.24% |
| idit3StageC4 | 9.71% |
| dif3StageC4 | 8.67% |
| real_dot_binrev3 | 8.05% |
| iditC4 | 7.42% |
| libc memset/memmove | ~6.5% |
| iditDispatch | 3.07% |
| iditSmall | 2.57% |
| absSub | 1.90% |
| real_dot_binrev2 | 1.78% |

解读（仅数据，非片段上下文）：L1 miss 率 22%，miss 分散在「输入流转换 / FFT 递归跨步访问（s1,s2,s3 偏移）/ 进位 4 段并行 / rdot 重建」各处，**并非集中于 carryPropSeg**。FFT 的递归 strided 访问（块间 stride = float_len/4）越过 L1；进位 4 段并行（b1,b2,b3 偏移）同样跨步。
**铁律注**：cache-miss 只降 cycles、不降 `instructions:u`；本指标下纯 cache 优化零收益。此处提供仅作 LLM 全局意识——若某局部改动恰巧同时降指令，仍计入。

## 2. 热点片段（self% 来自 perf instructions:u；仅贴代码）

### 2.1 difC4 — 26.83%  （前向 FFT 主循环, AVX2 C4 蝶形）
```cpp
                void difC4(Float inout[], size_t float_len)   // 实际为 template <int IN_MODE>
                {
                    if constexpr (std::is_same_v<Float, double>)
                    {
                        if (float_len <= FFT_C4_MIN)
                        {
                            packC4(inout, float_len);
                            dif<false>(inout, float_len);
                            return;
                        }
                        const size_t fft_len = float_len / 2;
                        const size_t s1 = float_len / 4, s2 = s1 * 2, s3 = s1 * 3;
                        auto tp1 = reinterpret_cast<const double *>(
                            __builtin_assume_aligned(table1.getBegin(fft_len), 32));
                        auto tp3 = reinterpret_cast<const double *>(
                            __builtin_assume_aligned(table3.getBegin(fft_len), 32));
                        auto it = reinterpret_cast<double *>(__builtin_assume_aligned(inout, 32));
                        size_t k = fft_len / 16;
                        for (; k >= 2; k -= 2, it += 16, tp1 += 16, tp3 += 16)
                        {
                            C4d c0 = c4loadM<IN_MODE>(it), c1 = c4loadM<IN_MODE>(it + s1);
                            C4d c2 = c4loadM<IN_MODE>(it + s2), c3 = c4loadM<IN_MODE>(it + s3);
                            difSplitC4(c0, c1, c2, c3);
                            c4store(it, c0);
                            c4store(it + s1, c1);
                            c4store(it + s2, c4mul(c2, c4load(tp1)));
                            c4store(it + s3, c4mul(c3, c4load(tp3)));
                            C4d d0 = c4loadM<IN_MODE>(it + 8), d1 = c4loadM<IN_MODE>(it + s1 + 8);
                            C4d d2 = c4loadM<IN_MODE>(it + s2 + 8), d3 = c4loadM<IN_MODE>(it + s3 + 8);
                            difSplitC4(d0, d1, d2, d3);
                            c4store(it + 8, d0);
                            c4store(it + s1 + 8, d1);
                            c4store(it + s2 + 8, c4mul(d2, c4load(tp1 + 8)));
                            c4store(it + s3 + 8, c4mul(d3, c4load(tp3 + 8)));
                        }
                        if (k)
                        {
                            C4d c0 = c4loadM<IN_MODE>(it), c1 = c4loadM<IN_MODE>(it + s1);
                            C4d c2 = c4loadM<IN_MODE>(it + s2), c3 = c4loadM<IN_MODE>(it + s3);
                            difSplitC4(c0, c1, c2, c3);
                            c4store(it, c0);
                            c4store(it + s1, c1);
                            c4store(it + s2, c4mul(c2, c4load(tp1)));
                            c4store(it + s3, c4mul(c3, c4load(tp3)));
                        }
                        const size_t stride = float_len / 4;
                        difC4<0>(inout, stride * 2);
                        difC4<0>(inout + stride * 2, stride);
                        difC4<0>(inout + stride * 3, stride);
                    }
                }
```

### 2.2 iditC4 — 15.23%  （逆变换主循环, AVX2 C4 蝶形）
```cpp
                void iditC4(Float inout[], size_t float_len)   // 实际为 template <int OUT_MODE>
                {
                    if constexpr (std::is_same_v<Float, double>)
                    {
                        if (float_len <= FFT_C4_MIN)
                        {
                            idit<false>(inout, float_len);
                            packC4(inout, float_len);
                            return;
                        }
                        const size_t stride = float_len / 4;
                        iditC4<0>(inout, stride * 2);
                        iditC4<0>(inout + stride * 2, stride);
                        iditC4<0>(inout + stride * 3, stride);
                        const size_t fft_len = float_len / 2;
                        const size_t s1 = float_len / 4, s2 = s1 * 2, s3 = s1 * 3;
                        auto tp1 = reinterpret_cast<const double *>(
                            __builtin_assume_aligned(table1.getBegin(fft_len), 32));
                        auto tp3 = reinterpret_cast<const double *>(
                            __builtin_assume_aligned(table3.getBegin(fft_len), 32));
                        auto it = reinterpret_cast<double *>(__builtin_assume_aligned(inout, 32));
                        size_t k = fft_len / 16;
                        for (; k >= 2; k -= 2, it += 16, tp1 += 16, tp3 += 16)
                        {
                            C4d c0 = c4load(it), c1 = c4load(it + s1);
                            C4d c2 = c4mulConj(c4load(it + s2), c4load(tp1));
                            C4d c3 = c4mulConj(c4load(it + s3), c4load(tp3));
                            iditSplitC4(c0, c1, c2, c3);
                            c4storeM<OUT_MODE>(it, c0);
                            c4storeM<OUT_MODE>(it + s1, c1);
                            c4storeM<OUT_MODE>(it + s2, c2);
                            c4storeM<OUT_MODE>(it + s3, c3);
                            C4d d0 = c4load(it + 8), d1 = c4load(it + s1 + 8);
                            C4d d2 = c4mulConj(c4load(it + s2 + 8), c4load(tp1 + 8));
                            C4d d3 = c4mulConj(c4load(it + s3 + 8), c4load(tp3 + 8));
                            iditSplitC4(d0, d1, d2, d3);
                            c4storeM<OUT_MODE>(it + 8, d0);
                            c4storeM<OUT_MODE>(it + s1 + 8, d1);
                            c4storeM<OUT_MODE>(it + s2 + 8, d2);
                            c4storeM<OUT_MODE>(it + s3 + 8, d3);
                        }
                        if (k)
                        {
                            C4d c0 = c4load(it), c1 = c4load(it + s1);
                            C4d c2 = c4mulConj(c4load(it + s2), c4load(tp1));
                            C4d c3 = c4mulConj(c4load(it + s3), c4load(tp3));
                            iditSplitC4(c0, c1, c2, c3);
                            c4storeM<OUT_MODE>(it, c0);
                            c4storeM<OUT_MODE>(it + s1, c1);
                            c4storeM<OUT_MODE>(it + s2, c2);
                            c4storeM<OUT_MODE>(it + s3, c3);
                        }
                    }
                }
```

### 2.3 difSmall / iditSmall — 5.79% / 1.98%  （小长度标量蝶形）
```cpp
                template <bool RIRI_IN>
                HINT_AI_SMALL void difSmall(Float inout[], size_t float_len)
                {
                    if (float_len <= 2) return;
                    auto itc = reinterpret_cast<C2 *>(inout);
                    auto itf = reinterpret_cast<F2 *>(inout);
                    if (float_len == 4)
                    {
                        if (RIRI_IN) std::swap(inout[1], inout[2]);
                        transform2(inout[0], inout[1]);
                        transform2(inout[2], inout[3]);
                    }
                    else
                    {
                        if (RIRI_IN) { std::swap(inout[1], inout[2]); std::swap(inout[5], inout[6]); }
                        Float r0 = inout[0], r1 = inout[1], i0 = inout[2], i1 = inout[3];
                        Float r2 = inout[4], r3 = inout[5], i2 = inout[6], i3 = inout[7];
                        difSplit(r0, i0, r1, i1, r2, i2, r3, i3);
                        transform2(r0, r1); transform2(i0, i1);
                        inout[0] = r0, inout[1] = r1, inout[2] = i0, inout[3] = i1;
                        inout[4] = r2, inout[5] = r3, inout[6] = i2, inout[7] = i3;
                    }
                }
                template <bool RIRI_OUT>
                HINT_AI_SMALL void iditSmall(Float inout[], size_t float_len)
                {
                    if (float_len <= 2) return;
                    auto itc = reinterpret_cast<C2 *>(inout);
                    if (float_len == 4)
                    {
                        transform2(inout[0], inout[1]);
                        transform2(inout[2], inout[3]);
                        if (RIRI_OUT) std::swap(inout[1], inout[2]);
                    }
                    else
                    {
                        Float r0 = inout[0], r1 = inout[1], i0 = inout[2], i1 = inout[3];
                        Float r2 = inout[4], r3 = inout[5], i2 = inout[6], i3 = inout[7];
                        transform2(r0, r1); transform2(i0, i1);
                        iditSplit(r0, i0, r1, i1, r2, i2, r3, i3);
                        inout[0] = r0, inout[1] = r1, inout[2] = i0, inout[3] = i1;
                        inout[4] = r2, inout[5] = r3, inout[6] = i2, inout[7] = i3;
                        if (RIRI_OUT) { std::swap(inout[1], inout[2]); std::swap(inout[5], inout[6]); }
                    }
                }
```

### 2.4 difDispatch / iditDispatch — 12.53% / 5.38%  （编译期展开 codelet 分发）
```cpp
                template <bool RIRI_IN>
                HINT_AI_DISPATCH void difDispatch(Float inout[], size_t float_len)
                {
                    switch (float_len)
                    {
#if FFT_FIXED_MAX >= 128
                    case 128: difFixed<128, RIRI_IN>(inout); return;
#endif
#if FFT_FIXED_MAX >= 64
                    case 64: difFixed<64, RIRI_IN>(inout); return;
#endif
#if FFT_FIXED_MAX >= 32
                    case 32: difFixed<32, RIRI_IN>(inout); return;
#endif
#if FFT_FIXED_MAX >= 16
                    case 16: difFixed<16, RIRI_IN>(inout); return;
#endif
                    default: difSmall<RIRI_IN>(inout, float_len); return;
                    }
                }
                template <bool RIRI_OUT>
                HINT_AI_DISPATCH void iditDispatch(Float inout[], size_t float_len)
                {
                    switch (float_len)
                    {
#if FFT_FIXED_MAX >= 128
                    case 128: iditFixed<128, RIRI_OUT>(inout); return;
#endif
#if FFT_FIXED_MAX >= 64
                    case 64: iditFixed<64, RIRI_OUT>(inout); return;
#endif
#if FFT_FIXED_MAX >= 32
                    case 32: iditFixed<32, RIRI_OUT>(inout); return;
#endif
#if FFT_FIXED_MAX >= 16
                    case 16: iditFixed<16, RIRI_OUT>(inout); return;
#endif
                    default: iditSmall<RIRI_OUT>(inout, float_len); return;
                    }
                }
```

### 2.5 real_dot_binrev2 — 0.53%  （rdot 重建, 2-power 路径）
```cpp
            inline void real_dot_binrev2(Float in_out[], const Float in[], size_t float_len)
            {
                using F2 = Float2<Float>;
                Float inv = 1.0 / float_len;
                real_dot_binrev<2>(in_out, in, 16, inv);
                inv = 0.25 / float_len;
                const F2 invx = F2::from1(inv);
                static thread_local BinRevTableC2HP<Float> table(31, 32);
                for (size_t begin = 16; begin < float_len; begin *= 2)
                {
                    table.reset(begin / 2);
                    if constexpr (std::is_same_v<Float, double>)
                    {
                        if (begin >= 32)
                        {
                            const __m256d invv = _mm256_set1_pd(inv);
                            double *q0 = in_out + begin, *q1 = q0 + begin - 8;
                            const double *q2 = in + begin, *q3 = q2 + begin - 8;
                            for (size_t u = begin / 16; u > 0; u--, q0 += 8, q1 -= 8, q2 += 8, q3 -= 8)
                            {
                                const __m256d w0 = table.iterateV();
                                const __m256d w1 = table.iterateV();
                                dot_rfftX4(q0, q1, q2, q3, c4twiddle(w0, w1), invv);
                            }
                            continue;
                        }
                    }
                    auto it0 = in_out + begin, it1 = it0 + begin - 4;
                    auto it2 = in + begin, it3 = it2 + begin - 4;
                    for (; it0 < it1; it0 += 4, it1 -= 4, it2 += 4, it3 -= 4)
                        dot_rfftX2(it0, it1, it2, it3, table.iterate(), invx);
                }
            }
```

### 2.6 dif3StageC4 — 3.85%  （前向 radix-3 顶层, AVX2）
```cpp
            template <bool RIRI_IN>
            inline void dif3StageC4(double *inout, size_t m)
            {
                const __m256d vh = _mm256_set1_pd(0.5);
                const __m256d vs = _mm256_set1_pd(double(SQRT3_DIV2));
                const __m256d vns = _mm256_set1_pd(-double(SQRT3_DIV2));
                auto &t1 = getTable3a<double>();
                auto &t2 = getTable3b<double>();
                t1.expand(m), t2.expand(m);
                auto p1 = reinterpret_cast<const double *>(__builtin_assume_aligned(t1.getBegin(m), 32));
                auto p2 = reinterpret_cast<const double *>(__builtin_assume_aligned(t2.getBegin(m), 32));
                double *b0 = reinterpret_cast<double *>(__builtin_assume_aligned(inout, 32));
                double *b1 = b0 + m * 2, *b2 = b0 + m * 4;
                size_t c = m / 4;
                for (; c >= 2; c -= 2, b0 += 16, b1 += 16, b2 += 16, p1 += 16, p2 += 16)
                {
                    const C4d a0 = c4loadM<RIRI_IN ? 2 : 1>(b0);
                    const C4d a1 = c4loadM<RIRI_IN ? 2 : 1>(b1);
                    const C4d a2 = c4loadM<RIRI_IN ? 2 : 1>(b2);
                    const __m256d sr = a1.re + a2.re, si = a1.im + a2.im;
                    const __m256d dr = a1.re - a2.re, di = a1.im - a2.im;
                    const __m256d hr = a0.re - sr * vh, hi = a0.im - si * vh;
                    const __m256d gr = di * vns, gi = dr * vs;
                    c4store(b0, C4d{a0.re + sr, a0.im + si});
                    c4store(b1, c4mul(C4d{hr - gr, hi - gi}, c4loadC2(p1)));
                    c4store(b2, c4mul(C4d{hr + gr, hi + gi}, c4loadC2(p2)));
                    const C4d e0 = c4loadM<RIRI_IN ? 2 : 1>(b0 + 8);
                    const C4d e1 = c4loadM<RIRI_IN ? 2 : 1>(b1 + 8);
                    const C4d e2 = c4loadM<RIRI_IN ? 2 : 1>(b2 + 8);
                    const __m256d sr2 = e1.re + e2.re, si2 = e1.im + e2.im;
                    const __m256d dr2 = e1.re - e2.re, di2 = e1.im - e2.im;
                    const __m256d hr2 = e0.re - sr2 * vh, hi2 = e0.im - si2 * vh;
                    const __m256d gr2 = di2 * vns, gi2 = dr2 * vs;
                    c4store(b0 + 8, C4d{e0.re + sr2, e0.im + si2});
                    c4store(b1 + 8, c4mul(C4d{hr2 - gr2, hi2 - gi2}, c4loadC2(p1 + 8)));
                    c4store(b2 + 8, c4mul(C4d{hr2 + gr2, hi2 + gi2}, c4loadC2(p2 + 8)));
                }
                if (c)
                {
                    const C4d a0 = c4loadM<RIRI_IN ? 2 : 1>(b0);
                    const C4d a1 = c4loadM<RIRI_IN ? 2 : 1>(b1);
                    const C4d a2 = c4loadM<RIRI_IN ? 2 : 1>(b2);
                    const __m256d sr = a1.re + a2.re, si = a1.im + a2.im;
                    const __m256d dr = a1.re - a2.re, di = a1.im - a2.im;
                    const __m256d hr = a0.re - sr * vh, hi = a0.im - si * vh;
                    const __m256d gr = di * vns, gi = dr * vs;
                    c4store(b0, C4d{a0.re + sr, a0.im + si});
                    c4store(b1, c4mul(C4d{hr - gr, hi - gi}, c4loadC2(p1)));
                    c4store(b2, c4mul(C4d{hr + gr, hi + gi}, c4loadC2(p2)));
                }
            }
```

### 2.7 idit3StageC4 — 1.98%  （逆 radix-3 顶层, AVX2）
```cpp
            template <bool RIRI_OUT>
            inline void idit3StageC4(double *inout, size_t m)
            {
                const __m256d vh = _mm256_set1_pd(0.5);
                const __m256d vs = _mm256_set1_pd(double(SQRT3_DIV2));
                const __m256d vns = _mm256_set1_pd(-double(SQRT3_DIV2));
                auto &t1 = getTable3a<double>();
                auto &t2 = getTable3b<double>();
                t1.expand(m), t2.expand(m);
                auto p1 = reinterpret_cast<const double *>(__builtin_assume_aligned(t1.getBegin(m), 32));
                auto p2 = reinterpret_cast<const double *>(__builtin_assume_aligned(t2.getBegin(m), 32));
                double *b0 = reinterpret_cast<double *>(__builtin_assume_aligned(inout, 32));
                double *b1 = b0 + m * 2, *b2 = b0 + m * 4;
                size_t c = m / 4;
                for (; c >= 2; c -= 2, b0 += 16, b1 += 16, b2 += 16, p1 += 16, p2 += 16)
                {
                    const C4d t0 = c4load(b0);
                    const C4d x1 = c4mulConj(c4load(b1), c4loadC2(p1));
                    const C4d x2 = c4mulConj(c4load(b2), c4loadC2(p2));
                    const __m256d sr = x1.re + x2.re, si = x1.im + x2.im;
                    const __m256d dr = x1.re - x2.re, di = x1.im - x2.im;
                    const __m256d hr = t0.re - sr * vh, hi = t0.im - si * vh;
                    const __m256d gr = di * vns, gi = dr * vs;
                    c4storeM<RIRI_OUT ? 2 : 1>(b0, C4d{t0.re + sr, t0.im + si});
                    c4storeM<RIRI_OUT ? 2 : 1>(b1, C4d{hr + gr, hi + gi});
                    c4storeM<RIRI_OUT ? 2 : 1>(b2, C4d{hr - gr, hi - gi});
                    const C4d u0 = c4load(b0 + 8);
                    const C4d y1 = c4mulConj(c4load(b1 + 8), c4loadC2(p1 + 8));
                    const C4d y2 = c4mulConj(c4load(b2 + 8), c4loadC2(p2 + 8));
                    const __m256d sr2 = y1.re + y2.re, si2 = y1.im + y2.im;
                    const __m256d dr2 = y1.re - y2.re, di2 = y1.im - y2.im;
                    const __m256d hr2 = u0.re - sr2 * vh, hi2 = u0.im - si2 * vh;
                    const __m256d gr2 = di2 * vns, gi2 = dr2 * vs;
                    c4storeM<RIRI_OUT ? 2 : 1>(b0 + 8, C4d{u0.re + sr2, u0.im + si2});
                    c4storeM<RIRI_OUT ? 2 : 1>(b1 + 8, C4d{hr2 + gr2, hi2 + gi2});
                    c4storeM<RIRI_OUT ? 2 : 1>(b2 + 8, C4d{hr2 - gr2, hi2 - gi2});
                }
                if (c)
                {
                    const C4d t0 = c4load(b0);
                    const C4d x1 = c4mulConj(c4load(b1), c4loadC2(p1));
                    const C4d x2 = c4mulConj(c4load(b2), c4loadC2(p2));
                    const __m256d sr = x1.re + x2.re, si = x1.im + x2.im;
                    const __m256d dr = x1.re - x2.re, di = x1.im - x2.im;
                    const __m256d hr = t0.re - sr * vh, hi = t0.im - si * vh;
                    const __m256d gr = di * vns, gi = dr * vs;
                    c4storeM<RIRI_OUT ? 2 : 1>(b0, C4d{t0.re + sr, t0.im + si});
                    c4storeM<RIRI_OUT ? 2 : 1>(b1, C4d{hr + gr, hi + gi});
                    c4storeM<RIRI_OUT ? 2 : 1>(b2, C4d{hr - gr, hi - gi});
                }
            }
```

### 2.8 real_dot_binrev3 — 6.77%  （rdot 重建, fft3 路径, 块0 + 块1↔块2 交叉）
```cpp
            inline void real_dot_binrev3(Float in_out[], const Float in[], size_t float_len)
            {
                using F2 = Float2<Float>;
                using C2 = Complex2<Float>;
                const size_t m = float_len / 6, blk = m * 2;
                const Float inv = Float(1) / Float(float_len);
                const F2 invx = F2::from1(Float(0.25) / Float(float_len));
                static thread_local BinRevTableC2HP<Float> table(31, 32);
                real_dot_binrev<2>(in_out, in, 16, inv);
                for (size_t begin = 16; begin < blk; begin *= 2)
                {
                    table.reset(begin / 2);
                    if constexpr (std::is_same_v<Float, double>)
                    {
                        if (begin >= 32)
                        {
                            const __m256d invv = _mm256_set1_pd(Float(0.25) / Float(float_len));
                            double *q0 = in_out + begin, *q1 = q0 + begin - 8;
                            const double *q2 = in + begin, *q3 = q2 + begin - 8;
                            for (size_t u = begin / 16; u > 0; u--, q0 += 8, q1 -= 8, q2 += 8, q3 -= 8)
                            {
                                const __m256d w0 = table.iterateV();
                                const __m256d w1 = table.iterateV();
                                dot_rfftX4(q0, q1, q2, q3, c4twiddle(w0, w1), invv);
                            }
                            continue;
                        }
                    }
                    auto it0 = in_out + begin, it1 = it0 + begin - 4;
                    auto it2 = in + begin, it3 = it2 + begin - 4;
                    for (; it0 < it1; it0 += 4, it1 -= 4, it2 += 4, it3 -= 4)
                        dot_rfftX2(it0, it1, it2, it3, table.iterate(), invx);
                }
                Float *o1 = in_out + blk, *o2 = in_out + blk * 2;
                const Float *n1 = in + blk, *n2 = in + blk * 2;
                const Float ang = Float(-HINT_2PI) / Float(float_len);
                const C2 rot(Float(std::cos(ang)), Float(std::sin(ang)));
                {
                    const C2 *sm = smallOmega8<Float>();
                    for (size_t t = 0; t < 4; t++)
                    {
                        const size_t f = 4 * t;
                        dot_rfftX2(o1 + f, o2 + blk - 4 - f, n1 + f, n2 + blk - 4 - f,
                                   sm[t].mul(rot), invx);
                    }
                }
                for (size_t begin = 16; begin < blk; begin *= 2)
                {
                    table.reset(begin / 2);
                    if constexpr (std::is_same_v<Float, double>)
                    {
                        const __m256d invv = _mm256_set1_pd(Float(0.25) / Float(float_len));
                        const C4d rotv{_mm256_set1_pd(rot.real.x0), _mm256_set1_pd(rot.imag.x0)};
                        double *p0 = o1 + begin, *p1 = o2 + blk - 8 - begin;
                        const double *r0 = n1 + begin, *r1 = n2 + blk - 8 - begin;
                        for (size_t u = begin / 8; u > 0; u--, p0 += 8, p1 -= 8, r0 += 8, r1 -= 8)
                        {
                            const __m256d w0 = table.iterateV();
                            const __m256d w1 = table.iterateV();
                            dot_rfftX4(p0, p1, r0, r1, c4mul(c4twiddle(w0, w1), rotv), invv);
                        }
                        continue;
                    }
                    auto i0 = o1 + begin, i1 = o2 + blk - 4 - begin;
                    auto j0 = n1 + begin, j1 = n2 + blk - 4 - begin;
                    for (size_t u = begin / 4; u > 0; u--, i0 += 4, i1 -= 4, j0 += 4, j1 -= 4)
                        dot_rfftX2(i0, i1, j0, j1, table.iterate().mul(rot), invx);
                }
            }
```

### 2.9 copyU16ToF64AndFill — 1.85%  （输入 limb→double 转换 + 填零, AVX2）
```cpp
    inline void copyU16ToF64AndFill(const uint32_t *src, double *dst, size_t n, size_t total)
    {
        size_t j = 0;
#if defined(__AVX2__)
        for (; j + 8 <= n; j += 8)
        {
            __m256i v = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(src + j));
            _mm256_storeu_pd(dst + j,     _mm256_cvtepi32_pd(_mm256_castsi256_si128(v)));
            _mm256_storeu_pd(dst + j + 4, _mm256_cvtepi32_pd(_mm256_extracti128_si256(v, 1)));
        }
#endif
        for (; j < n; j++) dst[j] = (double)src[j];
        j = n;
#if defined(__AVX2__)
        __m256d zero = _mm256_setzero_pd();
        for (; j + 4 <= total; j += 4) _mm256_storeu_pd(dst + j, zero);
#endif
        for (; j < total; j++) dst[j] = 0.0;
    }
```

### 2.10 absAdd / absSub — 1.15% / 3.25%  （大整数加减, 标量 8 路展开, base-2^16 无 AVX2）
```cpp
        static bool absAdd(View in1, View in2, Span out)
        {
            if (in1.size < in2.size) std::swap(in1, in2);
            size_t i = 0;
            Limb carry = 0;
            for (; i + 7 < in2.size; i += 8)
            {
                out[i]   = add_half<Limb>(in1[i],   in2[i]   + carry, BASE, carry);
                out[i+1] = add_half<Limb>(in1[i+1], in2[i+1] + carry, BASE, carry);
                out[i+2] = add_half<Limb>(in1[i+2], in2[i+2] + carry, BASE, carry);
                out[i+3] = add_half<Limb>(in1[i+3], in2[i+3] + carry, BASE, carry);
                out[i+4] = add_half<Limb>(in1[i+4], in2[i+4] + carry, BASE, carry);
                out[i+5] = add_half<Limb>(in1[i+5], in2[i+5] + carry, BASE, carry);
                out[i+6] = add_half<Limb>(in1[i+6], in2[i+6] + carry, BASE, carry);
                out[i+7] = add_half<Limb>(in1[i+7], in2[i+7] + carry, BASE, carry);
            }
            for (; i < in2.size; i++) out[i] = add_half<Limb>(in1[i], in2[i] + carry, BASE, carry);
            for (; i < in1.size; i++)
            {
                if (carry == 0) break;
                out[i] = add_half<Limb>(in1[i], carry, BASE, carry);
            }
            if (carry == 0 && i < in1.size && out.ptr != in1.ptr)
                std::memcpy(out.ptr + i, in1.ptr + i, (in1.size - i) * sizeof(Limb));
            return carry;
        }
        static bool absSub(View in1, View in2, Span out)
        {
            assert(in1.size >= in2.size);
            size_t i = 0;
            Limb borrow = 0;
            for (; i + 7 < in2.size; i += 8)
            {
                out[i]   = sub_half<Limb>(in1[i],   in2[i]   + borrow, BASE, borrow);
                out[i+1] = sub_half<Limb>(in1[i+1], in2[i+1] + borrow, BASE, borrow);
                out[i+2] = sub_half<Limb>(in1[i+2], in2[i+2] + borrow, BASE, borrow);
                out[i+3] = sub_half<Limb>(in1[i+3], in2[i+3] + borrow, BASE, borrow);
                out[i+4] = sub_half<Limb>(in1[i+4], in2[i+4] + borrow, BASE, borrow);
                out[i+5] = sub_half<Limb>(in1[i+5], in2[i+5] + borrow, BASE, borrow);
                out[i+6] = sub_half<Limb>(in1[i+6], in2[i+6] + borrow, BASE, borrow);
                out[i+7] = sub_half<Limb>(in1[i+7], in2[i+7] + borrow, BASE, borrow);
            }
            for (; i < in2.size; i++) out[i] = sub_half<Limb>(in1[i], in2[i] + borrow, BASE, borrow);
            for (; i < in1.size; i++) out[i] = sub_half<Limb>(in1[i], borrow, BASE, borrow);
            return borrow;
        }
```

### 2.11 carryPropSeg — 9.73%  （进位传播; 4 段并行 + 串行尾部 + 段边界涟漪）
```cpp
        static uint64_t carryPropSeg(const double *v, Limb *out, size_t n)
        {
            constexpr size_t MIN_PAR = 2048;
            uint64_t carry = 0;
            size_t i = 0;
            if (n >= MIN_PAR)
            {
                const size_t seg = (n >> 2) & ~size_t(7);
                const size_t b1 = seg, b2 = seg * 2, b3 = seg * 3;
                uint64_t c0 = 0, c1 = 0, c2 = 0, c3 = 0;
                for (size_t k = 0; k < seg; ++k)
                {
                    uint64_t s0 = c0 + cvtRoundU64(v + k);
                    uint64_t s1 = c1 + cvtRoundU64(v + b1 + k);
                    uint64_t s2 = c2 + cvtRoundU64(v + b2 + k);
                    uint64_t s3 = c3 + cvtRoundU64(v + b3 + k);
                    uint64_t q0 = divBASE(s0);
                    uint64_t q1 = divBASE(s1);
                    uint64_t q2 = divBASE(s2);
                    uint64_t q3 = divBASE(s3);
                    out[k]      = Limb(s0 - q0 * BASE);
                    out[b1 + k] = Limb(s1 - q1 * BASE);
                    out[b2 + k] = Limb(s2 - q2 * BASE);
                    out[b3 + k] = Limb(s3 - q3 * BASE);
                    c0 = q0; c1 = q1; c2 = q2; c3 = q3;
                }
                carry = c3;
                for (i = b3 + seg; i < n; ++i)
                {
                    carry += cvtRoundU64(v + i);
                    uint64_t q = divBASE(carry);
                    out[i] = Limb(carry - q * BASE);
                    carry = q;
                }
                uint64_t ov = c0;
                for (size_t p = b1; ov > 0 && p < b2; ++p)
                {
                    uint64_t s = uint64_t(out[p]) + ov;
                    uint64_t q = divBASE(s);
                    out[p] = Limb(s - q * BASE);
                    ov = q;
                }
                ov += c1;
                for (size_t p = b2; ov > 0 && p < b3; ++p)
                {
                    uint64_t s = uint64_t(out[p]) + ov;
                    uint64_t q = divBASE(s);
                    out[p] = Limb(s - q * BASE);
                    ov = q;
                }
                ov += c2;
                for (size_t p = b3; ov > 0 && p < n; ++p)
                {
                    uint64_t s = uint64_t(out[p]) + ov;
                    uint64_t q = divBASE(s);
                    out[p] = Limb(s - q * BASE);
                    ov = q;
                }
                return carry + ov;
            }
            for (; i + 7 < n; i += 8)
            {
                HINT_PREFETCH(v + i + 16, 0, 0);
                HINT_PREFETCH(v + i + 24, 0, 0);
                uint64_t s0 = carry + cvtRoundU64(v + i);
                uint64_t q0 = divBASE(s0);
                uint64_t s1 = q0 + cvtRoundU64(v + i + 1);
                uint64_t q1 = divBASE(s1);
                uint64_t s2 = q1 + cvtRoundU64(v + i + 2);
                uint64_t q2 = divBASE(s2);
                uint64_t s3 = q2 + cvtRoundU64(v + i + 3);
                uint64_t q3 = divBASE(s3);
                uint64_t s4 = q3 + cvtRoundU64(v + i + 4);
                uint64_t q4 = divBASE(s4);
                uint64_t s5 = q4 + cvtRoundU64(v + i + 5);
                uint64_t q5 = divBASE(s5);
                uint64_t s6 = q5 + cvtRoundU64(v + i + 6);
                uint64_t q6 = divBASE(s6);
                uint64_t s7 = q6 + cvtRoundU64(v + i + 7);
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
            for (; i < n; ++i)
            {
                carry += cvtRoundU64(v + i);
                uint64_t q = divBASE(carry);
                out[i] = Limb(carry - q * BASE);
                carry = q;
            }
            return carry;
        }
```

### 2.12 absInvNewton — 2.15%  （Newton 倒数; 递归 + absSqr + fftMulPre/absMul + 末位校正）
```cpp
        static void absInvNewton(View m, Span inv,
                                 const double *m_dft = nullptr, size_t m_dft_float_len = 0)
        {
            size_t k = m.size;
            assert(k > 0);
            assert(inv.size >= k + 1);
            if (k <= INV_NEWTON_BASE_THRESHOLD)
            {
                Limb b_2k[INV_NEWTON_BASE_THRESHOLD * 2 + 2];
                b_2k[k * 2] = 1;
                std::fill_n(b_2k, k * 2, Limb(0));
                absDivBasicCore(Span(b_2k, k * 2 + 1), m, inv);
                return;
            }
            size_t s = (k - 1) / 2;
            absInvNewton(m + s, inv);
            size_t inv0_len = k - s + 1;
            Span inv0(inv.ptr, inv0_len);
            thread_local std::vector<Limb> tprod, tinv2;
            size_t prod_size = inv0_len * 2 + k;
            size_t inv2_size = k + 1;
            if (tprod.capacity() < prod_size) tprod.reserve(prod_size);
            if (tinv2.capacity() < inv2_size) tinv2.reserve(inv2_size);
            std::fill_n(tinv2.data(), s, Limb(0));
            Span prod_span(tprod.data(), prod_size), inv2_span(tinv2.data(), inv2_size);
            bool cf = absAdd(inv0, inv0, inv2_span + s);
            assert(!cf);
            absSqr(inv0, prod_span);
            {
                size_t conv_len = inv0_len * 2 + k - 1;
                size_t need_float_len = fft_ceil_lin(conv_len);
#ifndef DISABLE_FFT_ALL
                if (m_dft != nullptr && m_dft_float_len == need_float_len)
                    fftMulPre(View(tprod.data(), inv0_len * 2), m_dft, k, need_float_len, prod_span);
                else
#endif
                    absMul(View(tprod.data(), inv0_len * 2), m, prod_span);
            }
            prod_span = prod_span + 2 * (k - s);
            assert(prod_span[prod_span.size - 1] == 0);
            prod_span.size--;
            absSub(inv2_span, prod_span, inv);
            // 末位校正标量 limb 循环 (inv·m 与 B^{2k} 比较, 偏大则 inv--)
            {
                thread_local std::vector<Limb> tchk;
                if (tchk.capacity() < size_t(2 * k + 3)) tchk.resize(2 * k + 3);
                for (int _corr = 0; _corr < 4; _corr++)
                {
                    std::fill_n(tchk.data(), 2 * k + 2, Limb(0));
                    absMul(View(inv.ptr, k + 1), m, Span(tchk.data(), 2 * k + 1));
                    bool gtB = false;
                    if (tchk[2 * k] > 1) gtB = true;
                    else if (tchk[2 * k] == 1)
                    {
                        for (size_t _j = 0; _j < 2 * k; _j++) if (tchk[_j]) { gtB = true; break; }
                    }
                    if (gtB)
                    {
                        Limb _bw = 1;
                        for (size_t _j = 0; _j < k + 1 && _bw; _j++)
                            inv[_j] = sub_half<Limb>(inv[_j], _bw, BASE, _bw);
                        continue;
                    }
                    Limb _c = 0;
                    for (size_t _j = 0; _j < k; _j++) { /* (inv+1)·m <= B^{2k} 判定, 偏小则 inv++ */ }
                    // ... (省略尾部, 非热点)
                }
            }
        }
```

## 3. 向量化原语（AVX2, 已 inline；局部优化的作用域）

### 3.1 C4d 结构 + load/store/mul/mulConj
```cpp
            struct C4d { __m256d re, im; };
            HINT_AI C4d c4load(const double *p)
            { return C4d{_mm256_load_pd(p), _mm256_load_pd(p + 4)}; }
            HINT_AI void c4store(double *p, const C4d &c)
            { _mm256_store_pd(p, c.re); _mm256_store_pd(p + 4, c.im); }
            HINT_AI C4d c4mul(const C4d &a, const C4d &b)
            { return C4d{a.re * b.re - a.im * b.im, a.im * b.re + a.re * b.im}; }
            HINT_AI C4d c4mulConj(const C4d &a, const C4d &b)
            { return C4d{a.re * b.re + a.im * b.im, a.im * b.re - a.re * b.im}; }
```

### 3.2 difSplitC4 / iditSplitC4（AVX2 4 复数蝶形）
```cpp
            HINT_AI void difSplitC4(C4d &c0, C4d &c1, C4d &c2, C4d &c3)
            {
                const __m256d ar = c0.re - c2.re, ai = c0.im - c2.im;
                const __m256d br = c1.re - c3.re, bi = c1.im - c3.im;
                c0.re = c0.re + c2.re; c0.im = c0.im + c2.im;
                c1.re = c1.re + c3.re; c1.im = c1.im + c3.im;
                c2.re = ar + bi; c2.im = ai - br;
                c3.re = ar - bi; c3.im = ai + br;
            }
            HINT_AI void iditSplitC4(C4d &c0, C4d &c1, C4d &c2, C4d &c3)
            {
                const __m256d sr = c2.re + c3.re, si = c2.im + c3.im;
                const __m256d dr = c2.re - c3.re, di = c2.im - c3.im;
                c2.re = c0.re - sr; c2.im = c0.im - si;
                c0.re = c0.re + sr; c0.im = c0.im + si;
                c3.re = c1.re + di; c3.im = c1.im - dr;
                c1.re = c1.re - di; c1.im = c1.im + dr;
            }
```

### 3.3 dot_rfftX2 / c4twiddle / dot_rfftX4（rdot 重建 AVX2 核心）
```cpp
            inline void dot_rfftX2(Float *inout0, Float *inout1, const Float *in0, const Float *in1,
                                   const Complex2<Float> &omega0, const Float2<Float> &inv)
            {
                using C2 = Complex2<Float>;
                auto mul1 = [](C2 c0, C2 c1) {
                    return C2(c0.imag * c1.real + c0.real * c1.imag,
                              c0.imag * c1.imag - c0.real * c1.real);
                };
                auto mul2 = [](C2 c0, C2 c1) {
                    return C2(c0.real * c1.imag - c0.imag * c1.real,
                              c0.real * c1.real + c0.imag * c1.imag);
                };
                auto compute2 = [&omega0](C2 c0, C2 c1, C2 &out0, C2 &out1, auto Func) {
                    C2 t0(c0.real + c1.real, c0.imag - c1.imag), t1(c0.real - c1.real, c0.imag + c1.imag);
                    t1 = Func(t1, omega0);
                    out0 = t0 + t1;
                    out1.real = t0.real - t1.real;
                    out1.imag = t1.imag - t0.imag;
                };
                C2 c0, c1;
                {
                    C2 x0, x1, x2, x3;
                    c0.load(inout0), c1.load(inout1);
                    compute2(c0, c1.reverse(), x0, x1, mul1);
                    c0.load(in0), c1.load(in1);
                    compute2(c0, c1.reverse(), x2, x3, mul1);
                    c0 = x0.mul(x2) * inv;
                    c1 = x1.mul(x3) * inv;
                    compute2(c0, c1, c0, c1, mul2);
                }
                c0.store(inout0), c1.reverse().store(inout1);
            }

            HINT_AI C4d c4twiddle(const __m256d &w0, const __m256d &w1)
            { return C4d{_mm256_permute2f128_pd(w0, w1, 0x20), _mm256_permute2f128_pd(w0, w1, 0x31)}; }

            HINT_AI void dot_rfftX4(double *inout0, double *inout1, const double *in0, const double *in1,
                                    const C4d &om, const __m256d &inv)
            {
                auto mul1 = [](const C4d &c0, const C4d &c1) {
                    return C4d{c0.im * c1.re + c0.re * c1.im, c0.im * c1.im - c0.re * c1.re};
                };
                auto mul2 = [](const C4d &c0, const C4d &c1) {
                    return C4d{c0.re * c1.im - c0.im * c1.re, c0.re * c1.re + c0.im * c1.im};
                };
                auto compute2 = [&om](const C4d &c0, const C4d &c1, C4d &out0, C4d &out1, auto Func) {
                    C4d t0{c0.re + c1.re, c0.im - c1.im}, t1{c0.re - c1.re, c0.im + c1.im};
                    t1 = Func(t1, om);
                    out0 = C4d{t0.re + t1.re, t0.im + t1.im};
                    out1 = C4d{t0.re - t1.re, t1.im - t0.im};
                };
                C4d c0, c1;
                {
                    C4d x0, x1, x2, x3;
                    c0 = c4loadC2(inout0), c1 = c4loadC2Rev(inout1);
                    compute2(c0, c1, x0, x1, mul1);
                    c0 = c4loadC2(in0), c1 = c4loadC2Rev(in1);
                    compute2(c0, c1, x2, x3, mul1);
                    c0 = c4mul(x0, x2); c0.re = c0.re * inv, c0.im = c0.im * inv;
                    c1 = c4mul(x1, x3); c1.re = c1.re * inv, c1.im = c1.im * inv;
                    compute2(c0, c1, c0, c1, mul2);
                }
                c4storeC2(inout0, c0);
                c4storeC2(inout1, c4rev(c1));
            }
```

### 3.4 transform2 / difSplit / iditSplit（标量小长度用）
```cpp
            inline void transform2(T &sum, T &diff) { T t0 = sum, t1 = diff; sum = t0 + t1; diff = t0 - t1; }
            template <typename Float>
            inline void difSplit(Float &r0, Float &i0, Float &r1, Float &i1, Float &r2, Float &i2, Float &r3, Float &i3)
            {
                transform2(r0, r2); transform2(i0, i2); transform2(r1, i3); transform2(i1, i3);
                transform2(r2, i3); transform2(i2, r3, r3, i2); std::swap(i3, r3);
            }
            template <typename Float>
            inline void iditSplit(Float &r0, Float &i0, Float &r1, Float &i1, Float &r2, Float &i2, Float &r3, Float &i3)
            {
                transform2(r2, r3); transform2(i2, i3);
                transform2(r0, r2); transform2(i0, i2); transform2(r1, i3, i3, r1); transform2(i1, r3);
                std::swap(i3, r3);
            }
```

## 4. 局部优化约束（给 LLM）

- **不做算法级改动**：保留 real-FFT 结构、C4 布局、twiddle 表、递归分治；不改 `fft_ceil` 档位分发。
- **仅限 AVX2 指令集**（目标 EPYC 7B13 无 AVX-512）；不引 512 位。
- **数值必须逐位等价**（除法结果精确）；不依赖 `-ffast-math` 融舍入（会改数值）。
- 自由度：循环展开深度 / FMA 调度与融合 / 寄存器分配（避免 spill）/ 指令选择（等价改写）/ prefetch 策略（注：P0 已删 `HINT_PREFETCH` 降指令，重加只降 cycles）/ 微数据布局 / cache-blocking（仅降 cycles，不影响 `instructions:u`）。
- **收口判据**：9gen×Nseed oracle 全过 **且** `instructions:u` ≤ 449.0M（基准）/ 426.36M（单 one_case）。回归即否决。
- 目标：请基于以上热点 + ISA，给出 **`instructions:u` 理论下界**与可达的局部优化（不接受跨函数重构 / rdot 融合——已证对指令数零收益）。
