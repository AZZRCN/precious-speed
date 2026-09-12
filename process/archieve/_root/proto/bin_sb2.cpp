// bin_sb2.cpp —— 二进制 schoolbook 除法原型 v2
//
// v1 教训: 90M instr / 41.4M cycles, 其中 bin2dec 33.7% + dec2bin 28.8% = 转换吃掉 90%。
//          转换慢的原因: (a) 每次 divrem 重算 invert_limb (硬件 divq);
//                        (b) 用 1e16 分块要归一化移位;
//                        (c) 十进制limb 中转多余一层。
// v2 改法:
//   1. 基数改 10^19。1e19 >= 2^63 => **已归一, divrem 不用任何移位**,
//      且 v = invert_limb(1e19) = 0xd83c94fb6d2ac34a 是编译期常量。
//   2. 直接 字符串 <-> 二进制, 不经十进制 limb 中转
//      (这也正是真集成时的形态: 小除法根本不建十进制 limb)。
//   3. SWAR 解析 / 两位查表输出。
//
// 编译: g++ -O2 -std=c++17 -march=x86-64-v3 bin_sb2.cpp -o bin_sb2
// 运行: ./bin_sb2 <input.in> [run|verify|stat]
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <immintrin.h>
#include <string>

using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using u128 = unsigned __int128;

static const u64 D19 = 10000000000000000000ull;      // 10^19, 已归一 (bit63=1)
static const u64 V19 = 0xd83c94fb6d2ac34aull;        // invert_limb(D19)

// ============================ mpn 原语 ============================

static inline u64 mul_1(u64 *r, const u64 *s, size_t n, u64 x, u64 cin)
{
    u64 c = cin;
    for (size_t i = 0; i < n; i++)
    {
        u128 p = (u128)s[i] * x + c;
        r[i] = (u64)p;
        c = (u64)(p >> 64);
    }
    return c;
}

static inline u64 submul_1(u64 *r, const u64 *s, size_t n, u64 x)
{
    u64 c = 0;
    for (size_t i = 0; i < n; i++)
    {
        u128 p = (u128)s[i] * x + c;
        u64 pl = (u64)p;
        c = (u64)(p >> 64);
        u64 ri = r[i];
        u64 t = ri - pl;
        c += (t > ri);
        r[i] = t;
    }
    return c;
}

static inline u64 add_n(u64 *r, const u64 *s, size_t n)
{
    unsigned char cf = 0;
    for (size_t i = 0; i < n; i++)
    {
        unsigned long long t;
        cf = _addcarry_u64(cf, (unsigned long long)r[i], (unsigned long long)s[i], &t);
        r[i] = (u64)t;
    }
    return cf;
}

static inline u64 lshift(u64 *r, const u64 *s, size_t n, unsigned sh)
{
    if (sh == 0) { if (r != s) memmove(r, s, n * sizeof(u64)); return 0; }
    u64 high = s[n - 1] >> (64 - sh);
    for (size_t i = n - 1; i > 0; i--)
        r[i] = (s[i] << sh) | (s[i - 1] >> (64 - sh));
    r[0] = s[0] << sh;
    return high;
}

static inline void rshift(u64 *r, const u64 *s, size_t n, unsigned sh)
{
    if (sh == 0) { if (r != s) memmove(r, s, n * sizeof(u64)); return; }
    for (size_t i = 0; i + 1 < n; i++)
        r[i] = (s[i] >> sh) | (s[i + 1] << (64 - sh));
    r[n - 1] = s[n - 1] >> sh;
}

static inline u64 invert_limb(u64 d) { return (u64)((~(u128)0) / d); }

static inline void udiv_qrnnd_preinv(u64 &q, u64 &r, u64 uh, u64 ul, u64 d, u64 v)
{
    u128 t = (u128)v * uh;
    u64 qh = (u64)(t >> 64), ql = (u64)t;
    u64 ql2 = ql + ul;
    qh += uh + 1 + (ql2 < ql);
    u64 rr = ul - qh * d;
    if (rr > ql2) { qh--; rr += d; }
    if (__builtin_expect(rr >= d, 0)) { qh++; rr -= d; }
    q = qh; r = rr;
}

