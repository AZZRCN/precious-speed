#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Apply ADD optimization patches to candidate sources.

Usage: python patch_add.py <A1|A2|A3>
Patches are literal-string replacements against best/add.cpp content.
Every patch asserts the anchor appears exactly once -> no silent misapply.
"""
import sys
import os

PS = r"D:\precious_speed"
EXE = os.path.join(PS, "lc_bench", "exe")

# ---------------------------------------------------------------- anchors ---
PARSE_OLD = """        const char *sa = iCursor;
        size_t la;
        int64_t va;
        // FIX: parsePositiveUntilNondigit 不处理负号, 负数用 swarTokenLen 获取 token
        if (sa < iEnd && *sa == '-') {
            la = swarTokenLen(iCursor);
            iCursor += la;
            va = 0;
        } else {
            va = parsePositiveUntilNondigit(sa, la);
            iCursor = sa + la;
        }
        if (iCursor < iEnd && *iCursor < 0x21) iCursor++;  // skip space

        const char *sb = iCursor;
        size_t lb;
        int64_t vb;
        if (sb < iEnd && *sb == '-') {
            lb = swarTokenLen(iCursor);
            iCursor += lb;
            vb = 0;
        } else {
            vb = parsePositiveUntilNondigit(sb, lb);
            iCursor = sb + lb;
        }
        if (iCursor < iEnd && *iCursor < 0x21) iCursor++;  // skip newline"""

PARSE_NEW = """        // OPT-A1: 负号跳过 1 字节后直接喂 SWAR 快解析器, 再取负。
        // 旧版对负数先 swarTokenLen 扫一遍并丢弃值, 再 tryParseI64Unchecked 从头
        // 重解析(后者是 4 字节慢循环)。small_00 有 75% 的行含负号, 即 75% 的 token
        // 走了二次慢解析。现在负数与正数共用同一条 SWAR 8 字节快路径。
        const char *sa = iCursor;
        size_t dla;
        size_t nega = size_t(sa < iEnd && *sa == '-');
        int64_t va = parsePositiveUntilNondigit(sa + nega, dla);
        va = nega ? -va : va;
        iCursor = sa + nega + dla;
        if (iCursor < iEnd && *iCursor < 0x21) iCursor++;  // skip space

        const char *sb = iCursor;
        size_t dlb;
        size_t negb = size_t(sb < iEnd && *sb == '-');
        int64_t vb = parsePositiveUntilNondigit(sb + negb, dlb);
        vb = negb ? -vb : vb;
        iCursor = sb + negb + dlb;
        if (iCursor < iEnd && *iCursor < 0x21) iCursor++;  // skip newline"""

DISPATCH_OLD = """        // Check if both tokens are positive <= 18 digits (fast path)
        if (la > 0 && la <= 18 && lb > 0 && lb <= 18 && sa[0] != '-' && sb[0] != '-') {
            // va + vb < 2*10^18 < INT64_MAX, no overflow
#ifdef PROFILE_DIV
            auto _p2 = std::chrono::high_resolution_clock::now();
            t_parse += std::chrono::duration<double, std::milli>(_p2 - _p1).count();
#endif
            writeI64(va + vb);
#ifdef PROFILE_DIV
            _p1 = std::chrono::high_resolution_clock::now();
            t_write += std::chrono::duration<double, std::milli>(_p1 - _p2).count();
#endif
        } else {
            // Slow path: negative, or > 18 digits → use Integer
            if (tryParseI64Unchecked(sa, la, va) && tryParseI64Unchecked(sb, lb, vb)) {
                writeI64(va + vb);
            } else {
                a.fromCharRange(sa, sa + la);
                b.fromCharRange(sb, sb + lb);
                a += b;
                writeHint(a);
            }
#ifdef PROFILE_DIV
            _p1 = std::chrono::high_resolution_clock::now();
            t_write += std::chrono::duration<double, std::milli>(_p1 - _p0).count();
            t_parse += std::chrono::duration<double, std::milli>(_p1 - _p0).count();
#endif
        }
        *oCursor++ = '\\n';"""

