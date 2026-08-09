#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""D23 -> D24: 把 real-FFT 点乘阶段 (dot_rfftX2) 从 128-bit C2 升到 256-bit C4。

背景 (D22 lri_03 画像):
  dot_rfftX2        44.72M (13.66%)  <- 最大单函数, 反汇编 118 insn/call,
                                        其中 61 条是 blend/perm/unpck/insert 搬运,
                                        还有 8 条标量 vsubsd。
  real_dot_binrev3  21.71M ( 6.63%)  <- 驱动循环 + BinRevTable::iterate()

做法:
  1) dot_rfftX4: 一次吃 4 组共轭对 (原来 2 组)。所有算术是 re/im 的逐元素运算,
     C4 布局下全部同 lane 全宽, shuffle 只剩边界 load/store 的 perm2f128 +
     镜像端的 permute4x64(0x1B)。预估 ~82 insn / 4 对 vs 现在 236 / 4 对。
  2) BinRevTableC2HP::iterateV(): 直接把 table[pop] 这 4 个连续 double 当作
     RRII 用 vmovupd 取出 (它本来就在内存里, 不产生 spill), 两次合成一个 C4d
     只要 2 条 vperm2f128。iterate 的推进逻辑一字不改 -> 旋转因子序列完全一致。
  3) real_dot_binrev2 / real_dot_binrev3 的三处层循环改成成对步进 (fwd += 8,
     bwd -= 8)。C2 版第 g 次用 T_g 配 (it0, it1), 第 g+1 次用 T_{g+1} 配
     (it0+4, it1-4); 合成 C4 后正向就是 it0[0..7], 反向是 it1c4[0..7] 整体
     复数逆序 —— 与 C2 两次调用逐元素等价。