// 专用: 除以常量 10^19 (已归一, 无移位, v 编译期常量)
static inline u64 divrem_D19(u64 *q, const u64 *u, size_t n)
{
    u64 r = 0;
    for (size_t i = n; i-- > 0;)
    {
        u64 qi, ri;
        udiv_qrnnd_preinv(qi, ri, r, u[i], D19, V19);
        q[i] = qi; r = ri;
    }
    return r;
}

static inline u64 divrem_1(u64 *q, const u64 *u, size_t n, u64 d)
{
    unsigned sh = (unsigned)__builtin_clzll(d);
    u64 dn = d << sh;
    u64 v = invert_limb(dn);
    u64 r;
    if (sh == 0)
    {
        r = 0;
        for (size_t i = n; i-- > 0;)
        { u64 qi, ri; udiv_qrnnd_preinv(qi, ri, r, u[i], dn, v); q[i] = qi; r = ri; }
        return r;
    }
    r = u[n - 1] >> (64 - sh);
    for (size_t i = n; i-- > 0;)
    {
        u64 cur = (u[i] << sh) | (i ? (u[i - 1] >> (64 - sh)) : 0);
        u64 qi, ri; udiv_qrnnd_preinv(qi, ri, r, cur, dn, v); q[i] = qi; r = ri;
    }
    return r >> sh;
}

// ============================ Knuth D ============================
static void bin_divrem(const u64 *u, size_t m, const u64 *v, size_t k,
                       u64 *q, u64 *r, u64 *wu, u64 *wv)
{
    if (k == 1) { r[0] = divrem_1(q, u, m, v[0]); return; }
    unsigned sh = (unsigned)__builtin_clzll(v[k - 1]);
    lshift(wv, v, k, sh);
    wu[m] = lshift(wu, u, m, sh);
    const u64 d1 = wv[k - 1], d0 = wv[k - 2];
    const u64 vinv = invert_limb(d1);
    for (size_t j = m - k + 1; j-- > 0;)
    {
        u64 qhat, rhat;
        u64 uh = wu[j + k], ul = wu[j + k - 1];
        if (__builtin_expect(uh >= d1, 0))
        {
            qhat = ~(u64)0;
            u64 rh; bool ov = __builtin_add_overflow(ul, d1, &rh);
            if (!ov)
                while ((u128)qhat * d0 > (((u128)rh << 64) | wu[j + k - 2]))
                { qhat--; if (__builtin_add_overflow(rh, d1, &rh)) break; }
        }
        else
        {
            udiv_qrnnd_preinv(qhat, rhat, uh, ul, d1, vinv);
            while ((u128)qhat * d0 > (((u128)rhat << 64) | wu[j + k - 2]))
            { qhat--; if (__builtin_add_overflow(rhat, d1, &rhat)) break; }
        }
        u64 borrow = submul_1(wu + j, wv, k, qhat);
        if (__builtin_expect(wu[j + k] < borrow, 0))
        { qhat--; u64 c = add_n(wu + j, wv, k); wu[j + k] += c; }
        wu[j + k] -= borrow;
        q[j] = qhat;
    }
    rshift(r, wu, k, sh);
}

// ============================ 字符串 <-> 二进制 ============================

static inline u64 parse8(const char *p)
{
    u64 v; memcpy(&v, p, 8);
    v -= 0x3030303030303030ull;
    v = (v * 10 + (v >> 8)) & 0x00FF00FF00FF00FFull;
    v = (v * 100 + (v >> 16)) & 0x0000FFFF0000FFFFull;
    v = (v * 10000 + (v >> 32)) & 0xFFFFFFFFull;
    return v;
}
// 解析 len (1..19) 位十进制
static inline u64 parse_chunk(const char *p, size_t len)
{
    u64 v = 0;
    while (len >= 8) { v = v * 100000000ull + parse8(p); p += 8; len -= 8; }
    while (len--) v = v * 10 + u64(*p++ - '0');
    return v;
}

