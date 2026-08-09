// bin_sb.cpp —— 二进制 schoolbook 除法原型 (对标 div_D47 的十进制 absDivBasicCore)
//
// 目的: 验证「小除法转二进制做」是否真能把 rnz_01 的 schoolbook 段砍掉 4~9x。
//   当前十进制路径实测: absSubMul1 = 112.9M Ir / rnz_01, 其中 schoolbook 段约 88.4M Ir。
//   本原型只跑 D47 会路由到 schoolbook 的那些用例 (n2<=64 || qn<=64),
//   全流程 = 十进制limb -> 二进制 -> Knuth D 除法 -> 回十进制limb。
//   用 perf stat 量 instructions/cycles, 直接和 88.4M / ~81M 对比。
//
// 编译: g++ -O2 -std=c++17 -march=x86-64-v3 bin_sb.cpp -o bin_sb
// 运行: ./bin_sb <input.in> [mode]
//   mode = run    只跑, 供 perf 计量 (默认)
//   mode = verify 每例校验 q*b+r==a 且 0<=r<b (慢, 只为正确性)
//   mode = stat   打印规模统计
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <vector>
#include <string>
#include <algorithm>
#include <immintrin.h>

using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using u128 = unsigned __int128;

static const u64 DEC16 = 10000000000000000ull; // 1e16 = 4 个 1e4 limb
static const u16 BASE4 = 10000;

// ============================ 基础 mpn 原语 ============================

// r[0..n) = s[0..n) * x + cin ; 返回高位进位
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

// r[0..n) -= s[0..n) * x ; 返回借位 (0..2^64-1)
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
        c += (t > ri); // 借位并入下一轮的 c
        r[i] = t;
    }
    return c;
}

// r[0..n) += s[0..n) ; 返回进位
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

// 左移 (0 < sh < 64), r 与 s 可同址; 返回移出的高位
static inline u64 lshift(u64 *r, const u64 *s, size_t n, unsigned sh)
{
    if (sh == 0)
    {
        if (r != s) memmove(r, s, n * sizeof(u64));
        return 0;
    }
    u64 high = s[n - 1] >> (64 - sh);
    for (size_t i = n - 1; i > 0; i--)
        r[i] = (s[i] << sh) | (s[i - 1] >> (64 - sh));
    r[0] = s[0] << sh;
    return high;
}

// 右移 (0 < sh < 64)
static inline void rshift(u64 *r, const u64 *s, size_t n, unsigned sh)
{
    if (sh == 0)
    {
        if (r != s) memmove(r, s, n * sizeof(u64));
        return;
    }
    for (size_t i = 0; i + 1 < n; i++)
        r[i] = (s[i] >> sh) | (s[i + 1] << (64 - sh));
    r[n - 1] = s[n - 1] >> sh;
}

// Möller-Granlund: d >= 2^63, 返回 v = floor((2^128-1)/d) - 2^64
static inline u64 invert_limb(u64 d)
{
    u128 all = ~(u128)0;
    return (u64)(all / d);
}

// (uh,ul) / d, 要求 uh < d 且 d >= 2^63; v = invert_limb(d)
static inline void udiv_qrnnd_preinv(u64 &q, u64 &r, u64 uh, u64 ul, u64 d, u64 v)
{
    u128 t = (u128)v * uh;
    u64 qh = (u64)(t >> 64);
    u64 ql = (u64)t;
    u64 ql2 = ql + ul;
    qh += uh + 1 + (ql2 < ql);
    u64 rr = ul - qh * d;
    if (rr > ql2) { qh--; rr += d; }
    if (__builtin_expect(rr >= d, 0)) { qh++; rr -= d; }
    q = qh;
    r = rr;
}

