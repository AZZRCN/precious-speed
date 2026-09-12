# 生成 D46 = D45 + radix-5 mixed-radix (Cooley-Tukey, 照抄 radix-3 结构)
#
# 依据 (CEILHIST 实测, r_nearly_zero_01 overhead=42.89%):
#   lin  need=129..151 -> used=192  (x1000+)   radix-5 给 160  (-16.7%)
#   mn   need=66..70   -> used=128  (x500+)    radix-5 给 80   (-37.5%)
#   => 5*2^k 正打两簇最高点; 7*2^k 帮不上 (112<129, 224>192)。
#
# 三块必须成套改, 少一块就是错答案:
#   (a) dif5Stage/idit5Stage  : radix-5 蝶形 (5 点 DFT + 4 张 twiddle 表)
#   (b) real_dot_binrev5      : 实数打包频域点乘的共轭配对
#                               块 j <-> 块 (5-j), pos p <-> m-1-p, twiddle = 块0 序列 * w_FL^j
#   (c) fft_ceil / is_fft5    : 档位与派发
import sys

PS = r'D:\precious_speed'
src = open(f'{PS}/best/div_D45.cpp', encoding='utf-8', newline='').read()
assert 'dif3Stage' in src and 'real_dot_binrev3' in src, 'not D45 source'
CRLF = '\r\n' in src
src = src.replace('\r\n', '\n')
N0 = len(src)


def sub(old, new, tag):
    global src
    assert old in src, f'anchor missing: {tag}'
    assert src.count(old) == 1, f'anchor not unique: {tag}'
    src = src.replace(old, new, 1)


# ============ 1) FFTTable5<Float,FACTOR> x4 + getters (插在 getTable3b 之后) ============
T5 = r'''
            // ---- D46: radix-5 twiddle. e[n] = w_{5M}^{FACTOR*n}, 布局同 FFTTable3 ----
            template <typename Float, int FACTOR>
            struct FFTTable5
            {
                using C2 = Complex2<Float>;
                FFTTable5() : cur_m(2), table(8)
                {
                    Float theta = Float(-HINT_2PI) * FACTOR / Float(10); // 5*M, M=2
                    table[4] = 1, table[5] = std::cos(theta);
                    table[6] = 0, table[7] = std::sin(theta);
                }
                void expand(size_t m_need)
                {
                    if (m_need <= cur_m)
                    {
                        return;
                    }
                    table.resize(m_need * 4);
                    for (size_t m = cur_m * 2; m <= m_need; m *= 2)
                    {
                        Float *it = &table[m * 2];
                        const Float *last = &table[m];
                        Float theta = Float(-HINT_2PI) * FACTOR / Float(5 * m);
                        C2 unit(Float(std::cos(theta)), Float(std::sin(theta)));
                        for (size_t c = 0; c < m / 4; c++)
                        {
                            C2 o0, o1;
                            o0.load(last + 4 * c);
                            o1 = o0.mul(unit);
                            std::swap(o0.real.x1, o1.real.x0);
                            std::swap(o0.imag.x1, o1.imag.x0);
                            o0.store(it + 8 * c);
                            o1.store(it + 8 * c + 4);
                        }
                    }
                    cur_m = m_need;
                }
                const Float *getBegin(size_t m) const { return &table[m * 2]; }
                size_t cur_m;
                AlignedVec32<Float> table;
            };
            template <typename Float>
            inline FFTTable5<Float, 1> &getTable5a()
            {
                static FFTTable5<Float, 1> t;
                return t;
            }
            template <typename Float>
            inline FFTTable5<Float, 2> &getTable5b()
            {
                static FFTTable5<Float, 2> t;
                return t;
            }
            template <typename Float>
            inline FFTTable5<Float, 3> &getTable5c()
            {
                static FFTTable5<Float, 3> t;
                return t;
            }
            template <typename Float>
            inline FFTTable5<Float, 4> &getTable5d()
            {
                static FFTTable5<Float, 4> t;
                return t;
            }'''
A1 = '''            template <typename Float>
            inline FFTTable3<Float, 2> &getTable3b()
            {
                static FFTTable3<Float, 2> t;
                return t;
            }'''
