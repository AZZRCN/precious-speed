# _patch_d21.py —— D19/D20c -> D21: FFT 蝶形改用 C4 (4-complex SoA) 布局
#
# 依据 (c4probe 实测):
#   C2(RRII, 2 复数/ymm) 蝶形 44 insn/iter 处理 8 复数, mov/shuf:arith = 1.50, vpermpd 10/iter
#   C4(RRRRIIII, 4 复数/2ymm)     44 insn/iter 处理 16 复数, ratio = 1.00,      vpermpd 0/iter
#   => 每复数指令数精确 2x
#
# 可行性关键: Complex2{F2 real, imag} 的内存就是 RRII。C4 只是把连续 4 个复数的
#   re/im 分开存, **逻辑索引不变**, FFT 数学不变, twiddle 只需同序重新打包。
#   RRII|RRII <-> RRRR|IIII 只要 2 条 vperm2f128, 而且这个变换是**自逆的**。
#   => C4 版结果应 bit-identical 于 C2 版, 可直接对拍。
#
# 策略 (渐进, 低风险): C4 完全封闭在 FFT::dif / FFT::idit 内部。
#   dif :  入口 pack 一次 -> difC4 递归 -> 子长度 <= FFT_C4_MIN 时 unpack 回 C2 走原路径
#   idit:  反向对称
#   dif3Stage / dot_rfftX2 / real_dot_binrev3 / 进位归一化 全部不动。
import io, os, re, sys

SRC = r"D:\precious_speed\best\div_D19.cpp"
DST = r"D:\precious_speed\best\div_D21.cpp"

s = io.open(SRC, "r", encoding="utf-8", newline="").read()
orig_len = len(s)

# ---------------------------------------------------------------- 1) C4 基础设施
ANCHOR1 = """            template <typename Float, int DIV>
            struct FFTTable
"""
C4_INFRA = r"""            // ================= D21: 4-complex SoA (C4) butterfly layout =================
            // C2 布局 {r0,r1,i0,i1} 里 real/imag 各占半个 ymm, 于是
            //   (a) 复数乘的 real*o.real 只有 128-bit 有效宽度
            //   (b) difSplit 尾部 transform2(r2,i3) / swap(i3,r3) 跨 real/imag 分区
            // 两者都逼 GCC 发 vpermpd/vblendpd。C4 把 4 个复数的 re/im 各放一整个 ymm,
            // 上述两处全部变成同 lane 全宽运算, shuffle 归零。
#ifndef FFT_C4_MIN
#define FFT_C4_MIN 64
#endif
            struct C4d
            {
                __m256d re, im;
            };
            HINT_AI C4d c4load(const double *p)
            {
                return C4d{_mm256_load_pd(p), _mm256_load_pd(p + 4)};
            }
            HINT_AI void c4store(double *p, const C4d &c)
            {
                _mm256_store_pd(p, c.re);
                _mm256_store_pd(p + 4, c.im);
            }
            // 用 __m256d 运算符而非 fma intrinsic: 顶层 pragma target 未开 fma,
            // 交给 GCC 自行融合 (实测仍产出 vfmadd/vfnmadd)。
            HINT_AI C4d c4mul(const C4d &a, const C4d &b)
            {
                return C4d{a.re * b.re - a.im * b.im, a.im * b.re + a.re * b.im};
            }
            HINT_AI C4d c4mulConj(const C4d &a, const C4d &b)
            {
                return C4d{a.re * b.re + a.im * b.im, a.im * b.re - a.re * b.im};
            }
            // difSplit 的 C4 版。原语义 (A=c0-c2, B=c1-c3):
            //   c0 += c2 ; c1 += c3
            //   c2 = (A.re + B.im, A.im - B.re) ; c3 = (A.re - B.im, A.im + B.re)
            HINT_AI void difSplitC4(C4d &c0, C4d &c1, C4d &c2, C4d &c3)
            {
                const __m256d ar = c0.re - c2.re, ai = c0.im - c2.im;
                const __m256d br = c1.re - c3.re, bi = c1.im - c3.im;
                c0.re = c0.re + c2.re;
                c0.im = c0.im + c2.im;
                c1.re = c1.re + c3.re;
                c1.im = c1.im + c3.im;
                c2.re = ar + bi;
                c2.im = ai - br;
                c3.re = ar - bi;
                c3.im = ai + br;
            }
            // iditSplit 的 C4 版。原语义 (S=c2+c3, D=c2-c3):
            //   c0' = c0 + S ; c2' = c0 - S
            //   c1' = (c1.re - D.im, c1.im + D.re) ; c3' = (c1.re + D.im, c1.im - D.re)
            HINT_AI void iditSplitC4(C4d &c0, C4d &c1, C4d &c2, C4d &c3)
            {
                const __m256d sr = c2.re + c3.re, si = c2.im + c3.im;
                const __m256d dr = c2.re - c3.re, di = c2.im - c3.im;
                c2.re = c0.re - sr;
                c2.im = c0.im - si;
                c0.re = c0.re + sr;
                c0.im = c0.im + si;
                c3.re = c1.re + di;
                c3.im = c1.im - dr;
                c1.re = c1.re - di;
                c1.im = c1.im + dr;
            }
            // RRII|RRII <-> RRRR|IIII 。perm2f128(A,B,0x20)/(A,B,0x31) 这一对是**对合**,
            // 所以 pack 和 unpack 是同一个函数。n 必须是 8 的倍数。
            inline void packC4(double *p, size_t n)
            {
                for (size_t i = 0; i + 8 <= n; i += 8)
                {
                    const __m256d a = _mm256_load_pd(p + i);
                    const __m256d b = _mm256_load_pd(p + i + 4);
                    _mm256_store_pd(p + i, _mm256_permute2f128_pd(a, b, 0x20));
                    _mm256_store_pd(p + i + 4, _mm256_permute2f128_pd(a, b, 0x31));
                }
            }
            // RIRI -> RRRR|IIII (只在 dif<true> 最外层用一次)
            inline void packC4RIRI(double *p, size_t n)
            {
                for (size_t i = 0; i + 8 <= n; i += 8)
                {
                    const __m256d a = _mm256_load_pd(p + i);
                    const __m256d b = _mm256_load_pd(p + i + 4);
                    _mm256_store_pd(p + i, _mm256_permute4x64_pd(_mm256_unpacklo_pd(a, b), 0xD8));
                    _mm256_store_pd(p + i + 4, _mm256_permute4x64_pd(_mm256_unpackhi_pd(a, b), 0xD8));
                }
            }
            // RRRR|IIII -> RIRI (只在 idit<true> 最外层用一次)
            inline void unpackC4RIRI(double *p, size_t n)
            {
                for (size_t i = 0; i + 8 <= n; i += 8)
                {
                    const __m256d r = _mm256_load_pd(p + i);
                    const __m256d m = _mm256_load_pd(p + i + 4);
                    const __m256d lo = _mm256_unpacklo_pd(r, m);
                    const __m256d hi = _mm256_unpackhi_pd(r, m);
                    _mm256_store_pd(p + i, _mm256_permute2f128_pd(lo, hi, 0x20));
                    _mm256_store_pd(p + i + 4, _mm256_permute2f128_pd(lo, hi, 0x31));
                }
            }

"""
assert s.count(ANCHOR1) == 1, "anchor1"
s = s.replace(ANCHOR1, C4_INFRA + ANCHOR1)