// 字符串 -> 二进制 limb, 返回 limb 数
static size_t str2bin(const char *s, size_t n, u64 *out)
{
    size_t len = 0;
    size_t first = n % 19; if (first == 0) first = 19;
    u64 c = parse_chunk(s, first);
    if (c) out[len++] = c;
    size_t pos = first;
    while (pos < n)
    {
        u64 ch = parse_chunk(s + pos, 19); pos += 19;
        u64 carry = mul_1(out, out, len, D19, ch);
        if (carry) out[len++] = carry;
    }
    return len;
}

static const char DIG2[201] =
    "00010203040506070809101112131415161718192021222324252627282930313233343536373839"
    "40414243444546474849505152535455565758596061626364656667686970717273747576777879"
    "8081828384858687888990919293949596979899";

// 把 v 写成恰好 19 位 (前导补零) 到 p
static inline void write19(char *p, u64 v)
{
    for (int i = 0; i < 9; i++)
    {
        u32 d = u32(v % 100); v /= 100;
        p[17 - 2 * i] = DIG2[2 * d]; p[18 - 2 * i] = DIG2[2 * d + 1];
    }
    p[0] = char('0' + v);
}
// 二进制 -> 十进制字符串, 返回长度; 破坏 b
static size_t bin2str(u64 *b, size_t nb, char *out)
{
    if (nb == 0) { out[0] = '0'; return 1; }
    u64 parts[512]; size_t np = 0;
    while (nb)
    {
        u64 rem = divrem_D19(b, b, nb);
        while (nb && b[nb - 1] == 0) nb--;
        parts[np++] = rem;
    }
    // 最高块不补零
    char *p = out;
    { u64 v = parts[np - 1]; char tmp[24]; int L = 0;
      if (v == 0) tmp[L++] = '0';
      while (v) { tmp[L++] = char('0' + v % 10); v /= 10; }
      while (L) *p++ = tmp[--L]; }
    for (size_t i = np - 1; i-- > 0;) { write19(p, parts[i]); p += 19; }
    return size_t(p - out);
}