# A1: same dispatch shape, but keyed on digit length only (sign already folded
# into va/vb). tryParseI64Unchecked is gone from the hot path entirely.
DISPATCH_NEW_A1 = """        // OPT-A1: 判定只看数字位数, 符号已折进 va/vb。tryParseI64Unchecked
        // 彻底从热路径消失 (仅 >18 位才落到 Integer 大整数路径)。
        if (dla > 0 && dla <= 18 && dlb > 0 && dlb <= 18) {
            // |va|,|vb| < 10^18 -> |va+vb| < 2*10^18 < INT64_MAX, no overflow
#ifdef PROFILE_DIV
            auto _p2 = std::chrono::high_resolution_clock::now();
            t_parse += std::chrono::duration<double, std::milli>(_p2 - _p1).count();
#endif
            writeI64(va + vb);
#ifdef PROFILE_DIV
            _p1 = std::chrono::high_resolution_clock::now();
            t_write += std::chrono::duration<double, std::milli>(_p1 - _p2).count();
#endif
        } else {
            // Slow path: > 18 digits -> use Integer
            a.fromCharRange(sa, sa + nega + dla);
            b.fromCharRange(sb, sb + negb + dlb);
            a += b;
            writeHint(a);
#ifdef PROFILE_DIV
            _p1 = std::chrono::high_resolution_clock::now();
            t_write += std::chrono::duration<double, std::milli>(_p1 - _p0).count();
            t_parse += std::chrono::duration<double, std::milli>(_p1 - _p0).count();
#endif
        }
        *oCursor++ = '\\n';"""

# A2: writeI64 also emits the trailing '\n' (and the '-') inside one 32B store,
# so main must NOT append '\n' on the fast path.
DISPATCH_NEW_A2 = """        // OPT-A1: 判定只看数字位数, 符号已折进 va/vb。
        // OPT-A2: 快路径调用 writeI64NL, 它把 '-' + 数字 + '\\n' 一次性写出。
        if (dla > 0 && dla <= 18 && dlb > 0 && dlb <= 18) {
#ifdef PROFILE_DIV
            auto _p2 = std::chrono::high_resolution_clock::now();
            t_parse += std::chrono::duration<double, std::milli>(_p2 - _p1).count();
#endif
            writeI64NL(va + vb);
#ifdef PROFILE_DIV
            _p1 = std::chrono::high_resolution_clock::now();
            t_write += std::chrono::duration<double, std::milli>(_p1 - _p2).count();
#endif
        } else {
            a.fromCharRange(sa, sa + nega + dla);
            b.fromCharRange(sb, sb + negb + dlb);
            a += b;
            writeHint(a);
            *oCursor++ = '\\n';
#ifdef PROFILE_DIV
            _p1 = std::chrono::high_resolution_clock::now();
            t_write += std::chrono::duration<double, std::milli>(_p1 - _p0).count();
            t_parse += std::chrono::duration<double, std::milli>(_p1 - _p0).count();
#endif
        }"""

# ------------------------------------------------------------- writeI64 -----
WRITE_OLD = """    static inline void writeI64(int64_t val) {
        if (val == 0) { *oCursor++ = '0'; return; }
        uint64_t uv;
        bool neg = val < 0;
        if (neg) {
            *oCursor++ = '-';
            uv = uint64_t(-(val + 1)) + 1;
        } else {
            uv = uint64_t(val);
        }
        // 10000-base decomposition: low -> high, max 5 groups (18 digits = 4+4+4+4+2)
        uint32_t limbs[5];
        int n = 0;
        while (uv >= 10000) {
            limbs[n++] = uint32_t(uv % 10000);
            uv /= 10000;
        }
        limbs[n++] = uint32_t(uv); // highest group (1-4 digits)
        // Output highest group (no leading zeros)
        uint32_t high = limbs[n - 1];
        if (high < 10) {
            *oCursor++ = char('0' + high);
        } else if (high < 100) {
            *oCursor++ = char('0' + high / 10);
            *oCursor++ = char('0' + high % 10);
        } else if (high < 1000) {
            *oCursor++ = char('0' + high / 100);
            *oCursor++ = char('0' + high / 10 % 10);
            *oCursor++ = char('0' + high % 10);
        } else {
            std::memcpy(oCursor, &hint::outTable.t[high], 4);
            oCursor += 4;
        }
        // Output remaining groups (zero-padded, 4 bytes/table lookup)
        for (int j = n - 2; j >= 0; j--) {
            std::memcpy(oCursor, &hint::outTable.t[limbs[j]], 4);
            oCursor += 4;
        }
    }"""