# ---------------------------------------------------------------- 2) FFTTable: 段级 pack
# difC4 主循环最小 float_len = 2*FFT_C4_MIN, 故 fft_len(=rank) >= FFT_C4_MIN,
# 段起点 = rank*2/DIV >= FFT_C4_MIN*2/DIV。小于该偏移的段仍由 C2 路径读取, 保持原布局。
ANCHOR2 = """                void expand(size_t fft_len)
                {
                    size_t cur_len = table.size() * DIV / 4;
                    if (fft_len <= cur_len)
                    {
                        return;
                    }
                    size_t new_len = fft_len * 4 / DIV;"""
NEW2 = """                // D21: >= C4_OFF 的段按 C4 布局存放 (packC4 自逆, 故递推前先还原)。
                static constexpr size_t C4_OFF = size_t(FFT_C4_MIN) * 2 / DIV;
                void packSegs()
                {
                    if constexpr (std::is_same_v<Float, double>)
                    {
                        if (table.size() > C4_OFF)
                        {
                            packC4(&table[C4_OFF], table.size() - C4_OFF);
                        }
                    }
                }
                void expand(size_t fft_len)
                {
                    size_t cur_len = table.size() * DIV / 4;
                    if (fft_len <= cur_len)
                    {
                        return;
                    }
                    packSegs();
                    size_t new_len = fft_len * 4 / DIV;"""
assert s.count(ANCHOR2) == 1, "anchor2"
s = s.replace(ANCHOR2, NEW2)

ANCHOR3 = """                            omega0.store(it);
                            omega1.store(it + 4);
                        }
                    }
                }"""
NEW3 = """                            omega0.store(it);
                            omega1.store(it + 4);
                        }
                    }
                    packSegs();
                }"""
assert s.count(ANCHOR3) == 1, "anchor3"
s = s.replace(ANCHOR3, NEW3)

# ---------------------------------------------------------------- 3) dif / idit 入口分派
ANCHOR4 = """                template <bool RIRI_IN>
                void dif(Float inout[], size_t float_len)
                {
                    HINT_ASSUME(is_2pow(float_len));
                    if (float_len <= FFT_FIXED_MAX)"""
NEW4 = """                template <bool RIRI_IN>
                void dif(Float inout[], size_t float_len)
                {
                    HINT_ASSUME(is_2pow(float_len));
                    if constexpr (std::is_same_v<Float, double>)
                    {
                        if (float_len > FFT_C4_MIN)
                        {
                            if constexpr (RIRI_IN)
                            {
                                packC4RIRI(inout, float_len);
                            }
                            else
                            {
                                packC4(inout, float_len);
                            }
                            difC4(inout, float_len);
                            return;
                        }
                    }
                    if (float_len <= FFT_FIXED_MAX)"""
