# -*- coding: utf-8 -*-
"""
D26 patch: rdif/ridit 顶层 radix-3 级与三个子块 difC4/iditC4 的边界层融合。

动机 (callgrind 实证):
  dif3StageC4 / idit3StageC4 的 D1mr == DLmr —— L1 缺失 100% 穿透到 LL,
  说明这一趟完全没有复用, 是 compulsory miss, 分块救不了, 只能"减 pass 数"。

原流程 (rdif, float_len = 6m, blk = 2m):
    dif3StageC4<true>(p, m)      # 读 6m 写 6m —— 一整趟
    difC4<0>(p + i*blk, blk) x3  # 首层再把同一片 6m 读回来 (读 6m 写 6m)
  两趟之间的中间结果只被消费一次, 零复用价值。
融合后整片数组只读一次写一次。纯 pass reduction, 无机器常数 (非 LC 特调)。

索引对齐推导:
  difC4<0>(q, blk) 首层: fft_len = blk/2 = m, 迭代 m/16 次, it 从 q 起步长 8 doubles,
    读 it[0], it[s1], it[s2], it[s3], s1 = blk/4 = m/2; 旋转因子偏移 == it 偏移。
  dif3StageC4(p, m): 迭代 m/4 次, b0 步长 8, b1 = b0+2m, b2 = b0+4m;
    radix-3 表偏移 == b0 偏移。
  => 对 u = 0..3 在 x_u = j + u*s1 处做 4 次 radix-3, 恰好凑齐 3 个块各自
     首层蝶形所需的全部 4 个输入。三个块共用同一份 tp1/tp3 (都是 getBegin(m))。

【v2 关键修正】v1 用 `C4d &o0` 引用出参传结果, GCC 把这些局部量判为 address-taken,
  SRA 失效 -> 全部落栈再重载: lri_00 Dr +3.5% / lri_03 Dr +7.4%, 抵消了省下的一整趟。
  v2 改用宏直接写入具名局部变量, 保证纯 SSA 值, 不取地址。
  同时按 split-radix 的配对结构分两半 (先 u=0,2 落 c0 输出只留差; 再 u=1,3),
  把峰值活跃从 24 ymm 压到 ~18。

正确性 (读写顺序无冲突):
  本迭代写 {j, j+s1, j+s2, j+s3}; 对 j 的写发生在对 j+s1/j+s3 的读之前, 地址互不相同;
  对 j+s2 的写发生在对 j+s2 的读之后。下一迭代读 j+8+{0,s1,s2,s3}, 因 s1 = m/2 >= 16 > 8,
  与本迭代所写地址两两不等。
"""
import io, os, sys, shutil

BASE = r"D:\precious_speed\best\div_D25.cpp"
SRC = r"D:\precious_speed\best\div_D26.cpp"

shutil.copyfile(BASE, SRC)
s = io.open(SRC, encoding="utf-8", newline="").read()
orig_len = len(s)

# ---------------------------------------------------------------- 1) 表访问器
A_OLD = """                FFT() : table1(1), table3(3) {}
                void expand(size_t float_len)
                {
                    table1.expand(float_len / 2);
                    table3.expand(float_len / 2);
                }
"""
A_NEW = """                FFT() : table1(1), table3(3) {}
                void expand(size_t float_len)
                {
                    table1.expand(float_len / 2);
                    table3.expand(float_len / 2);
                }
                // D26: 供融合内核 (dif3FusedC4 / idit3FusedC4) 直接取旋转因子基址。
                // fft_len >= FFT_C4_MIN 的段已被 packSegs() 转成 C4 布局, 用 c4load 读。
                const Float *twBegin1(size_t fft_len) const { return table1.getBegin(fft_len); }
                const Float *twBegin3(size_t fft_len) const { return table3.getBegin(fft_len); }
"""
assert s.count(A_OLD) == 1, "A anchor %d" % s.count(A_OLD)
s = s.replace(A_OLD, A_NEW)

# ---------------------------------------------------------------- 2) 融合内核
B_ANCHOR = """            template <typename Float>
            inline void real_dot_binrev3(Float in_out[], const Float in[], size_t float_len)
"""
assert s.count(B_ANCHOR) == 1, "B anchor %d" % s.count(B_ANCHOR)

