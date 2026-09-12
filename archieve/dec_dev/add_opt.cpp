// 喵喵喵~ https://space.bilibili.com/620657947
// AZZRCN
// DEC addition 原型: int64/u128 短路径 + base-10^9 大数路径(简单进位)
// 目的: 验证「base-10^9 + 简单进位」才是 DEC 大数路径正解(严格优于现生产 base-10000+Barrett),
//       而 base-2^64(HEX 思路)因 decimal<->binary 不对齐, I/O 转换更贵, 对 DEC 反而亏。
// 注意: 短路径用简洁标量解析/格式化作示意(主导点 small_00 逻辑等价, 仅非 SIMD 最优);
//       重点测的是大数路径。正确性以对现生产 dec_add 全形状 bytecmp 收口。
#pragma GCC target("avx2,bmi,bmi2,popcnt,lzcnt")
#include <cstdint>
#include <cstring>
#include <immintrin.h>
#include <unistd.h>

constexpr int PAD = 128, INCAP = 9 << 20, OUTCAP = 9 << 20;
constexpr int MAXC = 200010;                 // 1.6M 位 / 9 ≈ 178k limb
alignas(64) static char inbuf_[PAD + INCAP + PAD];
alignas(64) static char outbuf[OUTCAP + 128];
static char* const inbuf = inbuf_ + PAD;
static uint32_t A[MAXC + 4], B[MAXC + 4], R[MAXC + 4];   // base-10^9 limbs (32-bit)

// ---------- 短路径: <=18 位 -> u64 ----------
static inline uint64_t parse_u64(const char* s, int n) {
    uint64_t v = 0;
    for (int i = 0; i < n; ++i) v = v * 10 + uint64_t(s[i] - '0');
    return v;
}
static inline char* fmt_u64(uint64_t uv, char* out) {
    if (uv == 0) { *out++ = '0'; return out; }
    char tmp[20]; int n = 0;
    while (uv) { tmp[n++] = char('0' + (uv % 10)); uv /= 10; }
    while (n) *out++ = tmp[--n];
    return out;
}
// ---------- 中路径: <=38 位 -> u128 ----------
static inline unsigned __int128 parse_u128(const char* s, int n) {
    unsigned __int128 v = 0;
    for (int i = 0; i < n; ++i) v = v * 10 + (unsigned __int128)(s[i] - '0');
    return v;
}
static inline char* fmt_u128(unsigned __int128 v, char* out) {
    if (v == 0) { *out++ = '0'; return out; }
    char tmp[40]; int n = 0;
    while (v) { tmp[n++] = char('0' + (int)(v % 10)); v /= 10; }
    while (n) *out++ = tmp[--n];
    return out;
}

// ---------- 大数路径: base-10^9 limbs (little-endian: limb[0]=最低位9位) ----------
// [s, s+n) 十进制 -> base-10^9 little-endian limbs; 返回 limb 数
static inline int parse_dec_limbs(const char* s, int n, uint32_t* out) {
    int nc = (n + 8) / 9;                 // ceil(n/9)
    int lead = n - 9 * (nc - 1);          // 最高位组的位数 (1..9)
    for (int k = 0; k < nc; ++k) {
        int start, cnt;
        if (k == nc - 1) { start = 0; cnt = lead; }            // 最高位组 (前 lead 位)
        else { start = n - 9 * (k + 1); cnt = 9; }              // 其余 (每 9 位一组)
        uint32_t limb = 0;
        for (int j = 0; j < cnt; ++j) limb = limb * 10 + uint32_t(s[start + j] - '0');
        out[k] = limb;
    }
    return nc;
}
static inline int mag_cmp(const uint32_t* a, int na, const uint32_t* b, int nb) {
    if (na != nb) return na > nb ? 1 : -1;
    for (int i = na - 1; i >= 0; --i) if (a[i] != b[i]) return a[i] > b[i] ? 1 : -1;
    return 0;
}
// 把 v 写成 digits 位十进制(前导零), 返回新 out
static inline char* fmt_limb(uint32_t v, int digits, char* out) {
    char tmp[9];
    for (int i = digits - 1; i >= 0; --i) { tmp[i] = char('0' + (v % 10)); v /= 10; }
    memcpy(out, tmp, (size_t)digits);
    return out + digits;
}

