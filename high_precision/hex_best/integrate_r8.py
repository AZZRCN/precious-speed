#!/usr/bin/env python3
# T111: 把混合基 radix-8 (rev3 store) + radix-4 底座 (rev2 store) 集成进 div_base16_opt.cpp
# 幂等: 重跑不会重复插入。开关 FFT_R8 (默认 1), 阈值 FFT_C4_MIN 用 -D 覆盖。
import io, os, sys, re

SRC = r"D:\precious_speed\hex_best\div_base16_opt.cpp"
s = io.open(SRC, encoding="utf-8", errors="surrogateescape").read()
orig_len = len(s)

MARK = "T111_R8_INTEGRATED"
if MARK in s:
    print("already integrated, skip")
    sys.exit(0)

# ---------- 1. 开关 ----------
old = "#ifndef FFT_C4_MIN\n#define FFT_C4_MIN 32"
assert s.count(old) == 1, "FFT_C4_MIN anchor"
s = s.replace(old, "// " + MARK + "\n#ifndef FFT_R8\n#define FFT_R8 1\n#endif\n"
                  + old, 1)

# ---------- 2. 重命名旧 split-radix C4 主体 ----------
ren = [
    ("                void difC4(Float inout[], size_t float_len)",
     "                void difC4SR(Float inout[], size_t float_len)"),
    ("                void iditC4(Float inout[], size_t float_len)",
     "                void iditC4SR(Float inout[], size_t float_len)"),
    ("                        difC4<0>(inout, stride * 2);\n"
     "                        difC4<0>(inout + stride * 2, stride);\n"
     "                        difC4<0>(inout + stride * 3, stride);",
     "                        difC4SR<0>(inout, stride * 2);\n"
     "                        difC4SR<0>(inout + stride * 2, stride);\n"
     "                        difC4SR<0>(inout + stride * 3, stride);"),
    ("                        iditC4<0>(inout, stride * 2);\n"
     "                        iditC4<0>(inout + stride * 2, stride);\n"
     "                        iditC4<0>(inout + stride * 3, stride);",
     "                        iditC4SR<0>(inout, stride * 2);\n"
     "                        iditC4SR<0>(inout + stride * 2, stride);\n"
     "                        iditC4SR<0>(inout + stride * 3, stride);"),
]
for a, b in ren:
    assert s.count(a) == 1, "rename anchor: " + a[:60]
    s = s.replace(a, b, 1)

# ---------- 3. expand() 挂 expandR8 ----------
old = ("                void expand(size_t float_len)\n"
       "                {\n"
       "                    table1.expand(float_len / 2);\n"
       "                    table3.expand(float_len / 2);\n"
       "                }")
assert s.count(old) == 1, "expand anchor"
s = s.replace(old,
"""                void expand(size_t float_len)
                {
                    table1.expand(float_len / 2);
                    table3.expand(float_len / 2);
#if FFT_R8
                    expandR8(float_len);
#endif
                }
#if FFT_R8
                // T111: radix-8 交织 twiddle 表。每组 g (4 个 j) 连续存 w1..w7,
                // 各 8 double (RRRR|IIII) = 56 double = 448 B (32B 对齐)。
                // w1/w3 直接取自 table1/table3 (同 rank 段的前一半, C4 布局),
                // 其余 4 个由同层 factor 递推得出: w2=w1^2, w4=w2^2, w6=w3^2,
                // w5=w4*w1, w7=w6*w1 —— 零 trig 调用, 误差 <= 3 eps。
                // 只建 radix-8 实际用到的层 (rank = N, N/8, N/64, ...),
                // 总量 = 7*(N/8)*(8/7) = N 复数 = 2N double, 与 split-radix 的
                // table1+table3 (2N double) 持平 —— twiddle 内存零增长。
                void expandR8(size_t float_len)
                {
                    if constexpr (std::is_same_v<Float, double>)
                    {
                        for (size_t fl = float_len; fl >= 64; fl /= 8)
                        {
                            const size_t fft_len = fl / 2;
                            const size_t M = fft_len / 8;
                            const size_t lg = lg_pow2_6(fft_len);
                            if (lg >= 40 || r8m[lg] >= M)
                            {
                                continue;
                            }
                            const size_t G = M / 4;
                            r8seg[lg].resize(G * 56);
                            auto t1 = reinterpret_cast<const double *>(table1.getBegin(fft_len));
                            auto t3 = reinterpret_cast<const double *>(table3.getBegin(fft_len));
                            double *p = r8seg[lg].data();
                            for (size_t g = 0; g < G; ++g, t1 += 8, t3 += 8, p += 56)
                            {
                                const C4d w1 = c4load(t1), w3 = c4load(t3);
                                const C4d w2 = c4mul(w1, w1);
                                const C4d w4 = c4mul(w2, w2);
                                const C4d w6 = c4mul(w3, w3);
                                const C4d w5 = c4mul(w4, w1);
                                const C4d w7 = c4mul(w6, w1);
                                c4store(p, w1);
                                c4store(p + 8, w2);
                                c4store(p + 16, w3);
                                c4store(p + 24, w4);
                                c4store(p + 32, w5);
                                c4store(p + 40, w6);
                                c4store(p + 48, w7);
                            }
                            r8m[lg] = M;
                        }
                    }
                }
#endif""", 1)

