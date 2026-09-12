#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""D20 = D19 + carryNormalize: FFT \u9006\u53d8\u6362\u540e\u7684\u8fdb\u4f4d\u5f52\u4e00\u5316\u5411\u91cf\u5316\u3002"""
import re
import sys

SRC = r"D:\precious_speed\best\div_D19.cpp"
DST = r"D:\precious_speed\best\div_D20.cpp"

s = open(SRC, encoding="utf-8", errors="surrogateescape").read()
orig = s

# ---------- 1) \u5728 divBASE \u540e\u63d2\u5165 carryNormalize ----------
ANCHOR = ("        static uint64_t divBASE(uint64_t s) "
          "{ return (uint64_t)((unsigned __int128)s * BARRETT_M >> 64); }\n")
assert s.count(ANCHOR) == 1, "divBASE anchor not unique"

HELPER = r'''
        // === D20: FFT \u9006\u53d8\u6362\u540e\u7684\u8fdb\u4f4d\u5f52\u4e00\u5316 (\u5411\u91cf\u5316 double->uint64) ===
        // \u753b\u50cf (lri_03): fftMulPre 20.46M Ir / fftMulModBm1Pre 12.29M Ir, 76% \u6807\u91cf\u3002
        // \u6bcf limb \u4e00\u6761 vaddsd + vcvttsd2si; \u800c\u4e14 uint64_t(double) \u8ba9 GCC \u63d2\u5165
        // vcomisd + jae \u7684 "\u662f\u5426 >= 2^63" \u7b26\u53f7\u4fdd\u62a4\u5206\u652f (\u5404 1.64M / 1.43M Ir)\u3002
        //
        // x = trunc(v + 0.5) \u662f\u7cbe\u786e\u6574\u6570 double \u4e14 0 <= x < 2^52 (Barrett \u6ce8\u91ca\u5df2\u754c\u5b9a
        // s_max ~ 2^51), \u4e8e\u662f (x + 2^52) \u7684 IEEE754 \u5c3e\u6570\u4f4e 52 \u4f4d\u5c31\u662f x \u7684\u4e8c\u8fdb\u5236\u8868\u793a
        // \u2014\u2014 \u4e00\u6761 vpsubq \u5373\u5b8c\u6210 4 \u8def\u8f6c\u6362, \u65e0\u4efb\u4f55 cvt \u6307\u4ee4\u4e0e\u5206\u652f\u3002
        // Barrett \u8fdb\u4f4d\u94fe\u672c\u8eab\u4e32\u884c, \u4fdd\u6301\u6807\u91cf 8 \u8def\u5c55\u5f00\u3002
        // vmaxpd 0: \u539f\u4ee3\u7801\u5bf9 v[i] < -0.5 \u662f UB (cvttsd2si \u5f97\u8d1f\u6570\u518d\u5f53 uint64),
        //           \u8fd9\u91cc\u5939\u5230 0 \u2014\u2014 \u5408\u6cd5\u8f93\u5165\u884c\u4e3a\u4e0d\u53d8, \u975e\u6cd5\u8f93\u5165\u4e0d\u518d\u70b8\u3002
        static uint64_t carryNormalize(const double *v, size_t n, Limb *out)
        {
            const __m256d half = _mm256_set1_pd(0.5);
            const __m256d zero = _mm256_setzero_pd();
            const __m256d magic = _mm256_set1_pd(4503599627370496.0); // 2^52
            const __m256i magicI = _mm256_castpd_si256(magic);
            alignas(32) uint64_t w[8];
            uint64_t carry = 0;
            size_t i = 0;
            for (; i + 7 < n; i += 8)
            {
                HINT_PREFETCH(v + i + 16, 0, 0);
                HINT_PREFETCH(v + i + 24, 0, 0);
                __m256d x0 = _mm256_max_pd(
                    _mm256_round_pd(_mm256_add_pd(_mm256_loadu_pd(v + i), half),
                                    _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC), zero);
                __m256d x1 = _mm256_max_pd(
                    _mm256_round_pd(_mm256_add_pd(_mm256_loadu_pd(v + i + 4), half),
                                    _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC), zero);
                _mm256_store_si256(reinterpret_cast<__m256i *>(w),
                                   _mm256_sub_epi64(_mm256_castpd_si256(
                                       _mm256_add_pd(x0, magic)), magicI));
                _mm256_store_si256(reinterpret_cast<__m256i *>(w + 4),
                                   _mm256_sub_epi64(_mm256_castpd_si256(
                                       _mm256_add_pd(x1, magic)), magicI));
                uint64_t s0 = carry + w[0]; uint64_t q0 = divBASE(s0);
                uint64_t s1 = q0 + w[1];    uint64_t q1 = divBASE(s1);
                uint64_t s2 = q1 + w[2];    uint64_t q2 = divBASE(s2);
                uint64_t s3 = q2 + w[3];    uint64_t q3 = divBASE(s3);
                uint64_t s4 = q3 + w[4];    uint64_t q4 = divBASE(s4);
                uint64_t s5 = q4 + w[5];    uint64_t q5 = divBASE(s5);
                uint64_t s6 = q5 + w[6];    uint64_t q6 = divBASE(s6);
                uint64_t s7 = q6 + w[7];    uint64_t q7 = divBASE(s7);
                out[i]     = Limb(s0 - q0 * BASE);
                out[i + 1] = Limb(s1 - q1 * BASE);
                out[i + 2] = Limb(s2 - q2 * BASE);
                out[i + 3] = Limb(s3 - q3 * BASE);
                out[i + 4] = Limb(s4 - q4 * BASE);
                out[i + 5] = Limb(s5 - q5 * BASE);
                out[i + 6] = Limb(s6 - q6 * BASE);
                out[i + 7] = Limb(s7 - q7 * BASE);
                carry = q7;
            }
            for (; i < n; i++)
            {
                double d = v[i] + 0.5;
                carry += uint64_t(int64_t(d < 0.0 ? 0.0 : d));
                uint64_t q = divBASE(carry);
                out[i] = Limb(carry - q * BASE);
                carry = q;
            }
            return carry;
        }
'''
s = s.replace(ANCHOR, ANCHOR + HELPER, 1)

