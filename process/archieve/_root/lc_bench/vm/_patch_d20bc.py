#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""D20 回归根因: GCC 把 carryNormalize 编成独立函数(栈重对齐+寄存器溢出) -> +1.1%。
   D20b = D20 + [[gnu::always_inline]]           (保守: 保留 round(v+0.5) 语义)
   D20c = D20b + 纯 magic rint (去掉 vroundpd 与 +0.5)
          magic-2^52 加法本身就是 round-to-nearest, trunc(v+0.5) 与 rint(v) 仅在
          v 精确落在 .5 时不同 —— FFT 输出离整数的误差 << 0.5, 该情形不可达。
          省下: 每 4 limb 一条 vroundpd + 一条 vaddpd。
"""
import io, os, re

SRC = r"D:\precious_speed\best\div_D20.cpp"
OUT_B = r"D:\precious_speed\best\div_D20b.cpp"
OUT_C = r"D:\precious_speed\best\div_D20c.cpp"

s = io.open(SRC, encoding="utf-8").read()

OLD_SIG = "        static uint64_t carryNormalize(const double *v, size_t n, Limb *out)"
NEW_SIG = ("        [[gnu::always_inline]] static inline uint64_t\n"
           "        carryNormalize(const double *v, size_t n, Limb *out)")
assert s.count(OLD_SIG) == 1, "signature not unique"
b = s.replace(OLD_SIG, NEW_SIG)
io.open(OUT_B, "w", encoding="utf-8", newline="\n").write(b)
print("D20b written, %d bytes" % len(b))

OLD_BODY = """                __m256d x0 = _mm256_max_pd(
                    _mm256_round_pd(_mm256_add_pd(_mm256_loadu_pd(v + i), half),
                                    _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC), zero);
                __m256d x1 = _mm256_max_pd(
                    _mm256_round_pd(_mm256_add_pd(_mm256_loadu_pd(v + i + 4), half),
                                    _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC), zero);"""
NEW_BODY = """                // D20c: (x + 2^52) 的低 52 位尾数 = rint(x)。magic 加法自带
                // round-to-nearest, 故 vroundpd 与 +0.5 全部省掉。
                __m256d x0 = _mm256_max_pd(_mm256_loadu_pd(v + i), zero);
                __m256d x1 = _mm256_max_pd(_mm256_loadu_pd(v + i + 4), zero);"""
assert b.count(OLD_BODY) == 1, "body not unique"
c = b.replace(OLD_BODY, NEW_BODY)
c = c.replace("            const __m256d half = _mm256_set1_pd(0.5);\n",
              "            (void)0; // D20c: half 不再需要\n", 1)
io.open(OUT_C, "w", encoding="utf-8", newline="\n").write(c)
print("D20c written, %d bytes" % len(c))