# --- A2 variant: fully branchless, 20-digit zero-padded scratch + 32B store ---
WRITE_NEW_A2 = """    static inline void writeI64(int64_t val) {
        if (val == 0) { *oCursor++ = '0'; return; }
        uint64_t uv;
        bool neg = val < 0;
        if (neg) {
            *oCursor++ = '-';
            uv = uint64_t(-(val + 1)) + 1;
        } else {
            uv = uint64_t(val);
        }
        // 10000-base decomposition: low -> high, max 5 groups (18 digits = 4+4+4+4+2)
        uint32_t limbs[5];
        int n = 0;
        while (uv >= 10000) {
            limbs[n++] = uint32_t(uv % 10000);
            uv /= 10000;
        }
        limbs[n++] = uint32_t(uv); // highest group (1-4 digits)
        // Output highest group (no leading zeros)
        uint32_t high = limbs[n - 1];
        if (high < 10) {
            *oCursor++ = char('0' + high);
        } else if (high < 100) {
            *oCursor++ = char('0' + high / 10);
            *oCursor++ = char('0' + high % 10);
        } else if (high < 1000) {
            *oCursor++ = char('0' + high / 100);
            *oCursor++ = char('0' + high / 10 % 10);
            *oCursor++ = char('0' + high % 10);
        } else {
            std::memcpy(oCursor, &hint::outTable.t[high], 4);
            oCursor += 4;
        }
        // Output remaining groups (zero-padded, 4 bytes/table lookup)
        for (int j = n - 2; j >= 0; j--) {
            std::memcpy(oCursor, &hint::outTable.t[limbs[j]], 4);
            oCursor += 4;
        }
    }

    // ---------------------------------------------------------------- OPT-A2
    // decDigits: 十进制位数, 无分支 (Lemire). x >= 1 required.
    static inline uint32_t decDigits(uint64_t x) {
        static const uint64_t kPow10[20] = {
            1ULL, 10ULL, 100ULL, 1000ULL, 10000ULL,
            100000ULL, 1000000ULL, 10000000ULL, 100000000ULL, 1000000000ULL,
            10000000000ULL, 100000000000ULL, 1000000000000ULL,
            10000000000000ULL, 100000000000000ULL, 1000000000000000ULL,
            10000000000000000ULL, 100000000000000000ULL,
            1000000000000000000ULL, 10000000000000000000ULL};
        uint32_t lz = 63u - uint32_t(__builtin_clzll(x));   // 0..63
        uint32_t d = ((lz + 1u) * 1233u) >> 12;             // floor(log10) guess
        return d + uint32_t(x >= kPow10[d]) ;               // 1..20, branchless
    }

    // writeI64NL: 把 '-' + 十进制数字 + '\\n' 一次性写出, 零数据相关分支。
    // small_00 的位数在 1..18 均匀分布 -> 旧版 while(uv>=10000) 的迭代次数
    // 0..4 完全随机, 分支预测器必然失效(每次 misprediction ~17 cycle)。
    // 这里改为: 定长展开成 5 组 4 位(含前导零)写入栈上 scratch, 再按位数偏移
    // 做一次 32 字节非对齐搬运。用固定吞吐换掉不可预测分支。
    static inline void writeI64NL(int64_t val) {
        uint64_t uv;
        uint64_t neg = uint64_t(val < 0);
        // 无分支取绝对值
        int64_t m = -int64_t(neg);
        uv = uint64_t((val ^ m) - m);

        // scratch[0..19] = 20 位十进制(前导零), scratch[20] = '\\n'
        // 负号写在 scratch[19-d], 正好落在数字起点前一格(前导零区)。
        alignas(32) char sc[64];
        uint64_t q1 = uv / 100000000ULL;          // 高 11 位
        uint32_t r1 = uint32_t(uv - q1 * 100000000ULL);
        uint32_t q2 = uint32_t(q1 / 100000000ULL); // 最高 3 位 (uv < 2e19)
        uint32_t r2 = uint32_t(q1 - uint64_t(q2) * 100000000ULL);
        uint32_t g4 = q2;
        uint32_t g3 = r2 / 10000u, g2 = r2 - g3 * 10000u;
        uint32_t g1 = r1 / 10000u, g0 = r1 - g1 * 10000u;
        std::memcpy(sc + 0,  &hint::outTable.t[g4], 4);
        std::memcpy(sc + 4,  &hint::outTable.t[g3], 4);
        std::memcpy(sc + 8,  &hint::outTable.t[g2], 4);
        std::memcpy(sc + 12, &hint::outTable.t[g1], 4);
        std::memcpy(sc + 16, &hint::outTable.t[g0], 4);
        sc[20] = '\\n';

        uint32_t d = (uv == 0) ? 1u : decDigits(uv);   // uv==0 -> 输出 "0"
        sc[19 - d] = '-';
        const char *src = sc + 20 - d - neg;
        // 一次 32B 搬运覆盖 '-' + 至多 19 位数字 + '\\n' (<= 21 字节)
        std::memcpy(oCursor, src, 32);
        oCursor += d + neg + 1;
    }"""