// u[0..n) /= d (单 limb), 商写回 q[0..n), 返回余数。d 任意 (内部归一)
static inline u64 divrem_1(u64 *q, const u64 *u, size_t n, u64 d)
{
    unsigned sh = (unsigned)__builtin_clzll(d);
    u64 dn = d << sh;
    u64 v = invert_limb(dn);
    u64 r = 0;
    if (sh == 0)
    {
        for (size_t i = n; i-- > 0;)
        {
            u64 qi, ri;
            udiv_qrnnd_preinv(qi, ri, r, u[i], dn, v);
            q[i] = qi; r = ri;
        }
        return r;
    }
    // 归一: 等价于把 (u<<sh) 除以 dn, 末了余数再 >>sh。
    // 移位后 limb i = (u[i]<<sh) | (u[i-1]>>(64-sh)); 溢出的最高 sh 位作初始余数。
    r = u[n - 1] >> (64 - sh);   // < 2^sh <= 2^63 <= dn, 满足 uh<d
    for (size_t i = n; i-- > 0;)
    {
        u64 cur = (u[i] << sh) | (i ? (u[i - 1] >> (64 - sh)) : 0);
        u64 qi, ri;
        udiv_qrnnd_preinv(qi, ri, r, cur, dn, v);
        q[i] = qi; r = ri;
    }
    return r >> sh;
}

// ============================ Knuth D 二进制除法 ============================
// un[0..m) / vn[0..k) -> q[0..m-k+1), r 写回 un 低 k limb。破坏 un/vn 内容。
// 要求 m >= k >= 1, vn[k-1] != 0。内部缓冲由调用者给 (wu: m+1 limb, wv: k limb)。
static void bin_divrem(const u64 *u, size_t m, const u64 *v, size_t k,
                       u64 *q, u64 *r, u64 *wu, u64 *wv)
{
    if (k == 1)
    {
        r[0] = divrem_1(q, u, m, v[0]);
        return;
    }
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
            // 商溢出 64 位 -> 钳到 2^64-1, 余数 rhat = (uh*2^64+ul) - qhat*d1 = d1+ul (uh==d1)
            qhat = ~(u64)0;
            u64 rh;
            bool ov = __builtin_add_overflow(ul, d1, &rh);
            if (!ov)   // rhat >= 2^64 时 3-by-2 判据恒不成立, 直接跳过
            {
                while ((u128)qhat * d0 > (((u128)rh << 64) | wu[j + k - 2]))
                {
                    qhat--;
                    if (__builtin_add_overflow(rh, d1, &rh)) break;
                }
            }
        }
        else
        {
            udiv_qrnnd_preinv(qhat, rhat, uh, ul, d1, vinv);
            while ((u128)qhat * d0 > (((u128)rhat << 64) | wu[j + k - 2]))
            {
                qhat--;
                if (__builtin_add_overflow(rhat, d1, &rhat)) break;
            }
        }
        u64 borrow = submul_1(wu + j, wv, k, qhat);
        if (__builtin_expect(wu[j + k] < borrow, 0))
        {
            // 估高了, 加回一次
            qhat--;
            u64 c = add_n(wu + j, wv, k);
            wu[j + k] += c;
        }
        wu[j + k] -= borrow;
        q[j] = qhat;
    }
    rshift(r, wu, k, sh);
}

// ============================ 进制转换 ============================

// 十进制 limb (u16, base 1e4, 小端) -> 二进制 u64 limb, 返回 limb 数
static size_t dec2bin(const u16 *d, size_t nd, u64 *out)
{
    size_t len = 0;
    size_t nchunk = (nd + 3) / 4;
    for (size_t ci = nchunk; ci-- > 0;)
    {
        size_t base = ci * 4;
        u64 c = 0;
        size_t hi = std::min(base + 4, nd);
        for (size_t i = hi; i-- > base;)
            c = c * BASE4 + d[i];
        u64 mul = DEC16;
        if (hi - base < 4)
        {
            mul = 1;
            for (size_t t = 0; t < hi - base; t++) mul *= BASE4;
        }
        // out = out * mul + c
        u64 carry = mul_1(out, out, len, mul, c);
        if (carry) out[len++] = carry;
    }
    while (len && out[len - 1] == 0) len--;
    return len;
}