FUSED = r"""            // ============ D26: radix-3 顶层与 difC4/iditC4 边界层融合 ============
            // 见 lc_bench/_patch_d26.py 顶部注释的完整推导。要点:
            //   原来是「radix-3 扫一整趟 -> difC4 首层再扫一整趟」, 中间结果零复用;
            //   融合后 radix-3 的输出直接在寄存器里喂进首层蝶形, 省掉整整一趟读+写。
            // 前置条件 (由 rdif/ridit 保证): float_len = 6m 且 is_fft3 => m 是 2 的幂;
            //   blk = 2m > FFT_C4_MIN => m >= 32 => m % 16 == 0, 首层循环恰好整除。
            //
            // 必须用宏而不是 inline 函数 + 引用出参: 后者会让 GCC 把这些局部量视为
            // address-taken, SRA 失效, 12 个 C4d 全落栈再重载 (实测 Dr +3.5%~+7.4%,
            // 把省下的一整趟全吃回去)。宏保证是纯粹的具名局部值。

            // 前向 radix-3 (DIF, 带出口旋转因子): 读 RIRI, 结果留在 O0/O1/O2 三个 C4d 局部量
#define D26_RAD3_FWD(X, O0, O1, O2)                                                  \
    do                                                                               \
    {                                                                                \
        const double *b_ = base + (X);                                               \
        const C4d a0_ = c4loadRIRI(b_);                                              \
        const C4d a1_ = c4loadRIRI(b_ + blk);                                        \
        const C4d a2_ = c4loadRIRI(b_ + blk * 2);                                    \
        const __m256d sr_ = a1_.re + a2_.re, si_ = a1_.im + a2_.im;                  \
        const __m256d dr_ = a1_.re - a2_.re, di_ = a1_.im - a2_.im;                  \
        const __m256d hr_ = a0_.re - sr_ * vh, hi_ = a0_.im - si_ * vh;              \
        const __m256d gr_ = di_ * vns, gi_ = dr_ * vs;                               \
        (O0) = C4d{a0_.re + sr_, a0_.im + si_};                                      \
        (O1) = c4mul(C4d{hr_ - gr_, hi_ - gi_}, c4loadC2(q1 + (X)));                 \
        (O2) = c4mul(C4d{hr_ + gr_, hi_ + gi_}, c4loadC2(q2 + (X)));                 \
    } while (0)

            // 逆向 radix-3 (IDIT, 入口共轭旋转因子), 三个结果直接以 RIRI 落盘
#define D26_RAD3_INV_STORE(X, T0, R1, R2)                                            \
    do                                                                               \
    {                                                                                \
        const C4d x1_ = c4mulConj((R1), c4loadC2(q1 + (X)));                         \
        const C4d x2_ = c4mulConj((R2), c4loadC2(q2 + (X)));                         \
        const __m256d sr_ = x1_.re + x2_.re, si_ = x1_.im + x2_.im;                  \
        const __m256d dr_ = x1_.re - x2_.re, di_ = x1_.im - x2_.im;                  \
        const __m256d hr_ = (T0).re - sr_ * vh, hi_ = (T0).im - si_ * vh;            \
        const __m256d gr_ = di_ * vns, gi_ = dr_ * vs;                               \
        double *b_ = base + (X);                                                     \
        c4storeRIRI(b_, C4d{(T0).re + sr_, (T0).im + si_});                          \
        c4storeRIRI(b_ + blk, C4d{hr_ + gr_, hi_ + gi_});                            \
        c4storeRIRI(b_ + blk * 2, C4d{hr_ - gr_, hi_ - gi_});                        \
    } while (0)

            inline void dif3FusedC4(double *p, size_t m)
            {
                auto &fft = getSharedFFT<double>();
                const size_t blk = m * 2;
                const size_t s1 = blk / 4, s2 = s1 * 2, s3 = s1 * 3; // s1 == m/2
                const __m256d vh = _mm256_set1_pd(0.5);
                const __m256d vs = _mm256_set1_pd(double(SQRT3_DIV2));
                const __m256d vns = _mm256_set1_pd(-double(SQRT3_DIV2));
                auto &t1 = getTable3a<double>();
                auto &t2 = getTable3b<double>();
                t1.expand(m), t2.expand(m);
                auto q1 = reinterpret_cast<const double *>(__builtin_assume_aligned(t1.getBegin(m), 32));
                auto q2 = reinterpret_cast<const double *>(__builtin_assume_aligned(t2.getBegin(m), 32));
                auto tp1 = reinterpret_cast<const double *>(__builtin_assume_aligned(fft.twBegin1(m), 32));
                auto tp3 = reinterpret_cast<const double *>(__builtin_assume_aligned(fft.twBegin3(m), 32));
                double *base = reinterpret_cast<double *>(__builtin_assume_aligned(p, 32));
                for (size_t j = 0; j < s1; j += 8)
                {
                    HINT_PREFETCH(tp1 + j + 16, 0, 1);
                    HINT_PREFETCH(tp3 + j + 16, 0, 1);
                    // ---- 前半: u = 0, 2 -> split-radix 的 c0, c2 ----
                    C4d A0, A1, A2, B0, B1, B2;
                    D26_RAD3_FWD(j, A0, A1, A2);
                    D26_RAD3_FWD(j + s2, B0, B1, B2);
                    // c0 输出 = c0 + c2 立刻落盘, 只把 c0 - c2 留在寄存器
                    const C4d U0{A0.re - B0.re, A0.im - B0.im};
                    c4store(base + j, C4d{A0.re + B0.re, A0.im + B0.im});
                    const C4d U1{A1.re - B1.re, A1.im - B1.im};
                    c4store(base + blk + j, C4d{A1.re + B1.re, A1.im + B1.im});
                    const C4d U2{A2.re - B2.re, A2.im - B2.im};
                    c4store(base + blk * 2 + j, C4d{A2.re + B2.re, A2.im + B2.im});
                    // ---- 后半: u = 1, 3 -> split-radix 的 c1, c3 ----
                    C4d C0, C1, C2v, D0, D1, D2;
                    D26_RAD3_FWD(j + s1, C0, C1, C2v);
                    D26_RAD3_FWD(j + s3, D0, D1, D2);
                    const C4d w1 = c4load(tp1 + j), w3 = c4load(tp3 + j);
                    {
                        double *o = base + j;
                        const __m256d br = C0.re - D0.re, bi = C0.im - D0.im;
                        c4store(o + s1, C4d{C0.re + D0.re, C0.im + D0.im});
                        c4store(o + s2, c4mul(C4d{U0.re + bi, U0.im - br}, w1));
                        c4store(o + s3, c4mul(C4d{U0.re - bi, U0.im + br}, w3));
                    }
                    {
                        double *o = base + blk + j;
                        const __m256d br = C1.re - D1.re, bi = C1.im - D1.im;
                        c4store(o + s1, C4d{C1.re + D1.re, C1.im + D1.im});
                        c4store(o + s2, c4mul(C4d{U1.re + bi, U1.im - br}, w1));
                        c4store(o + s3, c4mul(C4d{U1.re - bi, U1.im + br}, w3));
                    }
                    {
                        double *o = base + blk * 2 + j;
                        const __m256d br = C2v.re - D2.re, bi = C2v.im - D2.im;
                        c4store(o + s1, C4d{C2v.re + D2.re, C2v.im + D2.im});
                        c4store(o + s2, c4mul(C4d{U2.re + bi, U2.im - br}, w1));
                        c4store(o + s3, c4mul(C4d{U2.re - bi, U2.im + br}, w3));
                    }
                }
                // 首层已就地完成, 剩下就是 difC4<0> 的三段递归 (对每个子块)
                const size_t stride = blk / 4;
                for (size_t b = 0; b < 3; b++)
                {
                    double *q = base + b * blk;
                    fft.template difC4<0>(q, stride * 2);
                    fft.template difC4<0>(q + stride * 2, stride);
                    fft.template difC4<0>(q + stride * 3, stride);
                }
            }

            inline void idit3FusedC4(double *p, size_t m)
            {
                auto &fft = getSharedFFT<double>();
                const size_t blk = m * 2;
                const size_t s1 = blk / 4, s2 = s1 * 2, s3 = s1 * 3;
                const __m256d vh = _mm256_set1_pd(0.5);
                const __m256d vs = _mm256_set1_pd(double(SQRT3_DIV2));
                const __m256d vns = _mm256_set1_pd(-double(SQRT3_DIV2));
                auto &t1 = getTable3a<double>();
                auto &t2 = getTable3b<double>();
                t1.expand(m), t2.expand(m);
                auto q1 = reinterpret_cast<const double *>(__builtin_assume_aligned(t1.getBegin(m), 32));
                auto q2 = reinterpret_cast<const double *>(__builtin_assume_aligned(t2.getBegin(m), 32));
                auto tp1 = reinterpret_cast<const double *>(__builtin_assume_aligned(fft.twBegin1(m), 32));
                auto tp3 = reinterpret_cast<const double *>(__builtin_assume_aligned(fft.twBegin3(m), 32));
                double *base = reinterpret_cast<double *>(__builtin_assume_aligned(p, 32));
                // 先做 iditC4<0> 的三段递归 (它的主循环在递归之后, 融合的正是那最后一层)
                const size_t stride = blk / 4;
                for (size_t b = 0; b < 3; b++)
                {
                    double *q = base + b * blk;
                    fft.template iditC4<0>(q, stride * 2);
                    fft.template iditC4<0>(q + stride * 2, stride);
                    fft.template iditC4<0>(q + stride * 3, stride);
                }
                for (size_t j = 0; j < s1; j += 8)
                {
                    HINT_PREFETCH(tp1 + j + 16, 0, 1);
                    HINT_PREFETCH(tp3 + j + 16, 0, 1);
                    const C4d w1 = c4load(tp1 + j), w3 = c4load(tp3 + j);
                    // 三个子块各自做末层 iditSplit -> 12 个 C4d, 再按 4 个 x 位置做逆 radix-3
                    C4d k00, k01, k02, k03, k10, k11, k12, k13, k20, k21, k22, k23;
#define D26_IDIT_LAST(OFF, O0, O1, O2, O3)                                           \
    do                                                                               \
    {                                                                                \
        double *o_ = base + (OFF) + j;                                               \
        C4d c0_ = c4load(o_), c1_ = c4load(o_ + s1);                                 \
        C4d c2_ = c4mulConj(c4load(o_ + s2), w1);                                    \
        C4d c3_ = c4mulConj(c4load(o_ + s3), w3);                                    \
        iditSplitC4(c0_, c1_, c2_, c3_);                                             \
        (O0) = c0_, (O1) = c1_, (O2) = c2_, (O3) = c3_;                              \
    } while (0)
                    D26_IDIT_LAST(0, k00, k01, k02, k03);
                    D26_IDIT_LAST(blk, k10, k11, k12, k13);
                    D26_IDIT_LAST(blk * 2, k20, k21, k22, k23);
#undef D26_IDIT_LAST
                    D26_RAD3_INV_STORE(j, k00, k10, k20);
                    D26_RAD3_INV_STORE(j + s1, k01, k11, k21);
                    D26_RAD3_INV_STORE(j + s2, k02, k12, k22);
                    D26_RAD3_INV_STORE(j + s3, k03, k13, k23);
                }
            }
#undef D26_RAD3_FWD
#undef D26_RAD3_INV_STORE

"""
s = s.replace(B_ANCHOR, FUSED + B_ANCHOR)