# --- A3 variant: keep branches but split 64-bit division into 32-bit halves ---
WRITE_NEW_A3 = """    static inline void writeI64(int64_t val) {
        if (val == 0) { *oCursor++ = '0'; return; }
        uint64_t uv;
        bool neg = val < 0;
        if (neg) {
            *oCursor++ = '-';
            uv = uint64_t(-(val + 1)) + 1;
        } else {
            uv = uint64_t(val);
        }
        // OPT-A3: 用值域分层代替 while 变长循环。
        // 旧版 while(uv>=10000) 迭代 0..4 次, 在 1..18 位均匀分布下完全不可预测。
        // 分层后每层内部是定长直线代码, 且低半部改用 32 位除法(magic mul 更短)。
        if (uv < 10000ULL) {                       // 1-4 位
            writeHead(uint32_t(uv));
        } else if (uv < 100000000ULL) {            // 5-8 位
            uint32_t u = uint32_t(uv);
            uint32_t h = u / 10000u;
            writeHead(h);
            put4(u - h * 10000u);
        } else if (uv < 1000000000000ULL) {        // 9-12 位
            uint32_t hi = uint32_t(uv / 100000000ULL);   // 1-4 位
            uint32_t lo = uint32_t(uv % 100000000ULL);
            uint32_t l1 = lo / 10000u;
            writeHead(hi);
            put4(l1);
            put4(lo - l1 * 10000u);
        } else if (uv < 10000000000000000ULL) {    // 13-16 位
            uint64_t hi = uv / 100000000ULL;             // 5-8 位
            uint32_t lo = uint32_t(uv - hi * 100000000ULL);
            uint32_t h = uint32_t(hi);
            uint32_t h1 = h / 10000u;
            uint32_t l1 = lo / 10000u;
            writeHead(h1);
            put4(h - h1 * 10000u);
            put4(l1);
            put4(lo - l1 * 10000u);
        } else {                                   // 17-20 位
            uint64_t q1 = uv / 100000000ULL;             // 9-12 位
            uint32_t r1 = uint32_t(uv - q1 * 100000000ULL);
            uint32_t hi = uint32_t(q1 / 100000000ULL);   // 1-4 位
            uint32_t r2 = uint32_t(q1 - uint64_t(hi) * 100000000ULL);
            uint32_t a1 = r2 / 10000u;
            uint32_t b1 = r1 / 10000u;
            writeHead(hi);
            put4(a1);
            put4(r2 - a1 * 10000u);
            put4(b1);
            put4(r1 - b1 * 10000u);
        }
    }"""

