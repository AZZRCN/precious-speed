# -*- coding: utf-8 -*-
"""
D25 = D24 + radix-3 顶层 (dif3Stage/idit3Stage) 的 C4 化 + 与 difC4<0>/iditC4<0> 直连。

动机 (callgrind, lri_03):
  dif3Stage 14.0M (4.94%) + idit3Stage 8.9M (3.15%) 仍是 Complex2 (RRII, real/imag
  各占半个 ymm), 每迭代只处理 2 个复数, .mul() 跨 lane。
  另外 difC4<1>/iditC4<1> (6.6M / 5.6M) 的边界格式转换 c4loadC2 每迭代 8 条 perm2f128,
  只因为上游 dif3Stage 输出的是 C2/RRII。

做法:
  1) 新增 dif3StageC4 / idit3StageC4: 一次吃 4 个复数, 3 点 DFT 全部同 lane 全宽,
     输出/输入直接用 C4 格式。
  2) rdif: dif3StageC4 -> difC4<0> x3 (零 shuffle 的 c4load)
     ridit: iditC4<0> x3 -> idit3StageC4
  3) 保留原 C2 路径作 fallback (非 double / m%4 / blk<=FFT_C4_MIN)。
"""
import sys, os, io

SRC = r"D:\precious_speed\best\div_D24.cpp"
DST = r"D:\precious_speed\best\div_D25.cpp"

s = io.open(SRC, "r", encoding="utf-8", newline="").read()
orig_len = len(s)

# ---------------------------------------------------------------- 1) 插入 C4 版 radix-3 级
ANCHOR_INS = """            template <typename Float>
            inline void real_dot_binrev3(Float in_out[], const Float in[], size_t float_len)"""
assert s.count(ANCHOR_INS) == 1, "anchor real_dot_binrev3 not unique"

NEW_FUNCS = r"""            // ================= D25: radix-3 顶层的 C4 版 =================
            // 原 dif3Stage/idit3Stage 用 Complex2 (RRII): real/imag 各占半个 ymm, 每迭代
            // 只处理 2 个复数, 且 .mul()/.permute() 都在 128-bit 分区间跨 lane。
            // C4 版一次吃 4 个复数, 3 点 DFT 的全部加减/缩放变成同 lane 全宽运算; 更关键的是
            // 它的输出直接就是 C4 布局 -> 下游可以走 difC4<0> (零 shuffle 的 c4load), 省掉
            // 原 difC4<1> 每迭代 4 次 c4loadC2 = 8 条 perm2f128。idit 方向对称。
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
                for (size_t c = m / 4; c > 0; c--, b0 += 8, b1 += 8, b2 += 8, p1 += 8, p2 += 8)
                {
                    HINT_PREFETCH(p1 + 16, 0, 1);
                    HINT_PREFETCH(p2 + 16, 0, 1);
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
                for (size_t c = m / 4; c > 0; c--, b0 += 8, b1 += 8, b2 += 8, p1 += 8, p2 += 8)
                {
                    HINT_PREFETCH(p1 + 16, 0, 1);
                    HINT_PREFETCH(p2 + 16, 0, 1);
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

"""
s = s.replace(ANCHOR_INS, NEW_FUNCS + ANCHOR_INS, 1)

# ---------------------------------------------------------------- 2) rdif 直连
OLD_RDIF = """                const size_t m = float_len / 6, blk = m * 2;
                fft.expand(blk);
                dif3Stage<true>(p, m);
                fft.template dif<false>(p, blk);
                fft.template dif<false>(p + blk, blk);
                fft.template dif<false>(p + blk * 2, blk);"""
assert s.count(OLD_RDIF) == 1, "rdif anchor not unique"
NEW_RDIF = """                const size_t m = float_len / 6, blk = m * 2;
                fft.expand(blk);
                if constexpr (std::is_same_v<Float, double>)
                {
                    // D25: radix-3 级直接吐 C4, 三个子块用 IN_MODE=0 (零 shuffle)
                    if (blk > FFT_C4_MIN && (m & 3) == 0)
                    {
                        dif3StageC4<true>(reinterpret_cast<double *>(p), m);
                        fft.template difC4<0>(p, blk);
                        fft.template difC4<0>(p + blk, blk);
                        fft.template difC4<0>(p + blk * 2, blk);
                        return;
                    }
                }
                dif3Stage<true>(p, m);
                fft.template dif<false>(p, blk);
                fft.template dif<false>(p + blk, blk);
                fft.template dif<false>(p + blk * 2, blk);"""
s = s.replace(OLD_RDIF, NEW_RDIF, 1)

# ---------------------------------------------------------------- 3) ridit 直连
OLD_RIDIT = """                const size_t m = float_len / 6, blk = m * 2;
                fft.expand(blk);
                fft.template idit<false>(p, blk);
                fft.template idit<false>(p + blk, blk);
                fft.template idit<false>(p + blk * 2, blk);
                idit3Stage<true>(p, m);"""
assert s.count(OLD_RIDIT) == 1, "ridit anchor not unique"
NEW_RIDIT = """                const size_t m = float_len / 6, blk = m * 2;
                fft.expand(blk);
                if constexpr (std::is_same_v<Float, double>)
                {
                    if (blk > FFT_C4_MIN && (m & 3) == 0)
                    {
                        fft.template iditC4<0>(p, blk);
                        fft.template iditC4<0>(p + blk, blk);
                        fft.template iditC4<0>(p + blk * 2, blk);
                        idit3StageC4<true>(reinterpret_cast<double *>(p), m);
                        return;
                    }
                }
                fft.template idit<false>(p, blk);
                fft.template idit<false>(p + blk, blk);
                fft.template idit<false>(p + blk * 2, blk);
                idit3Stage<true>(p, m);"""
s = s.replace(OLD_RIDIT, NEW_RIDIT, 1)

io.open(DST, "w", encoding="utf-8", newline="").write(s)
print("D24 %d -> D25 %d bytes (+%d)" % (orig_len, len(s), len(s) - orig_len))