# ---------- 2) \u628a 5 \u5904 8-way \u8fdb\u4f4d\u5757 + \u5c3e\u5faa\u73af \u6362\u6210 helper \u8c03\u7528 ----------
RX = re.compile(
    r'uint64_t carry = 0;\n'
    r'(?P<ind>[ ]+)size_t i = 0;\n'
    r'[ ]+for \(; i \+ 7 < (?P<bound>\w+); i \+= 8\)\n'
    r'.*?'
    r'\n[ ]+carry = q7;\n[ ]+\}\n'
    r'[ ]+for \(; i < (?P=bound); i\+\+\)\n'
    r'[ ]+\{\n'
    r'[ ]+carry \+= uint64_t\((?P<var>\w+)\[i\] \+ 0\.5\);\n'
    r'[ ]+uint64_t q = divBASE\(carry\);\n'
    r'[ ]+out\[i\] = Limb\(carry - q \* BASE\);\n'
    r'[ ]+carry = q;\n'
    r'[ ]+\}\n',
    re.S)


def rep(m):
    ind, bound, var = m.group('ind'), m.group('bound'), m.group('var')
    return ('uint64_t carry = carryNormalize(%s, %s, out.ptr);\n'
            '%ssize_t i = %s;\n'
            '%s(void)i;\n' % (var, bound, ind, bound, ind))


s, cnt = RX.subn(rep, s)
print("carry blocks replaced =", cnt)
if cnt != 5:
    sys.exit("expected 5 replacements, got %d" % cnt)

open(DST, "w", encoding="utf-8", errors="surrogateescape").write(s)
print("D20 written, %d -> %d bytes" % (len(orig), len(s)))