sub(A1, A1 + '\n' + T5, 'getTable3b')

# ============ 2) dif5Stage / idit5Stage (插在 real_dot_binrev3 之前) ============
S5 = r'''
            // ================= D46: radix-5 顶层蝶形 =================
            // W5 = e^{-2pi i/5};  c1=Re W5, s1=-Im W5, c2=Re W5^2, s2=-Im W5^2
            //   A0 = a0+a1+a2+a3+a4
            //   t1=a1+a4, t2=a2+a3, t3=a1-a4, t4=a2-a3
            //   u1 = a0 + t1*c1 + t2*c2 ; v1 = t3*s1 + t4*s2
            //   u2 = a0 + t1*c2 + t2*c1 ; v2 = t3*s2 - t4*s1
            //   A1 = u1 - i*v1, A4 = u1 + i*v1, A2 = u2 - i*v2, A3 = u2 + i*v2
            // 之后 A_j *= w_{5M}^{j*n} (第 j 张表), 五块各自走 2 幂 dif。
            constexpr Float64 R5_C1 = 0.309016994374947424102293417183;
            constexpr Float64 R5_S1 = 0.951056516295153572116439333379;
            constexpr Float64 R5_C2 = -0.809016994374947424102293417183;
            constexpr Float64 R5_S2 = 0.587785252292473129078558064009;

            template <bool RIRI_IN, typename Float>
            inline void dif5Stage(Float *inout, size_t m)
            {
                using C2 = Complex2<Float>;
                const Float c1 = Float(R5_C1), s1 = Float(R5_S1);
                const Float c2 = Float(R5_C2), s2 = Float(R5_S2);
                auto &t1t = getTable5a<Float>();
                auto &t2t = getTable5b<Float>();
                auto &t3t = getTable5c<Float>();
                auto &t4t = getTable5d<Float>();
                t1t.expand(m), t2t.expand(m), t3t.expand(m), t4t.expand(m);
                auto p1 = reinterpret_cast<const C2 *>(t1t.getBegin(m));
                auto p2 = reinterpret_cast<const C2 *>(t2t.getBegin(m));
                auto p3 = reinterpret_cast<const C2 *>(t3t.getBegin(m));
                auto p4 = reinterpret_cast<const C2 *>(t4t.getBegin(m));
                Float *b0 = inout, *b1 = inout + m * 2, *b2 = inout + m * 4;
                Float *b3 = inout + m * 6, *b4 = inout + m * 8;
                for (size_t c = m / 2; c > 0; c--, b0 += 4, b1 += 4, b2 += 4, b3 += 4, b4 += 4,
                            p1++, p2++, p3++, p4++)
                {
                    C2 a0, a1, a2, a3, a4;
                    a0.load(b0), a1.load(b1), a2.load(b2), a3.load(b3), a4.load(b4);
                    if (RIRI_IN)
                    {
                        a0.permute(), a1.permute(), a2.permute(), a3.permute(), a4.permute();
                    }
                    C2 t1 = a1 + a4, t2 = a2 + a3, t3 = a1 - a4, t4 = a2 - a3;
                    C2 u1(a0.real + t1.real * c1 + t2.real * c2,
                          a0.imag + t1.imag * c1 + t2.imag * c2);
                    C2 u2(a0.real + t1.real * c2 + t2.real * c1,
                          a0.imag + t1.imag * c2 + t2.imag * c1);
                    C2 v1(t3.real * s1 + t4.real * s2, t3.imag * s1 + t4.imag * s2);
                    C2 v2(t3.real * s2 - t4.real * s1, t3.imag * s2 - t4.imag * s1);
                    C2 A0 = a0 + t1 + t2;
                    C2 A1(u1.real + v1.imag, u1.imag - v1.real);
                    C2 A4(u1.real - v1.imag, u1.imag + v1.real);
                    C2 A2(u2.real + v2.imag, u2.imag - v2.real);
                    C2 A3(u2.real - v2.imag, u2.imag + v2.real);
                    A0.store(b0);
                    A1.mul(p1[0]).store(b1);
                    A2.mul(p2[0]).store(b2);
                    A3.mul(p3[0]).store(b3);
                    A4.mul(p4[0]).store(b4);
                }
            }
            // 逆: 先解 twiddle (mulConj), 再乘 conj(W5) (不含 1/5, 与 idit 同约定)
            template <bool RIRI_OUT, typename Float>
            inline void idit5Stage(Float *inout, size_t m)
            {
                using C2 = Complex2<Float>;
                const Float c1 = Float(R5_C1), s1 = Float(R5_S1);
                const Float c2 = Float(R5_C2), s2 = Float(R5_S2);
                auto &t1t = getTable5a<Float>();
                auto &t2t = getTable5b<Float>();
                auto &t3t = getTable5c<Float>();
                auto &t4t = getTable5d<Float>();
                t1t.expand(m), t2t.expand(m), t3t.expand(m), t4t.expand(m);
                auto p1 = reinterpret_cast<const C2 *>(t1t.getBegin(m));
                auto p2 = reinterpret_cast<const C2 *>(t2t.getBegin(m));
                auto p3 = reinterpret_cast<const C2 *>(t3t.getBegin(m));
                auto p4 = reinterpret_cast<const C2 *>(t4t.getBegin(m));
                Float *b0 = inout, *b1 = inout + m * 2, *b2 = inout + m * 4;
                Float *b3 = inout + m * 6, *b4 = inout + m * 8;
                for (size_t c = m / 2; c > 0; c--, b0 += 4, b1 += 4, b2 += 4, b3 += 4, b4 += 4,
                            p1++, p2++, p3++, p4++)
                {
                    C2 y0, y1, y2, y3, y4;
                    y0.load(b0), y1.load(b1), y2.load(b2), y3.load(b3), y4.load(b4);
                    C2 x1 = y1.mulConj(p1[0]), x2 = y2.mulConj(p2[0]);
                    C2 x3 = y3.mulConj(p3[0]), x4 = y4.mulConj(p4[0]);
                    C2 t1 = x1 + x4, t2 = x2 + x3, t3 = x1 - x4, t4 = x2 - x3;
                    C2 u1(y0.real + t1.real * c1 + t2.real * c2,
                          y0.imag + t1.imag * c1 + t2.imag * c2);
                    C2 u2(y0.real + t1.real * c2 + t2.real * c1,
                          y0.imag + t1.imag * c2 + t2.imag * c1);
                    C2 v1(t3.real * s1 + t4.real * s2, t3.imag * s1 + t4.imag * s2);
                    C2 v2(t3.real * s2 - t4.real * s1, t3.imag * s2 - t4.imag * s1);
                    C2 a0 = y0 + t1 + t2;
                    C2 a1(u1.real - v1.imag, u1.imag + v1.real);
                    C2 a4(u1.real + v1.imag, u1.imag - v1.real);
                    C2 a2(u2.real - v2.imag, u2.imag + v2.real);
                    C2 a3(u2.real + v2.imag, u2.imag - v2.real);
                    if (RIRI_OUT)
                    {
                        a0.permute(), a1.permute(), a2.permute(), a3.permute(), a4.permute();
                    }
                    a0.store(b0), a1.store(b1), a2.store(b2), a3.store(b3), a4.store(b4);
                }
            }

            // D46: 跨块共轭配对点乘 (块 j 与块 R-j, 位置 p <-> m-1-p)
            //   twiddle = 与块0 完全相同的位反转序列 * 常数 rot = w_FL^j
            //   (radix-3 的块1<->块2 就是本函数 j=1 的特例; 那份代码保持原样不动)
            template <typename Float>
            inline void dot_cross_blocks(Float *o1, Float *o2, const Float *n1, const Float *n2,
                                         size_t blk, const Complex2<Float> &rot,
                                         const Float2<Float> &invx, Float inv4,
                                         BinRevTableC2HP<Float> &table)
            {
                using C2 = Complex2<Float>;
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
                        const __m256d invv = _mm256_set1_pd(inv4);
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
                    {
                        dot_rfftX2(i0, i1, j0, j1, table.iterate().mul(rot), invx);
                    }
                }
            }

            // D46: float_len = 5*2^k 的实数频域点乘
            //   N_c = float_len/2 = 5m 个复数, 块 j 位置 p 承载频点 f = 5p + j
            //   共轭伙伴 N_c-f = 5(m-1-p) + (5-j)  => 块0 自配对(同 2 幂), 块1<->4, 块2<->3
            //   twiddle w_FL^{5p+j} = w_{2m}^p * w_FL^j  => 复用块0 序列 * 常数
            template <typename Float>
            inline void real_dot_binrev5(Float in_out[], const Float in[], size_t float_len)
            {
                using F2 = Float2<Float>;
                using C2 = Complex2<Float>;
                const size_t m = float_len / 10, blk = m * 2;
                const Float inv = Float(1) / Float(float_len);
                const Float inv4 = Float(0.25) / Float(float_len);
                const F2 invx = F2::from1(inv4);
                static thread_local BinRevTableC2HP<Float> table(31, 32);
                // ---- 块0: 与 2 幂长度 blk 的实数解包完全同构 ----
                real_dot_binrev<2>(in_out, in, 16, inv);
                for (size_t begin = 16; begin < blk; begin *= 2)
                {
                    table.reset(begin / 2);
                    if constexpr (std::is_same_v<Float, double>)
                    {
                        if (begin >= 32)
                        {
                            const __m256d invv = _mm256_set1_pd(inv4);
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
                    {
                        dot_rfftX2(it0, it1, it2, it3, table.iterate(), invx);
                    }
                }
                // ---- 块1<->块4 (rot^1), 块2<->块3 (rot^2) ----
                const Float ang = Float(-HINT_2PI) / Float(float_len);
                const C2 rot1(Float(std::cos(ang)), Float(std::sin(ang)));
                const C2 rot2(Float(std::cos(2 * ang)), Float(std::sin(2 * ang)));
                dot_cross_blocks(in_out + blk, in_out + blk * 4, in + blk, in + blk * 4,
                                 blk, rot1, invx, inv4, table);
                dot_cross_blocks(in_out + blk * 2, in_out + blk * 3, in + blk * 2, in + blk * 3,
                                 blk, rot2, invx, inv4, table);
            }

'''
A2 = '''            template <typename Float>
            inline void real_dot_binrev3(Float in_out[], const Float in[], size_t float_len)'''