# ------------------------------------------------------- OPT-A4 SIMD parse ---
def _shuf_table():
    """kShufRight[dl][i]: 把 d[0..dl-1] 右对齐到结果 [16-dl..15], 其余置零。"""
    rows = []
    for dl in range(17):
        vals = []
        for i in range(16):
            if i >= 16 - dl:
                vals.append(str(i - 16 + dl))
            else:
                vals.append("-128")   # 0x80 -> pshufb 置零
        rows.append("        {" + ",".join(f"{v:>4}" for v in vals) + "}")
    return ",\n".join(rows)


SIMD_PARSE = """
    // ---------------------------------------------------------------- OPT-A4
    // 无分支 SIMD 解析。动机(callgrind 实测): 旧 parsePositiveUntilNondigit 按
    // "是否 >=8 位 / 是否 >=16 位" 分层, 而 small_00 的位数在 1..18 均匀分布,
    // P(>=8)=11/18=61% —— 正好是分支预测器最坏的情形。该函数独占了 A2 版全部
    // 分支误预测的 61.4% (336,067 / 547,800)。
    // 改造: 一次看 32 字节 -> movemask+ctz 直接得 token 长度 -> pshufb 右对齐
    // -> 定长 madd 链求值。主分支变成 P(<=16 位)=16/18=89% 的强偏斜分支。
    alignas(16) static const int8_t kShufRight[17][16] = {
%SHUF%
    };

    // 每字节是否 '0'..'9' 的位掩码
    static inline uint32_t digitMask16(__m128i v) {
        __m128i sub = _mm_sub_epi8(v, _mm_set1_epi8('0'));
        __m128i le9 = _mm_cmpeq_epi8(_mm_min_epu8(sub, _mm_set1_epi8(9)), sub);
        return uint32_t(_mm_movemask_epi8(le9));
    }

    // 解析 v 的前 dl (<=16) 位十进制, 全程无分支
    // maddubs: [d0,d1] -> d0*10+d1 ; madd: [x,y] -> x*100+y ; 再 *10000+
    static inline uint64_t simdParse16(__m128i v, uint32_t dl) {
        __m128i d = _mm_sub_epi8(v, _mm_set1_epi8('0'));
        __m128i a = _mm_shuffle_epi8(
            d, _mm_load_si128(reinterpret_cast<const __m128i *>(kShufRight[dl])));
        __m128i t1 = _mm_maddubs_epi16(a, _mm_set1_epi16(0x010A));      // 10,1
        __m128i t2 = _mm_madd_epi16(t1, _mm_set1_epi32(0x00010064));    // 100,1
        __m128i t3 = _mm_packus_epi32(t2, t2);
        __m128i t4 = _mm_madd_epi16(t3, _mm_set1_epi32(0x00012710));    // 10000,1
        uint64_t hi = uint32_t(_mm_cvtsi128_si32(t4));
        uint64_t lo = uint32_t(_mm_extract_epi32(t4, 1));
        return hi * 100000000ULL + lo;
    }

    static inline int64_t parseSIMD(const char *s, size_t &len) {
        // 尾部不足 32 字节 -> 回退到标量版 (mmap 末页越界保护)
        if (__builtin_expect(s + 32 > iEnd, 0)) {
            return parsePositiveUntilNondigit(s, len);
        }
        __m128i v0 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(s));
        __m128i v1 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(s + 16));
        uint32_t m = digitMask16(v0) | (digitMask16(v1) << 16);
        // 置第 32 位, 保证 m==0xFFFFFFFF 时 ctzll 有定义 (结果恰为 32)
        uint32_t dl = uint32_t(__builtin_ctzll((~uint64_t(m)) | (1ULL << 32)));
        len = dl;
        if (__builtin_expect(dl <= 16, 1)) {          // 89%
            return int64_t(simdParse16(v0, dl));
        }
        if (__builtin_expect(dl <= 18, 1)) {          // 17-18 位
            uint32_t hd = dl - 16;                    // 1 或 2
            uint64_t hv = uint64_t(s[0] - '0');
            if (hd == 2) hv = hv * 10 + uint64_t(s[1] - '0');
            __m128i lowv = _mm_loadu_si128(reinterpret_cast<const __m128i *>(s + hd));
            return int64_t(hv * 10000000000000000ULL + simdParse16(lowv, 16));
        }
        // >18 位: 值交给 Integer 路径, 这里只需给出正确的 token 长度。
        // dl<32 表示边界已在这 32 字节内找到, len=dl 已正确 —— 必须立刻返回,
        // 否则下面的续扫会跨过分隔符把两个 token 粘在一起 (曾导致 8 例 WA)。
        if (dl < 32) return 0;
        size_t i = 32;
        while (s + i + 8 <= iEnd) {
            uint64_t d8;
            std::memcpy(&d8, s + i, 8);
            uint64_t mk = (~d8) & (d8 - 0x2121212121212121ULL) & 0x8080808080808080ULL;
            if (mk) { i += size_t(__builtin_ctzll(mk)) / 8; len = i; return 0; }
            i += 8;
        }
        while (s + i < iEnd && s[i] >= 0x21) i++;
        len = i;
        return 0;
    }
"""