// ============================ 主程序 ============================
int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: bin_sb2 <input.in> [run|verify|stat]\n"); return 1; }
    const char *mode = (argc > 2) ? argv[2] : "run";

    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror("open"); return 1; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    std::vector<char> buf(sz + 32);
    if (fread(buf.data(), 1, sz, f) != (size_t)sz) { fprintf(stderr, "read fail\n"); return 1; }
    buf[sz] = 0; fclose(f);

    std::vector<std::pair<size_t, size_t>> tok;
    { size_t i = 0;
      while (i < (size_t)sz)
      { while (i < (size_t)sz && (unsigned char)buf[i] <= ' ') i++;
        size_t st = i;
        while (i < (size_t)sz && (unsigned char)buf[i] > ' ') i++;
        if (i > st) tok.push_back({st, i - st}); } }
    size_t T = (size_t)strtoull(std::string(buf.data() + tok[0].first, tok[0].second).c_str(), 0, 10);

    // 只留 D47 会走 schoolbook 的用例
    const size_t L = 64;
    std::vector<std::pair<size_t, size_t>> ta, tb;   // (off,len)
    size_t skipped = 0, decops = 0;
    for (size_t i = 0; i < T; i++)
    {
        size_t oa = tok[1 + 2 * i].first, la = tok[1 + 2 * i].second;
        size_t ob = tok[2 + 2 * i].first, lb = tok[2 + 2 * i].second;
        if (buf[oa] == '-' || buf[ob] == '-') { skipped++; continue; }
        if (la < lb) { skipped++; continue; }
        size_t n1 = (la + 3) / 4, n2 = (lb + 3) / 4, qn = n1 - n2 + 1;
        if (!(n2 <= L || qn <= L)) { skipped++; continue; }
        if (lb == 1 && buf[ob] == '0') { skipped++; continue; }
        ta.push_back({oa, la}); tb.push_back({ob, lb});
        decops += qn * n2;
    }

    if (!strcmp(mode, "stat"))
    { printf("cases=%zu (skipped %zu)  decimal schoolbook limb-ops=%zu (x8.1 = %.3g Ir)\n",
             ta.size(), skipped, decops, decops * 8.1);
      return 0; }

    // 分阶段计量: stat=读+切token, p=+str2bin, pd=+divrem, run=+bin2str
    const int PH = !strcmp(mode, "p") ? 1 : (!strcmp(mode, "pd") ? 2 : 3);

    const size_t CAP = 2048;
    std::vector<u64> ua(CAP), ub(CAP), uq(CAP), ur(CAP), wu(CAP), wv(CAP), a2(CAP), chk(CAP * 2);
    std::vector<char> ob(CAP * 24);
    u64 sink = 0; size_t bad = 0;

    for (size_t i = 0; i < ta.size(); i++)
    {
        const char *sa = buf.data() + ta[i].first; size_t la = ta[i].second;
        const char *sb = buf.data() + tb[i].first; size_t lb = tb[i].second;
        size_t m = str2bin(sa, la, ua.data());
        size_t k = str2bin(sb, lb, ub.data());
        if (m < k) { sink++; continue; }
        if (PH == 1) { sink += ua[m - 1] + ub[k - 1] + m + k; continue; }
        bin_divrem(ua.data(), m, ub.data(), k, uq.data(), ur.data(), wu.data(), wv.data());
        size_t nq = m - k + 1; while (nq > 1 && uq[nq - 1] == 0) nq--;
        size_t nr = k;         while (nr > 1 && ur[nr - 1] == 0) nr--;

        if (!strcmp(mode, "verify"))
        {
            std::fill(chk.begin(), chk.begin() + m + 2, 0ull);
            for (size_t j = 0; j < nq; j++)
            {
                u64 c = 0, x = uq[j];
                for (size_t t = 0; t < k; t++)
                { u128 p = (u128)ub[t] * x + chk[j + t] + c; chk[j + t] = (u64)p; c = (u64)(p >> 64); }
                size_t t = j + k;
                while (c) { u128 s2 = (u128)chk[t] + c; chk[t] = (u64)s2; c = (u64)(s2 >> 64); t++; }
            }
            u64 c = 0;
            for (size_t t = 0; t < nr; t++)
            { u128 s2 = (u128)chk[t] + ur[t] + c; chk[t] = (u64)s2; c = (u64)(s2 >> 64); }
            size_t t = nr;
            while (c) { u128 s2 = (u128)chk[t] + c; chk[t] = (u64)s2; c = (u64)(s2 >> 64); t++; }
            size_t m2 = str2bin(sa, la, a2.data());
            bool ok = (m2 <= m + 1);
            for (size_t t2 = 0; ok && t2 < m + 2; t2++)
            { u64 av = (t2 < m2) ? a2[t2] : 0; if (chk[t2] != av) ok = false; }
            if (ok && nr == k)
            { for (size_t t2 = k; t2-- > 0;)
              { if (ur[t2] != ub[t2]) { ok = (ur[t2] < ub[t2]); break; }
                if (t2 == 0) ok = false; } }
            else if (ok && nr > k) ok = false;
            if (!ok) { bad++; if (bad < 5) fprintf(stderr, "MISMATCH case %zu (la=%zu lb=%zu)\n", i, la, lb); }
        }

        if (PH == 2) { sink += uq[nq - 1] + ur[nr - 1] + nq + nr; continue; }
        size_t lq = bin2str(uq.data(), nq, ob.data());
        size_t lr = bin2str(ur.data(), nr, ob.data() + lq + 1);
        sink += lq + lr + (u64)(unsigned char)ob[0];
    }
    fprintf(stderr, "cases=%zu skipped=%zu sink=%llu bad=%zu\n",
            ta.size(), skipped, (unsigned long long)sink, bad);
    return bad ? 2 : 0;
}