sub(A2, S5 + A2, 'real_dot_binrev3 head')

# ============ 3) fft_ceil: 加 5*2^k 档 ============
OLD_CEIL = '''        size_t p = int_ceil2(n);
#ifdef NO_FFT3
        return p; // A/B 开关: 退回纯 2 幂档位
#else
        size_t h = p / 4 * 3; // = 3*2^(k-2), 落在 (p/2, p)
        if (h >= FFT3_MIN && h >= n)
            return h;
        return p;
#endif'''
NEW_CEIL = '''        size_t p = int_ceil2(n);
#ifdef NO_FFT3
        return p; // A/B 开关: 退回纯 2 幂档位
#else
        size_t best = p;
        size_t h = p / 4 * 3; // = 3*2^(k-2), 落在 (p/2, p)
        if (h >= FFT3_MIN && h >= n)
            best = h;
#ifdef ENABLE_FFT5
        // D46: 5*2^(k-3) 落在 (p/2, 3p/4), 比 3*2^k 更细一档
        size_t h5 = p / 8 * 5;
        if (h5 >= FFT5_MIN && h5 >= n && h5 < best)
            best = h5;
#endif
        return best;
#endif'''
sub(OLD_CEIL, NEW_CEIL, 'fft_ceil')

# FFT5_MIN 必须在 fft_ceil 之前声明
sub('    constexpr size_t FFT3_MIN = 192;',
    '    constexpr size_t FFT3_MIN = 192;\n'
    '    // FFT5_MIN: radix-5 路径要求每块 M = float_len/10 >= 8 (blk=2M>=16, 实数解包最小单元)\n'
    '    constexpr size_t FFT5_MIN = 80;',
    'FFT3_MIN')