# A4/A5 use parseSIMD instead of parsePositiveUntilNondigit in the hot loop.
PARSE_NEW_SIMD = PARSE_NEW.replace("parsePositiveUntilNondigit(sa + nega, dla)",
                                   "parseSIMD(sa + nega, dla)") \
                          .replace("parsePositiveUntilNondigit(sb + negb, dlb)",
                                   "parseSIMD(sb + negb, dlb)") \
                          .replace("OPT-A1: 负号跳过", "OPT-A1+A4: 负号跳过")

# anchor to inject SIMD helpers right after parsePositiveUntilNondigit's closing
SIMD_ANCHOR = """        uint64_t r = parse8SWAR_u64(u);
        len = dl;
        return (int64_t)r;
    }
"""

WRITE_HELPERS_A3 = """    // OPT-A3 helpers
    static inline void put4(uint32_t v) {          // 定长 4 位(含前导零)
        std::memcpy(oCursor, &hint::outTable.t[v], 4);
        oCursor += 4;
    }
    static inline void writeHead(uint32_t v) {     // 最高组, 去前导零 (v < 10000)
        if (v < 10) {
            *oCursor++ = char('0' + v);
        } else if (v < 100) {
            std::memcpy(oCursor, reinterpret_cast<const char *>(&hint::outTable.t[v]) + 2, 2);
            oCursor += 2;
        } else if (v < 1000) {
            std::memcpy(oCursor, reinterpret_cast<const char *>(&hint::outTable.t[v]) + 1, 3);
            oCursor += 3;
        } else {
            std::memcpy(oCursor, &hint::outTable.t[v], 4);
            oCursor += 4;
        }
    }

"""


def apply(text, old, new, label):
    cnt = text.count(old)
    if cnt != 1:
        raise SystemExit(f"ANCHOR FAIL [{label}]: found {cnt} occurrences, expected 1")
    return text.replace(old, new)