assert s.count(ANCHOR4) == 1, "anchor4"
s = s.replace(ANCHOR4, NEW4)

ANCHOR5 = """                template <bool RIRI_OUT>
                void idit(Float inout[], size_t float_len)
                {
                    HINT_ASSUME(is_2pow(float_len));
                    if (float_len <= FFT_FIXED_MAX)"""
NEW5 = """                template <bool RIRI_OUT>
                void idit(Float inout[], size_t float_len)
                {
                    HINT_ASSUME(is_2pow(float_len));
                    if constexpr (std::is_same_v<Float, double>)
                    {
                        if (float_len > FFT_C4_MIN)
                        {
                            iditC4(inout, float_len);
                            if constexpr (RIRI_OUT)
                            {
                                unpackC4RIRI(inout, float_len);
                            }
                            else
                            {
                                packC4(inout, float_len);
                            }
                            return;
                        }
                    }
                    if (float_len <= FFT_FIXED_MAX)"""
assert s.count(ANCHOR5) == 1, "anchor5"
s = s.replace(ANCHOR5, NEW5)

# ---------------------------------------------------------------- 4) difC4 / iditC4
ANCHOR6 = """                template <bool RIRI_IN>
                HINT_AI_SMALL void difSmall(Float inout[], size_t float_len)
"""
C4_BODY = r"""                // ---- D21: C4 布局的 split-radix 主体 ----
                // 索引与 C2 版完全一致: s1 = float_len/4 恰好等于 C2 版的 stride,
                // twiddle 段长 fft_len/2 也精确匹配 (每迭代吃 4 个 twiddle = 8 double)。
                void difC4(Float inout[], size_t float_len)
                {
                    if constexpr (std::is_same_v<Float, double>)
                    {
                        if (float_len <= FFT_C4_MIN)
                        {
                            packC4(inout, float_len); // C4 -> RRII, 回到原路径
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
                        for (size_t k = fft_len / 16; k > 0; k--, it += 8, tp1 += 8, tp3 += 8)
                        {
                            HINT_PREFETCH(tp1 + 16, 0, 1);
                            HINT_PREFETCH(tp3 + 16, 0, 1);
                            C4d c0 = c4load(it), c1 = c4load(it + s1);
                            C4d c2 = c4load(it + s2), c3 = c4load(it + s3);
                            difSplitC4(c0, c1, c2, c3);
                            c4store(it, c0);
                            c4store(it + s1, c1);
                            c4store(it + s2, c4mul(c2, c4load(tp1)));
                            c4store(it + s3, c4mul(c3, c4load(tp3)));
                        }
                        const size_t stride = float_len / 4;
                        difC4(inout, stride * 2);
                        difC4(inout + stride * 2, stride);
                        difC4(inout + stride * 3, stride);
                    }
                }
                void iditC4(Float inout[], size_t float_len)
                {
                    if constexpr (std::is_same_v<Float, double>)
                    {
                        if (float_len <= FFT_C4_MIN)
                        {
                            idit<false>(inout, float_len);
                            packC4(inout, float_len); // RRII -> C4, 交回上层
                            return;
                        }
                        const size_t stride = float_len / 4;
                        iditC4(inout, stride * 2);
                        iditC4(inout + stride * 2, stride);
                        iditC4(inout + stride * 3, stride);
                        const size_t fft_len = float_len / 2;
                        const size_t s1 = float_len / 4, s2 = s1 * 2, s3 = s1 * 3;
                        auto tp1 = reinterpret_cast<const double *>(
                            __builtin_assume_aligned(table1.getBegin(fft_len), 32));
                        auto tp3 = reinterpret_cast<const double *>(
                            __builtin_assume_aligned(table3.getBegin(fft_len), 32));
                        auto it = reinterpret_cast<double *>(__builtin_assume_aligned(inout, 32));
                        for (size_t k = fft_len / 16; k > 0; k--, it += 8, tp1 += 8, tp3 += 8)
                        {
                            HINT_PREFETCH(tp1 + 16, 0, 1);
                            HINT_PREFETCH(tp3 + 16, 0, 1);
                            C4d c0 = c4load(it), c1 = c4load(it + s1);
                            C4d c2 = c4mulConj(c4load(it + s2), c4load(tp1));
                            C4d c3 = c4mulConj(c4load(it + s3), c4load(tp3));
                            iditSplitC4(c0, c1, c2, c3);
                            c4store(it, c0);
                            c4store(it + s1, c1);
                            c4store(it + s2, c2);
                            c4store(it + s3, c3);
                        }
                    }
                }

"""
assert s.count(ANCHOR6) == 1, "anchor6"
s = s.replace(ANCHOR6, C4_BODY + ANCHOR6)

io.open(DST, "w", encoding="utf-8", newline="").write(s)
print("D21 written, %d -> %d bytes (+%d)" % (orig_len, len(s), len(s) - orig_len))