"""
import io
import os

PS = r"D:\precious_speed"
SRC = os.path.join(PS, "best", "div_D23.cpp")
DST = os.path.join(PS, "best", "div_D24.cpp")

s = io.open(SRC, encoding="utf-8", newline="").read()
n0 = len(s)

# ---------------------------------------------------------------- 1. iterateV
anchor = """                C2 iterate()
                {
                    C2 res = table[pop], unitx;"""
assert s.count(anchor) == 1, "anchor iterate"
s = s.replace(anchor, """                // D24: 返回 table[pop] 的原始 RRII 向量 (它本来就在内存里, 直接 vmovupd,
                // 不产生任何 spill)。推进逻辑与 iterate() 完全一致。
                __m256d iterateV()
                {
                    static_assert(std::is_same_v<Float, double>, "iterateV: double only");
                    const __m256d res = _mm256_loadu_pd(reinterpret_cast<const double *>(&table[pop]));
                    C2 unitx;
                    index++;
                    int zero = hint_ctz(index);
                    auto fp = reinterpret_cast<Float *>(&units[zero + 1]);
                    unitx.real.set1(fp[0]);
                    unitx.imag.set1(fp[1]);
                    pop -= zero;
                    table[pop + 1] = table[pop].mul(unitx);
                    pop++;
                    return res;
                }
                C2 iterate()
                {
                    C2 res = table[pop], unitx;""")

# ------------------------------------------------------------- 2. dot_rfftX4
anchor = """                c0.store(inout0), c1.reverse().store(inout1);
            }
            """
assert s.count(anchor) == 1, "anchor after dot_rfftX2"
s = s.replace(anchor, """                c0.store(inout0), c1.reverse().store(inout1);
            }

            // ================= D24: dot_rfftX2 的 C4 (256-bit) 版 =================
            // 4 个复数整体逆序 = 每个分量一条 vpermpd(0x1B)。
            HINT_AI C4d c4rev(const C4d &c)
            {
                return C4d{_mm256_permute4x64_pd(c.re, 0x1B), _mm256_permute4x64_pd(c.im, 0x1B)};
            }
            // C2/RRII 内存 -> 复数逆序的 C4 (镜像端专用)
            HINT_AI C4d c4loadC2Rev(const double *p)
            {
                return c4rev(c4loadC2(p));
            }
            // 两个 RRII 旋转因子向量 -> 一个 C4d, 只要 2 条 vperm2f128
            HINT_AI C4d c4twiddle(const __m256d &w0, const __m256d &w1)
            {
                return C4d{_mm256_permute2f128_pd(w0, w1, 0x20), _mm256_permute2f128_pd(w0, w1, 0x31)};
            }
            // 表达式结构逐条对齐 dot_rfftX2, 只是宽度 128 -> 256, 逐元素语义不变。
            HINT_AI void dot_rfftX4(double *inout0, double *inout1, const double *in0, const double *in1,
                                    const C4d &om, const __m256d &inv)
            {
                auto mul1 = [](const C4d &c0, const C4d &c1)
                {
                    return C4d{c0.im * c1.re + c0.re * c1.im, c0.im * c1.im - c0.re * c1.re};
                };
                auto mul2 = [](const C4d &c0, const C4d &c1)
                {
                    return C4d{c0.re * c1.im - c0.im * c1.re, c0.re * c1.re + c0.im * c1.im};
                };
                auto compute2 = [&om](const C4d &c0, const C4d &c1, C4d &out0, C4d &out1, auto Func)
                {
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

                    c0 = c4mul(x0, x2);
                    c0.re = c0.re * inv, c0.im = c0.im * inv;
                    c1 = c4mul(x1, x3);
                    c1.re = c1.re * inv, c1.im = c1.im * inv;
                    compute2(c0, c1, c0, c1, mul2);
                }
                c4storeC2(inout0, c0);
                c4storeC2(inout1, c4rev(c1));
            }
            """)

# --------------------------------------------------- 3a. real_dot_binrev2 循环
anchor = """                static thread_local BinRevTableC2HP<Float> table(31, 32);
                for (size_t begin = 16; begin < float_len; begin *= 2)
                {
                    table.reset(begin / 2);
                    auto it0 = in_out + begin, it1 = it0 + begin - 4;
                    auto it2 = in + begin, it3 = it2 + begin - 4;
                    for (; it0 < it1; it0 += 4, it1 -= 4, it2 += 4, it3 -= 4)
                    {
                        dot_rfftX2(it0, it1, it2, it3, table.iterate(), invx);
                    }
                }"""
assert s.count(anchor) == 1, "anchor binrev2 loop"
s = s.replace(anchor, """                static thread_local BinRevTableC2HP<Float> table(31, 32);
                for (size_t begin = 16; begin < float_len; begin *= 2)
                {
                    table.reset(begin / 2);
                    if constexpr (std::is_same_v<Float, double>)
                    {
                        if (begin >= 32) // begin/8 次 C2 迭代 -> begin/16 次 C4 迭代
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
                    {
                        dot_rfftX2(it0, it1, it2, it3, table.iterate(), invx);
                    }
                }""")

# ------------------------------------------- 3b. real_dot_binrev3 块0 层循环
anchor = """                for (size_t begin = 16; begin < blk; begin *= 2)
                {
                    table.reset(begin / 2);
                    auto it0 = in_out + begin, it1 = it0 + begin - 4;
                    auto it2 = in + begin, it3 = it2 + begin - 4;
                    for (; it0 < it1; it0 += 4, it1 -= 4, it2 += 4, it3 -= 4)
                    {
                        dot_rfftX2(it0, it1, it2, it3, table.iterate(), invx);
                    }
                }"""
assert s.count(anchor) == 1, "anchor binrev3 blk0 loop"
s = s.replace(anchor, """                for (size_t begin = 16; begin < blk; begin *= 2)
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
                    {
                        dot_rfftX2(it0, it1, it2, it3, table.iterate(), invx);
                    }
                }""")

# ---------------------------------------- 3c. real_dot_binrev3 块1/2 层循环
anchor = """                for (size_t begin = 16; begin < blk; begin *= 2)
                {
                    table.reset(begin / 2);
                    auto i0 = o1 + begin, i1 = o2 + blk - 4 - begin;
                    auto j0 = n1 + begin, j1 = n2 + blk - 4 - begin;
                    for (size_t u = begin / 4; u > 0; u--, i0 += 4, i1 -= 4, j0 += 4, j1 -= 4)
                    {
                        dot_rfftX2(i0, i1, j0, j1, table.iterate().mul(rot), invx);
                    }
                }"""
assert s.count(anchor) == 1, "anchor binrev3 blk12 loop"
s = s.replace(anchor, """                for (size_t begin = 16; begin < blk; begin *= 2)
                {
                    table.reset(begin / 2);
                    if constexpr (std::is_same_v<Float, double>)
                    {
                        // begin/4 次 C2 迭代 (begin >= 16 -> 至少 4 次) -> begin/8 次 C4
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
                    {
                        dot_rfftX2(i0, i1, j0, j1, table.iterate().mul(rot), invx);
                    }
                }""")

io.open(DST, "w", encoding="utf-8", newline="").write(s)
print("D24 written, %d -> %d bytes (+%d)" % (n0, len(s), len(s) - n0))