# ============ 4) is_fft5 + FFT5_MIN (紧跟 is_fft3) ============
OLD_F3 = '''    constexpr bool is_fft3(size_t float_len)
    {
        return (float_len % 3) == 0;
    }'''
NEW_F3 = '''    constexpr bool is_fft3(size_t float_len)
    {
        return (float_len % 3) == 0;
    }
    constexpr bool is_fft5(size_t float_len)
    {
#ifdef ENABLE_FFT5
        // 与 is_fft3 互斥 (15*2^k 不由 fft_ceil 产生, 这里只是防御)
        return (float_len % 5) == 0 && (float_len % 3) != 0 && float_len >= FFT5_MIN;
#else
        (void)float_len;
        return false;
#endif
    }'''
sub(OLD_F3, NEW_F3, 'is_fft3')

# ============ 5) rdif / ridit / rdot 派发 ============
OLD_RDIF = '''                if (!is_fft3(float_len))
                {
                    fft.expand(float_len);
                    fft.template dif<true>(p, float_len);
                    return;
                }
                const size_t m = float_len / 6, blk = m * 2;'''
NEW_RDIF = '''                if (is_fft5(float_len))
                {
                    const size_t m5 = float_len / 10, blk5 = m5 * 2;
                    fft.expand(blk5);
                    dif5Stage<true>(p, m5);
                    for (size_t i = 0; i < 5; i++)
                    {
                        fft.template dif<false>(p + blk5 * i, blk5);
                    }
                    return;
                }
                if (!is_fft3(float_len))
                {
                    fft.expand(float_len);
                    fft.template dif<true>(p, float_len);
                    return;
                }
                const size_t m = float_len / 6, blk = m * 2;'''