# ---------------------------------------------------------------- 3) rdif 接线
C_OLD = """                    // D25: radix-3 级直接吐 C4, 三个子块用 IN_MODE=0 (零 shuffle)
                    if (blk > FFT_C4_MIN && (m & 3) == 0)
                    {
                        dif3StageC4<true>(reinterpret_cast<double *>(p), m);
                        fft.template difC4<0>(p, blk);
                        fft.template difC4<0>(p + blk, blk);
                        fft.template difC4<0>(p + blk * 2, blk);
                        return;
                    }
"""
C_NEW = """                    // D25: radix-3 级直接吐 C4, 三个子块用 IN_MODE=0 (零 shuffle)
                    if (blk > FFT_C4_MIN && (m & 3) == 0)
                    {
#ifndef NO_D26_FUSE
                        // D26: 与三个子块的 difC4 首层融合, 省掉一整趟 6m doubles 读+写
                        dif3FusedC4(reinterpret_cast<double *>(p), m);
#else
                        dif3StageC4<true>(reinterpret_cast<double *>(p), m);
                        fft.template difC4<0>(p, blk);
                        fft.template difC4<0>(p + blk, blk);
                        fft.template difC4<0>(p + blk * 2, blk);
#endif
                        return;
                    }
"""
assert s.count(C_OLD) == 1, "C anchor %d" % s.count(C_OLD)
s = s.replace(C_OLD, C_NEW)

D_OLD = """                    if (blk > FFT_C4_MIN && (m & 3) == 0)
                    {
                        fft.template iditC4<0>(p, blk);
                        fft.template iditC4<0>(p + blk, blk);
                        fft.template iditC4<0>(p + blk * 2, blk);
                        idit3StageC4<true>(reinterpret_cast<double *>(p), m);
                        return;
                    }
"""
D_NEW = """                    if (blk > FFT_C4_MIN && (m & 3) == 0)
                    {
#ifndef NO_D26_FUSE
                        idit3FusedC4(reinterpret_cast<double *>(p), m);
#else
                        fft.template iditC4<0>(p, blk);
                        fft.template iditC4<0>(p + blk, blk);
                        fft.template iditC4<0>(p + blk * 2, blk);
                        idit3StageC4<true>(reinterpret_cast<double *>(p), m);
#endif
                        return;
                    }
"""
assert s.count(D_OLD) == 1, "D anchor %d" % s.count(D_OLD)
s = s.replace(D_OLD, D_NEW)

io.open(SRC, "w", encoding="utf-8", newline="").write(s)
print("patched: %d -> %d bytes (+%d)" % (orig_len, len(s), len(s) - orig_len))