// 二进制 u64 limb -> 十进制 limb (u16, base 1e4), 返回 limb 数; 破坏 b
static size_t bin2dec(u64 *b, size_t nb, u16 *out)
{
    size_t no = 0;
    while (nb)
    {
        u64 rem = divrem_1(b, b, nb, DEC16);
        while (nb && b[nb - 1] == 0) nb--;
        if (nb)
        {
            for (int t = 0; t < 4; t++) { out[no++] = u16(rem % BASE4); rem /= BASE4; }
        }
        else
        {
            while (rem) { out[no++] = u16(rem % BASE4); rem /= BASE4; }
        }
    }
    if (no == 0) out[no++] = 0;
    return no;
}

// ============================ 输入解析 ============================

struct Num { std::vector<u16> d; };   // base 1e4 小端

static void str2dec(const char *s, size_t n, std::vector<u16> &out)
{
    out.clear();
    size_t nl = (n + 3) / 4;
    out.resize(nl);
    size_t pos = n;
    for (size_t i = 0; i < nl; i++)
    {
        size_t lo = (pos >= 4) ? pos - 4 : 0;
        u16 v = 0;
        for (size_t k = lo; k < pos; k++) v = u16(v * 10 + (s[k] - '0'));
        out[i] = v;
        pos = lo;
    }
    while (out.size() > 1 && out.back() == 0) out.pop_back();
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: bin_sb <input.in> [run|verify|stat]\n"); return 1; }
    const char *mode = (argc > 2) ? argv[2] : "run";

    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror("open"); return 1; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    std::vector<char> buf(sz + 1);
    if (fread(buf.data(), 1, sz, f) != (size_t)sz) { fprintf(stderr, "read fail\n"); return 1; }
    buf[sz] = 0; fclose(f);

    // 切 token
    std::vector<std::pair<size_t, size_t>> tok; // (off,len)
    {
        size_t i = 0;
        while (i < (size_t)sz)
        {
            while (i < (size_t)sz && (unsigned char)buf[i] <= ' ') i++;
            size_t st = i;
            while (i < (size_t)sz && (unsigned char)buf[i] > ' ') i++;
            if (i > st) tok.push_back({st, i - st});
        }
    }
    size_t T = (size_t)strtoull(std::string(buf.data() + tok[0].first, tok[0].second).c_str(), 0, 10);

    // 预解析成十进制 limb, 只保留 D47 会走 schoolbook 的用例
    const size_t L = 64;
    std::vector<Num> A, B;
    A.reserve(T); B.reserve(T);
    size_t skipped = 0;
    for (size_t i = 0; i < T; i++)
    {
        const char *sa = buf.data() + tok[1 + 2 * i].first;   size_t la = tok[1 + 2 * i].second;
        const char *sb = buf.data() + tok[2 + 2 * i].first;   size_t lb = tok[2 + 2 * i].second;
        if (sa[0] == '-' || sb[0] == '-') { skipped++; continue; }
        size_t n1 = (la + 3) / 4, n2 = (lb + 3) / 4;
        if (la < lb || lb == 0) { skipped++; continue; }
        size_t qn = n1 - n2 + 1;
        if (!(n2 <= L || qn <= L)) { skipped++; continue; }
        Num na, nb;
        str2dec(sa, la, na.d);
        str2dec(sb, lb, nb.d);
        if (nb.d.size() == 1 && nb.d[0] == 0) { skipped++; continue; }
        A.push_back(std::move(na)); B.push_back(std::move(nb));
    }

    if (!strcmp(mode, "stat"))
    {
        size_t ops = 0;
        for (size_t i = 0; i < A.size(); i++)
        {
            size_t n1 = A[i].d.size(), n2 = B[i].d.size();
            ops += (n1 - n2 + 1) * n2;
        }
        printf("cases=%zu (skipped %zu)  decimal schoolbook limb-ops=%zu  (x8.1 = %.3g Ir)\n",
               A.size(), skipped, ops, ops * 8.1);
        return 0;
    }

    // 工作缓冲
    const size_t CAP = 4096;
    std::vector<u64> ua(CAP), ub(CAP), uq(CAP), ur(CAP), wu(CAP), wv(CAP);
    std::vector<u16> dq(CAP * 8), dr(CAP * 8);
    std::vector<u64> chk(CAP * 2);

    u64 sink = 0;
    size_t bad = 0;
    for (size_t i = 0; i < A.size(); i++)
    {
        const std::vector<u16> &da = A[i].d, &db = B[i].d;
        std::fill(ua.begin(), ua.begin() + (da.size() / 4 + 4), 0ull);
        std::fill(ub.begin(), ub.begin() + (db.size() / 4 + 4), 0ull);
        size_t m = dec2bin(da.data(), da.size(), ua.data());
        size_t k = dec2bin(db.data(), db.size(), ub.data());
        if (m < k) { // a<b: q=0, r=a
            sink += 1; continue;
        }
        bin_divrem(ua.data(), m, ub.data(), k, uq.data(), ur.data(), wu.data(), wv.data());
        size_t nq = m - k + 1;
        while (nq > 1 && uq[nq - 1] == 0) nq--;
        size_t nr = k;
        while (nr > 1 && ur[nr - 1] == 0) nr--;

        if (!strcmp(mode, "verify"))
        {
            // chk = q*b + r, 与 a 比较
            std::fill(chk.begin(), chk.begin() + m + 2, 0ull);
            for (size_t j = 0; j < nq; j++)
            {
                u64 c = 0;
                u64 x = uq[j];
                for (size_t t = 0; t < k; t++)
                {
                    u128 p = (u128)ub[t] * x + chk[j + t] + c;
                    chk[j + t] = (u64)p;
                    c = (u64)(p >> 64);
                }
                size_t t = j + k;
                while (c) { u128 s = (u128)chk[t] + c; chk[t] = (u64)s; c = (u64)(s >> 64); t++; }
            }
            u64 c = 0;
            for (size_t t = 0; t < nr; t++)
            {
                u128 s = (u128)chk[t] + ur[t] + c; chk[t] = (u64)s; c = (u64)(s >> 64);
            }
            size_t t = nr;
            while (c) { u128 s = (u128)chk[t] + c; chk[t] = (u64)s; c = (u64)(s >> 64); t++; }
            // 与原 a 比 (需重算 a 的二进制, 因为 ua 已被 lshift 破坏)
            std::vector<u64> a2(m + 2, 0);
            size_t m2 = dec2bin(da.data(), da.size(), a2.data());
            bool ok = (m2 <= m + 1);
            for (size_t t2 = 0; ok && t2 < m + 2; t2++)
            {
                u64 av = (t2 < m2) ? a2[t2] : 0;
                if (chk[t2] != av) ok = false;
            }
            // r < b ?
            if (ok)
            {
                if (nr > k) ok = false;
                else if (nr == k)
                {
                    for (size_t t2 = k; t2-- > 0;)
                    {
                        if (ur[t2] != ub[t2]) { ok = (ur[t2] < ub[t2]); break; }
                        if (t2 == 0) ok = false;
                    }
                }
            }
            if (!ok) { bad++; if (bad < 5) fprintf(stderr, "MISMATCH case %zu (da=%zu db=%zu)\n", i, da.size(), db.size()); }
        }

        size_t lq = bin2dec(uq.data(), nq, dq.data());
        size_t lr = bin2dec(ur.data(), nr, dr.data());
        sink += lq + lr + dq[0] + dr[0];
    }
    fprintf(stderr, "cases=%zu skipped=%zu sink=%llu bad=%zu\n",
            A.size(), skipped, (unsigned long long)sink, bad);
    return bad ? 2 : 0;
}