sub(OLD_RDIF, NEW_RDIF, 'rdif')

OLD_RIDIT = '''                if (!is_fft3(float_len))
                {
                    fft.expand(float_len);
                    fft.template idit<true>(p, float_len);
                    return;
                }
                const size_t m = float_len / 6, blk = m * 2;'''
NEW_RIDIT = '''                if (is_fft5(float_len))
                {
                    const size_t m5 = float_len / 10, blk5 = m5 * 2;
                    fft.expand(blk5);
                    for (size_t i = 0; i < 5; i++)
                    {
                        fft.template idit<false>(p + blk5 * i, blk5);
                    }
                    idit5Stage<true>(p, m5);
                    return;
                }
                if (!is_fft3(float_len))
                {
                    fft.expand(float_len);
                    fft.template idit<true>(p, float_len);
                    return;
                }
                const size_t m = float_len / 6, blk = m * 2;'''
sub(OLD_RIDIT, NEW_RIDIT, 'ridit')

OLD_RDOT = '''                if (!is_fft3(float_len))
                {
                    real_dot_binrev2(in_out, in, float_len);
                }
                else
                {
                    real_dot_binrev3(in_out, in, float_len);
                }'''
NEW_RDOT = '''                if (is_fft5(float_len))
                {
                    real_dot_binrev5(in_out, in, float_len);
                }
                else if (!is_fft3(float_len))
                {
                    real_dot_binrev2(in_out, in, float_len);
                }
                else
                {
                    real_dot_binrev3(in_out, in, float_len);
                }'''
sub(OLD_RDOT, NEW_RDOT, 'rdot')

# ============ 6) asserts ============
for var in ['float_len', 'm']:
    old = f'assert(is_2pow({var}) || is_fft3({var}));'
    new = f'assert(is_2pow({var}) || is_fft3({var}) || is_fft5({var}));'
    assert src.count(old) >= 1, f'assert anchor missing: {old}'
    src = src.replace(old, new)

# ============ 7) 打开开关 ============
sub('#define HINT_OP_DIV\n',
    '#define HINT_OP_DIV\n#define ENABLE_FFT5 1 // D46: 放开 5*2^k FFT 档位\n',
    'HINT_OP_DIV')

out = f'{PS}/best/div_D46.cpp'
open(out, 'w', encoding='utf-8', newline='').write(src.replace('\n', '\r\n') if CRLF else src)
print(f'[mk_d46] {out}  {N0} -> {len(src)} bytes (+{len(src)-N0})')