def main():
    which = sys.argv[1].upper()
    src = os.path.join(PS, "best", "add.cpp")
    # best/add.cpp 是 CRLF; 用 universal-newline 读入归一化为 \n,
    # 输出统一写成 LF (候选只在 Ubuntu 上编译)。
    with open(src, "r", encoding="utf-8") as f:
        text = f.read()

    if which == "A1":
        text = apply(text, PARSE_OLD, PARSE_NEW, "parse")
        text = apply(text, DISPATCH_OLD, DISPATCH_NEW_A1, "dispatch")
        out = "add_A1_negfast.cpp"
    elif which == "A2":
        text = apply(text, PARSE_OLD, PARSE_NEW, "parse")
        text = apply(text, WRITE_OLD, WRITE_NEW_A2, "writeI64")
        text = apply(text, DISPATCH_OLD, DISPATCH_NEW_A2, "dispatch")
        out = "add_A2_nobranch.cpp"
    elif which == "A3":
        text = apply(text, PARSE_OLD, PARSE_NEW, "parse")
        text = apply(text, WRITE_OLD, WRITE_HELPERS_A3 + WRITE_NEW_A3, "writeI64")
        text = apply(text, DISPATCH_OLD, DISPATCH_NEW_A1, "dispatch")
        out = "add_A3_split.cpp"
    elif which == "A4":   # SIMD parse + branchless write
        simd = SIMD_PARSE.replace("%SHUF%", _shuf_table())
        text = apply(text, SIMD_ANCHOR, SIMD_ANCHOR + simd, "simd-inject")
        text = apply(text, PARSE_OLD, PARSE_NEW_SIMD, "parse")
        text = apply(text, WRITE_OLD, WRITE_NEW_A2, "writeI64")
        text = apply(text, DISPATCH_OLD, DISPATCH_NEW_A2, "dispatch")
        out = "add_A4_simd.cpp"
    elif which == "A6":   # A4 + branchless sign test
        simd = SIMD_PARSE.replace("%SHUF%", _shuf_table())
        text = apply(text, SIMD_ANCHOR, SIMD_ANCHOR + simd, "simd-inject")
        # 去掉 "sa < iEnd &&" 短路: 它逼编译器生成真分支, 而 *sa=='-' 是 50%
        # 纯随机 -> 每行 2 次几乎必然误预测 (实测 main 残留误预测 199,379 ≈ 2e5)。
        # 安全性: 循环由 t 控制, LC 保证 t 组数据存在, 故 sa < iEnd 恒成立。
        p6 = (PARSE_NEW_SIMD
              .replace("size_t nega = size_t(sa < iEnd && *sa == '-');",
                       "size_t nega = size_t(*sa == '-');   // OPT-A6 branchless")
              .replace("size_t negb = size_t(sb < iEnd && *sb == '-');",
                       "size_t negb = size_t(*sb == '-');   // OPT-A6 branchless")
              .replace("OPT-A1+A4:", "OPT-A1+A4+A6:"))
        if "OPT-A6 branchless" not in p6:
            raise SystemExit("A6 sign-test rewrite failed")
        text = apply(text, PARSE_OLD, p6, "parse")
        text = apply(text, WRITE_OLD, WRITE_NEW_A2, "writeI64")
        text = apply(text, DISPATCH_OLD, DISPATCH_NEW_A2, "dispatch")
        out = "add_A6_bsign.cpp"
    elif which == "A5":   # SIMD parse + tiered write
        simd = SIMD_PARSE.replace("%SHUF%", _shuf_table())
        text = apply(text, SIMD_ANCHOR, SIMD_ANCHOR + simd, "simd-inject")
        text = apply(text, PARSE_OLD, PARSE_NEW_SIMD, "parse")
        text = apply(text, WRITE_OLD, WRITE_HELPERS_A3 + WRITE_NEW_A3, "writeI64")
        text = apply(text, DISPATCH_OLD, DISPATCH_NEW_A1, "dispatch")
        out = "add_A5_simdtier.cpp"
    else:
        raise SystemExit("unknown variant: " + which)

    dst = os.path.join(EXE, out)
    with open(dst, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    print(f"OK {which} -> {dst}  ({len(text)} bytes)")


if __name__ == "__main__":
    main()