int main() {
    int len = 0;
    for (;;) {
        long r = read(0, inbuf + len, INCAP - len);
        if (r <= 0) break;
        len += (int)r;
    }
    memset(inbuf + len, 0, 96);
    inbuf[len] = '\n';

    const char* p = inbuf;
    while (*p < '0') ++p;
    uint32_t T = 0;
    while (*p > ' ') T = T * 10 + uint32_t(*p++ - '0');

    char* out = outbuf;

    for (uint32_t t = 0; t < T; ++t) {
        while (*p <= ' ') ++p;
        int sa = (*p == '-'); p += sa;
        const char* a0 = p;
        int la = 0; while (p[la] > ' ') ++la; p += la;
        while (*p <= ' ') ++p;
        int sb = (*p == '-'); p += sb;
        const char* b0 = p;
        int lb = 0; while (p[lb] > ' ') ++lb; p += lb;

        if (la <= 18 && lb <= 18) {
            uint64_t av = parse_u64(a0, la), bv = parse_u64(b0, lb);
            uint64_t mag; int neg;
            if (sa == sb) { mag = av + bv; neg = sa; }
            else if (av >= bv) { mag = av - bv; neg = sa; }
            else { mag = bv - av; neg = sb; }
            if (mag == 0) { *out++ = '0'; *out++ = '\n'; continue; }
            if (neg) *out++ = '-';
            out = fmt_u64(mag, out);
            *out++ = '\n';
            continue;
        }
        if (la <= 38 && lb <= 38) {
            unsigned __int128 av = parse_u128(a0, la), bv = parse_u128(b0, lb);
            unsigned __int128 mag; int neg;
            if (sa == sb) { mag = av + bv; neg = sa; }
            else if (av >= bv) { mag = av - bv; neg = sa; }
            else { mag = bv - av; neg = sb; }
            if (mag == 0) { *out++ = '0'; *out++ = '\n'; continue; }
            if (neg) *out++ = '-';
            out = fmt_u128(mag, out);
            *out++ = '\n';
            continue;
        }
        // ---------- 大数路径: base-10^9 ----------
        int nca = parse_dec_limbs(a0, la, A);
        int ncb = parse_dec_limbs(b0, lb, B);
        while (nca > 1 && A[nca - 1] == 0) --nca;
        while (ncb > 1 && B[ncb - 1] == 0) --ncb;

        int ncr, rsign;
        if (sa == sb) {
            int minc = nca < ncb ? nca : ncb;
            int maxc = nca > ncb ? nca : ncb;
            const uint32_t* L = (nca >= ncb) ? A : B;
            uint32_t carry = 0;
            int i = 0;
            for (; i < minc; ++i) {
                uint64_t s = (uint64_t)A[i] + (uint64_t)B[i] + carry;
                carry = s >= 1000000000ULL ? 1u : 0u;
                R[i] = (uint32_t)(s - carry * 1000000000ULL);
            }
            for (; i < maxc; ++i) {
                uint64_t s = (uint64_t)L[i] + carry;
                carry = s >= 1000000000ULL ? 1u : 0u;
                R[i] = (uint32_t)(s - carry * 1000000000ULL);
            }
            if (carry) R[maxc++] = 1;
            ncr = maxc; rsign = sa;
        } else {
            int cmp = mag_cmp(A, nca, B, ncb);
            if (cmp == 0) { *out++ = '0'; *out++ = '\n'; continue; }
            const uint32_t* big = cmp > 0 ? A : B;
            const uint32_t* small = cmp > 0 ? B : A;
            int nbig = cmp > 0 ? nca : ncb;
            int nsmall = cmp > 0 ? ncb : nca;
            rsign = cmp > 0 ? sa : sb;
            uint32_t borrow = 0;
            int i = 0;
            for (; i < nsmall; ++i) {
                uint64_t s = (uint64_t)big[i] + 1000000000ULL - (uint64_t)small[i] - borrow;
                if (s >= 1000000000ULL) { R[i] = (uint32_t)(s - 1000000000ULL); borrow = 0u; }
                else { R[i] = (uint32_t)s; borrow = 1u; }
            }
            for (; i < nbig; ++i) {
                uint64_t s = (uint64_t)big[i] + 1000000000ULL - borrow;
                if (s >= 1000000000ULL) { R[i] = (uint32_t)(s - 1000000000ULL); borrow = 0u; }
                else { R[i] = (uint32_t)s; borrow = 1u; }
            }
            ncr = nbig;
        }
        while (ncr > 1 && R[ncr - 1] == 0) --ncr;

        if (rsign && !(ncr == 1 && R[0] == 0)) *out++ = '-';
        uint32_t top = R[ncr - 1];
        int td = 1; for (uint32_t tv = top / 10; tv; tv /= 10) ++td;
        out = fmt_limb(top, td, out);
        for (int c = ncr - 2; c >= 0; --c) out = fmt_limb(R[c], 9, out);
        *out++ = '\n';
    }

    long off = 0, n = out - outbuf;
    while (off < n) {
        long w = write(1, outbuf + off, n - off);
        if (w <= 0) break;
        off += w;
    }
    return 0;
}