# ---------- 4. 构造函数初始化 r8m ----------
old = "                FFT() : table1(1), table3(3) {}"
assert s.count(old) == 1, "ctor anchor"
s = s.replace(old,
"""                FFT() : table1(1), table3(3)
                {
#if FFT_R8
                    for (int i = 0; i < 40; ++i) r8m[i] = 0;
#endif
                }""", 1)

# ---------- 5. private 成员 ----------
old = "            private:\n                Table table1, table3;\n            };"
assert s.count(old) == 1, "private anchor"
s = s.replace(old,
"""            private:
                Table table1, table3;
#if FFT_R8
                AlignedVec32<double> r8seg[40];
                size_t r8m[40];
#endif
            };""", 1)

# ---------- 6. 插入 radix-8 主体 + dispatcher (放在 iditC4SR 之后) ----------
anchor = """                template <bool RIRI_IN>
                HINT_AI_SMALL void difSmall(Float inout[], size_t float_len)"""
assert s.count(anchor) == 1, "difSmall anchor"

R8_BODY = r"""
#if FFT_R8
                // ================= T111: 混合基 radix-8 C4 主体 =================
                // radix-8 一趟砍掉 3 层 radix-2 蝶形, 蝶形 pass 数 = log8(N) = (1/3)log2(N)。
                // 输出用 rev3 store: y_m -> 子块 rev3(m) = {0,4,2,6,1,5,3,7}, 递归后
                // 位置数字串恰好等于 bitrev(m) —— 输出序与 split-radix 逐元素相同,
                // 故 rdot 的 binrev 契约、叶子 packC4+dif<false> 链路全部不动。
                // float_len < 64 降级 radix-4 (rev2 store), 保证 q2 >= C4d 宽度不漏算。
                template <int IN_MODE>
                void difC4R4(Float inout[], size_t float_len)
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
                        const size_t q2 = float_len / 4;
                        auto t1 = reinterpret_cast<const double *>(
                            __builtin_assume_aligned(table1.getBegin(fft_len), 32));
                        auto t3 = reinterpret_cast<const double *>(
                            __builtin_assume_aligned(table3.getBegin(fft_len), 32));
                        double *base = reinterpret_cast<double *>(__builtin_assume_aligned(inout, 32));
                        const size_t tb = fft_len / 16;
                        for (size_t g = 0; g < tb; ++g, base += 8, t1 += 8, t3 += 8)
                        {
                            double *L0 = base, *L1 = base + q2, *L2 = base + 2 * q2, *L3 = base + 3 * q2;
                            const C4d a0 = c4loadM<IN_MODE>(L0), a1 = c4loadM<IN_MODE>(L1);
                            const C4d a2 = c4loadM<IN_MODE>(L2), a3 = c4loadM<IN_MODE>(L3);
                            const __m256d s0r = a0.re + a2.re, s0i = a0.im + a2.im;
                            const __m256d d0r = a0.re - a2.re, d0i = a0.im - a2.im;
                            const __m256d s1r = a1.re + a3.re, s1i = a1.im + a3.im;
                            const __m256d d1r = a1.re - a3.re, d1i = a1.im - a3.im;
                            const C4d y0{s0r + s1r, s0i + s1i};
                            const C4d y1{d0r + d1i, d0i - d1r};
                            const C4d y2{s0r - s1r, s0i - s1i};
                            const C4d y3{d0r - d1i, d0i + d1r};
                            const C4d w1 = c4load(t1), w3 = c4load(t3);
                            const C4d w2 = c4mul(w1, w1);
                            c4store(L0, y0);
                            c4store(L2, c4mul(y1, w1));
                            c4store(L1, c4mul(y2, w2));
                            c4store(L3, c4mul(y3, w3));
                        }
                        const size_t sub = float_len / 4;
                        for (int r = 0; r < 4; ++r)
                        {
                            difC4R4<0>(inout + r * sub, sub);
                        }
                    }
                }
                template <int IN_MODE>
                void difC4R8(Float inout[], size_t float_len)
                {
                    if constexpr (std::is_same_v<Float, double>)
                    {
                        if (float_len <= FFT_C4_MIN)
                        {
                            packC4(inout, float_len);
                            dif<false>(inout, float_len);
                            return;
                        }
                        if (float_len < 64)
                        {
                            difC4R4<IN_MODE>(inout, float_len);
                            return;
                        }
                        const size_t fft_len = float_len / 2;
                        const size_t q2 = float_len / 8;
                        const double *tw = reinterpret_cast<const double *>(
                            __builtin_assume_aligned(r8seg[lg_pow2_6(fft_len)].data(), 32));
                        const __m256d s8 = _mm256_set1_pd(0.70710678118654752440084436210485);
                        const __m256d zero = _mm256_setzero_pd();
                        double *base = reinterpret_cast<double *>(__builtin_assume_aligned(inout, 32));
                        const size_t tb = fft_len / 32;
                        for (size_t g = 0; g < tb; ++g, base += 8, tw += 56)
                        {
                            double *L0 = base, *L1 = base + q2, *L2 = base + 2 * q2, *L3 = base + 3 * q2;
                            double *L4 = base + 4 * q2, *L5 = base + 5 * q2, *L6 = base + 6 * q2, *L7 = base + 7 * q2;
                            const C4d a0 = c4loadM<IN_MODE>(L0), a1 = c4loadM<IN_MODE>(L1);
                            const C4d a2 = c4loadM<IN_MODE>(L2), a3 = c4loadM<IN_MODE>(L3);
                            const C4d a4 = c4loadM<IN_MODE>(L4), a5 = c4loadM<IN_MODE>(L5);
                            const C4d a6 = c4loadM<IN_MODE>(L6), a7 = c4loadM<IN_MODE>(L7);
                            // stage 1: b = 上半 + 下半, e = 上半 - 下半
                            const __m256d b0r = a0.re + a4.re, b0i = a0.im + a4.im;
                            const __m256d b1r = a1.re + a5.re, b1i = a1.im + a5.im;
                            const __m256d b2r = a2.re + a6.re, b2i = a2.im + a6.im;
                            const __m256d b3r = a3.re + a7.re, b3i = a3.im + a7.im;
                            const __m256d e0r = a0.re - a4.re, e0i = a0.im - a4.im;
                            const __m256d e1r = a1.re - a5.re, e1i = a1.im - a5.im;
                            const __m256d e2r = a2.re - a6.re, e2i = a2.im - a6.im;
                            const __m256d e3r = a3.re - a7.re, e3i = a3.im - a7.im;
                            // c_m = e_m * W8^m  (W8 = exp(-i*pi/4))
                            const __m256d c0r = e0r, c0i = e0i;
                            const __m256d c1r = _mm256_mul_pd(s8, e1r + e1i), c1i = _mm256_mul_pd(s8, e1i - e1r);
                            const __m256d c2r = e2i, c2i = zero - e2r;
                            const __m256d c3r = _mm256_mul_pd(s8, e3i - e3r), c3i = _mm256_mul_pd(s8, zero - (e3i + e3r));
                            // 两个 DFT4
                            const __m256d s0r = b0r + b2r, s0i = b0i + b2i, d0r = b0r - b2r, d0i = b0i - b2i;
                            const __m256d s1r = b1r + b3r, s1i = b1i + b3i, d1r = b1r - b3r, d1i = b1i - b3i;
                            const C4d y0{s0r + s1r, s0i + s1i};
                            const C4d y4{s0r - s1r, s0i - s1i};
                            const C4d y2{d0r + d1i, d0i - d1r};
                            const C4d y6{d0r - d1i, d0i + d1r};
                            const __m256d t0r = c0r + c2r, t0i = c0i + c2i, f0r = c0r - c2r, f0i = c0i - c2i;
                            const __m256d t1r = c1r + c3r, t1i = c1i + c3i, f1r = c1r - c3r, f1i = c1i - c3i;
                            const C4d y1{t0r + t1r, t0i + t1i};
                            const C4d y5{t0r - t1r, t0i - t1i};
                            const C4d y3{f0r + f1i, f0i - f1r};
                            const C4d y7{f0r - f1i, f0i + f1r};
                            // rev3 store: 0->0 1->4 2->2 3->6 4->1 5->5 6->3 7->7
                            c4store(L0, y0);
                            c4store(L4, c4mul(y1, c4load(tw)));
                            c4store(L2, c4mul(y2, c4load(tw + 8)));
                            c4store(L6, c4mul(y3, c4load(tw + 16)));
                            c4store(L1, c4mul(y4, c4load(tw + 24)));
                            c4store(L5, c4mul(y5, c4load(tw + 32)));
                            c4store(L3, c4mul(y6, c4load(tw + 40)));
                            c4store(L7, c4mul(y7, c4load(tw + 48)));
                        }
                        const size_t sub = float_len / 8;
                        for (int r = 0; r < 8; ++r)
                        {
                            difC4R8<0>(inout + r * sub, sub);
                        }
                    }
                }
                template <int OUT_MODE>
                void iditC4R4(Float inout[], size_t float_len)
                {
                    if constexpr (std::is_same_v<Float, double>)
                    {
                        if (float_len <= FFT_C4_MIN)
                        {
                            idit<false>(inout, float_len);
                            packC4(inout, float_len);
                            return;
                        }
                        const size_t sub = float_len / 4;
                        for (int r = 0; r < 4; ++r)
                        {
                            iditC4R4<0>(inout + r * sub, sub);
                        }
                        const size_t fft_len = float_len / 2;
                        const size_t q2 = float_len / 4;
                        auto t1 = reinterpret_cast<const double *>(
                            __builtin_assume_aligned(table1.getBegin(fft_len), 32));
                        auto t3 = reinterpret_cast<const double *>(
                            __builtin_assume_aligned(table3.getBegin(fft_len), 32));
                        double *base = reinterpret_cast<double *>(__builtin_assume_aligned(inout, 32));
                        const size_t tb = fft_len / 16;
                        for (size_t g = 0; g < tb; ++g, base += 8, t1 += 8, t3 += 8)
                        {
                            double *L0 = base, *L1 = base + q2, *L2 = base + 2 * q2, *L3 = base + 3 * q2;
                            const C4d w1 = c4load(t1), w3 = c4load(t3);
                            const C4d w2 = c4mul(w1, w1);
                            const C4d y0 = c4load(L0);
                            const C4d y1 = c4mulConj(c4load(L2), w1);
                            const C4d y2 = c4mulConj(c4load(L1), w2);
                            const C4d y3 = c4mulConj(c4load(L3), w3);
                            const __m256d s0r = y0.re + y2.re, s0i = y0.im + y2.im;
                            const __m256d d0r = y0.re - y2.re, d0i = y0.im - y2.im;
                            const __m256d s1r = y1.re + y3.re, s1i = y1.im + y3.im;
                            const __m256d d1r = y1.re - y3.re, d1i = y1.im - y3.im;
                            c4storeM<OUT_MODE>(L0, C4d{s0r + s1r, s0i + s1i});
                            c4storeM<OUT_MODE>(L1, C4d{d0r - d1i, d0i + d1r});
                            c4storeM<OUT_MODE>(L2, C4d{s0r - s1r, s0i - s1i});
                            c4storeM<OUT_MODE>(L3, C4d{d0r + d1i, d0i - d1r});
                        }
                    }
                }
                template <int OUT_MODE>
                void iditC4R8(Float inout[], size_t float_len)
                {
                    if constexpr (std::is_same_v<Float, double>)
                    {
                        if (float_len <= FFT_C4_MIN)
                        {
                            idit<false>(inout, float_len);
                            packC4(inout, float_len);
                            return;
                        }
                        if (float_len < 64)
                        {
                            iditC4R4<OUT_MODE>(inout, float_len);
                            return;
                        }
                        const size_t sub = float_len / 8;
                        for (int r = 0; r < 8; ++r)
                        {
                            iditC4R8<0>(inout + r * sub, sub);
                        }
                        const size_t fft_len = float_len / 2;
                        const size_t q2 = float_len / 8;
                        const double *tw = reinterpret_cast<const double *>(
                            __builtin_assume_aligned(r8seg[lg_pow2_6(fft_len)].data(), 32));
                        const __m256d s8 = _mm256_set1_pd(0.70710678118654752440084436210485);
                        const __m256d zero = _mm256_setzero_pd();
                        double *base = reinterpret_cast<double *>(__builtin_assume_aligned(inout, 32));
                        const size_t tb = fft_len / 32;
                        for (size_t g = 0; g < tb; ++g, base += 8, tw += 56)
                        {
                            double *L0 = base, *L1 = base + q2, *L2 = base + 2 * q2, *L3 = base + 3 * q2;
                            double *L4 = base + 4 * q2, *L5 = base + 5 * q2, *L6 = base + 6 * q2, *L7 = base + 7 * q2;
                            const C4d y0 = c4load(L0);
                            const C4d y1 = c4mulConj(c4load(L4), c4load(tw));
                            const C4d y2 = c4mulConj(c4load(L2), c4load(tw + 8));
                            const C4d y3 = c4mulConj(c4load(L6), c4load(tw + 16));
                            const C4d y4 = c4mulConj(c4load(L1), c4load(tw + 24));
                            const C4d y5 = c4mulConj(c4load(L5), c4load(tw + 32));
                            const C4d y6 = c4mulConj(c4load(L3), c4load(tw + 40));
                            const C4d y7 = c4mulConj(c4load(L7), c4load(tw + 48));
                            const __m256d s0r = y0.re + y4.re, s0i = y0.im + y4.im;
                            const __m256d d0r = y0.re - y4.re, d0i = y0.im - y4.im;
                            const __m256d s1r = y2.re + y6.re, s1i = y2.im + y6.im;
                            const __m256d d1r = y2.re - y6.re, d1i = y2.im - y6.im;
                            const __m256d b0r = s0r + s1r, b0i = s0i + s1i;
                            const __m256d b2r = s0r - s1r, b2i = s0i - s1i;
                            const __m256d b1r = d0r - d1i, b1i = d0i + d1r;
                            const __m256d b3r = d0r + d1i, b3i = d0i - d1r;
                            const __m256d t0r = y1.re + y5.re, t0i = y1.im + y5.im;
                            const __m256d f0r = y1.re - y5.re, f0i = y1.im - y5.im;
                            const __m256d t1r = y3.re + y7.re, t1i = y3.im + y7.im;
                            const __m256d f1r = y3.re - y7.re, f1i = y3.im - y7.im;
                            const __m256d c0r = t0r + t1r, c0i = t0i + t1i;
                            const __m256d c2r = t0r - t1r, c2i = t0i - t1i;
                            const __m256d c1r = f0r - f1i, c1i = f0i + f1r;
                            const __m256d c3r = f0r + f1i, c3i = f0i - f1r;
                            const __m256d e0r = c0r, e0i = c0i;
                            const __m256d e1r = _mm256_mul_pd(s8, c1r - c1i), e1i = _mm256_mul_pd(s8, c1i + c1r);
                            const __m256d e2r = zero - c2i, e2i = c2r;
                            const __m256d e3r = _mm256_mul_pd(s8, zero - (c3r + c3i)), e3i = _mm256_mul_pd(s8, c3r - c3i);
                            c4storeM<OUT_MODE>(L0, C4d{b0r + e0r, b0i + e0i});
                            c4storeM<OUT_MODE>(L4, C4d{b0r - e0r, b0i - e0i});
                            c4storeM<OUT_MODE>(L1, C4d{b1r + e1r, b1i + e1i});
                            c4storeM<OUT_MODE>(L5, C4d{b1r - e1r, b1i - e1i});
                            c4storeM<OUT_MODE>(L2, C4d{b2r + e2r, b2i + e2i});
                            c4storeM<OUT_MODE>(L6, C4d{b2r - e2r, b2i - e2i});
                            c4storeM<OUT_MODE>(L3, C4d{b3r + e3r, b3i + e3i});
                            c4storeM<OUT_MODE>(L7, C4d{b3r - e3r, b3i - e3i});
                        }
                    }
                }
#endif
                // ---- C4 主体分派: FFT_R8 决定走 radix-8 混合基还是原 split-radix ----
                template <int IN_MODE>
                void difC4(Float inout[], size_t float_len)
                {
#if FFT_R8
                    difC4R8<IN_MODE>(inout, float_len);
#else
                    difC4SR<IN_MODE>(inout, float_len);
#endif
                }
                template <int OUT_MODE>
                void iditC4(Float inout[], size_t float_len)
                {
#if FFT_R8
                    iditC4R8<OUT_MODE>(inout, float_len);
#else
                    iditC4SR<OUT_MODE>(inout, float_len);
#endif
                }

"""
s = s.replace(anchor, R8_BODY + anchor, 1)

io.open(SRC, "w", encoding="utf-8", errors="surrogateescape").write(s)
print("OK: %d -> %d bytes (+%d)" % (orig_len, len(s), len(s) - orig_len))
