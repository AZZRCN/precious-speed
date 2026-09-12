// dec19_sb.cpp —— 基数 10^19 十进制 schoolbook 除法原型 v3
//
// v1/v2 教训:
//   二进制 schoolbook 的除法内核确实便宜 (rnz_01 只要 14.7M instr / 6.9M cyc,
//   对比 D47 十进制 base-1e4 的 88.4M Ir 模型 / absSubMul1 66.4M cyc),
//   但 字符串<->二进制 转换是 O(n^2), 吃掉 84% 的成本 => 二进制路线净亏。
//
// v3 思路: 不换到二进制, 只把十进制基数从 10^4 (u16) 提到 10^19 (u64)。
//   * 串<->limb 转换保持 **线性** (纯分块, SWAR 解析 / 两位查表输出)
//   * limb 数每维缩小 4.75x  =>  schoolbook limb-ops 降 ~22.6x
//   * 代价: 每个 limb-op 要一次 "128 位除以 10^19"。但 10^19 >= 2^63 已归一,
//           且 v = invert_limb(1e19) 是编译期常量 => Moller-Granlund 约 10 条指令。
//   预期 ~15 instr/limb-op * 0.48M = ~7M instr, 对比 base-1e4 的 88.4M。
//
// 编译: g++ -O2 -std=c++17 -march=x86-64-v3 dec19_sb.cpp -o dec19_sb
// 运行: ./dec19_sb <input.in> [run|verify|stat|p|pd]
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>

using u32 = uint32_t;
using u64 = uint64_t;
using u128 = unsigned __int128;
using i128 = __int128;

static const u64 D19 = 10000000000000000000ull;   // 10^19, bit63=1 => 已归一
static const u64 V19 = 0xd83c94fb6d2ac34aull;     // invert_limb(10^19)
static const u64 NEGD = 0ull - D19;               // 2^64 - 10^19

static inline u64 invert_limb(u64 d) { return (u64)((~(u128)0) / d); }

// Moller-Granlund: (uh,ul)/d, 要求 d>=2^63 且 uh<d, v=invert_limb(d)
static inline void udiv_qrnnd_preinv(u64 &q, u64 &r, u64 uh, u64 ul, u64 d, u64 v)
{
    u128 t = (u128)v * uh;
    u64 qh = (u64)(t >> 64), ql = (u64)t;
    u64 ql2 = ql + ul;
    qh += uh + 1 + (ql2 < ql);
    u64 rr = ul - qh * d;
    u64 m1 = 0ull - (u64)(rr > ql2);      // 无分支: 这一步实测 ~50% 命中, 用分支必崩
    qh += m1; rr += d & m1;
    u64 m2 = 0ull - (u64)(rr >= d);
    qh -= m2; rr -= d & m2;
    q = qh; r = rr;
}

// 128 位除以常量 10^19 (无移位, 逆是编译期常量) —— 全部 limb-op 的核心
static inline void divrem_D19(u64 &q, u64 &r, u64 uh, u64 ul)
{
    udiv_qrnnd_preinv(q, r, uh, ul, D19, V19);
}

// ====================== base-10^19 mpn 原语 ======================

// r = s*x + cin,  返回 carry (< D19)
static inline u64 mul19_1(u64 *r, const u64 *s, size_t n, u64 x, u64 cin)
{
    u64 c = cin;
    for (size_t i = 0; i < n; i++)
    {
        u128 p = (u128)s[i] * x + c;
        u64 qi, ri;
        divrem_D19(qi, ri, (u64)(p >> 64), (u64)p);
        r[i] = ri; c = qi;
    }
    return c;
}

// r -= s*x,  返回 borrow (<= D19)
static inline u64 submul19_1(u64 *r, const u64 *s, size_t n, u64 x)
{
    u64 c = 0, b = 0;
    for (size_t i = 0; i < n; i++)
    {
        u128 p = (u128)s[i] * x + c;
        u64 qi, ri;
        divrem_D19(qi, ri, (u64)(p >> 64), (u64)p);
        c = qi;
        u64 sub = ri + b;              // <= D19, 不溢出
        u64 ri0 = r[i];
        u64 t = ri0 - sub;
        b = (u64)(ri0 < sub);
        r[i] = t + (D19 & (0ull - b));  // 无分支
    }
    return c + b;
}

// r += s,  返回 carry (0/1)。注意 2*D19 > 2^64, 须处理 u64 回绕
static inline u64 add19_n(u64 *r, const u64 *s, size_t n)
{
    u64 c = 0;
    for (size_t i = 0; i < n; i++)
    {
        u64 a = r[i] + c;                    // <= D19, 不溢出
        u64 t = a + s[i];
        // 真值 = t + ov*2^64;  真值>=D19  <=>  ov | (t>=D19)
        u64 ge = (u64)(t < a) | (u64)(t >= D19);
        r[i] = t + (NEGD & (0ull - ge));     // 加 2^64-D19 == 减 D19 (mod 2^64)
        c = ge;
    }
    return c;
}

// q = u / d, 返回余数;  d < D19 任意
static inline u64 divrem19_1(u64 *q, const u64 *u, size_t n, u64 d)
{
    int sh = __builtin_clzll(d);
    u64 dn = d << sh;
    u64 vinv = invert_limb(dn);
    u64 r = 0;
    for (size_t i = n; i-- > 0;)
    {
        u128 num = (u128)r * D19 + u[i];   // < d * D19
        u64 nh = (u64)(num >> 64), nl = (u64)num;
        u64 h2 = (nh << sh) | ((nl >> 1) >> (63 - sh));   // sh=0 时右项恒 0, 无 UB
        u64 l2 = nl << sh;
        u64 qi, ri;
        udiv_qrnnd_preinv(qi, ri, h2, l2, dn, vinv);
        q[i] = qi; r = ri >> sh;
    }
    return r;
}

// Knuth D, base 10^19。a(m limbs) / b(k limbs) -> q(m-k+1), rem(k)
// wu 需 m+1 limbs, wv 需 k limbs
static void dec19_divrem(const u64 *a, size_t m, const u64 *b, size_t k,
                         u64 *q, u64 *rem, u64 *wu, u64 *wv)
{
    if (k == 1) { rem[0] = divrem19_1(q, a, m, b[0]); return; }

    // 归一化: 乘 d 使 wv[k-1] >= D19/2
    u64 d = D19 / (b[k - 1] + 1);
    if (d == 1)
    { memcpy(wv, b, k * 8); memcpy(wu, a, m * 8); wu[m] = 0; }
    else
    { mul19_1(wv, b, k, d, 0); wu[m] = mul19_1(wu, a, m, d, 0); }

    u64 v1 = wv[k - 1];
    int sh = __builtin_clzll(v1);          // 0 或 1 (v1 >= 5e18)
    u64 dn = v1 << sh;
    u64 vinv = invert_limb(dn);

    for (size_t j = m - k + 1; j-- > 0;)
    {
        u128 num = (u128)wu[j + k] * D19 + wu[j + k - 1];
        u64 nh = (u64)(num >> 64), nl = (u64)num;
        u64 h2 = (nh << sh) | ((nl >> 1) >> (63 - sh));   // sh=0 时右项恒 0, 无 UB
        u64 l2 = nl << sh;
        u64 qhat, rhat;
        if (__builtin_expect(h2 >= dn, 0)) qhat = D19 - 1;
        else
        {
            udiv_qrnnd_preinv(qhat, rhat, h2, l2, dn, vinv);
            if (__builtin_expect(qhat >= D19, 0)) qhat = D19 - 1;
        }

        u64 cy = submul19_1(wu + j, wv, k, qhat);
        i128 t = (i128)wu[j + k] - (i128)cy;
        while (t < 0)
        { qhat--; t += add19_n(wu + j, wv, k); }
        wu[j + k] = (u64)t;
        q[j] = qhat;
    }

    if (d == 1) memcpy(rem, wu, k * 8);
    else divrem19_1(rem, wu, k, d);
}

// ====================== 线性 串 <-> limb19 ======================

static inline u64 parse8(const char *p)
{
    u64 v; memcpy(&v, p, 8);
    v -= 0x3030303030303030ull;
    v = (v * 10 + (v >> 8)) & 0x00FF00FF00FF00FFull;
    v = (v * 100 + (v >> 16)) & 0x0000FFFF0000FFFFull;
    v = (v * 10000 + (v >> 32)) & 0xFFFFFFFFull;
    return v;
}
static inline u64 parse_chunk(const char *p, size_t len)
{
    u64 v = 0;
    while (len >= 8) { v = v * 100000000ull + parse8(p); p += 8; len -= 8; }
    while (len--) v = v * 10 + u64(*p++ - '0');
    return v;
}

// 串 -> base-1e19 limb (小端), 返回 limb 数。**线性**
static size_t str2dec19(const char *s, size_t n, u64 *out)
{
    size_t first = n % 19; if (!first) first = 19;
    size_t nl = 1 + (n - first) / 19;
    size_t idx = nl;
    out[--idx] = parse_chunk(s, first);
    size_t pos = first;
    while (pos < n) { out[--idx] = parse_chunk(s + pos, 19); pos += 19; }
    while (nl > 1 && out[nl - 1] == 0) nl--;
    return nl;
}

static const char DIG2[201] =
    "00010203040506070809101112131415161718192021222324252627282930313233343536373839"
    "40414243444546474849505152535455565758596061626364656667686970717273747576777879"
    "8081828384858687888990919293949596979899";

static inline void write19(char *p, u64 v)
{
    for (int i = 0; i < 9; i++)
    {
        u32 dd = u32(v % 100); v /= 100;
        p[17 - 2 * i] = DIG2[2 * dd]; p[18 - 2 * i] = DIG2[2 * dd + 1];
    }
    p[0] = char('0' + v);
}

// base-1e19 limb -> 串, 返回长度。**线性**
static size_t dec19str(const u64 *v, size_t n, char *out)
{
    char *p = out;
    u64 t = v[n - 1];
    if (t == 0) *p++ = '0';
    else { char b[24]; int bl = 0; while (t) { b[bl++] = char('0' + t % 10); t /= 10; }
           while (bl) *p++ = b[--bl]; }
    for (size_t i = n - 1; i-- > 0;) { write19(p, v[i]); p += 19; }
    return (size_t)(p - out);
}

// ====================== harness ======================

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: dec19_sb <input.in> [run|verify|stat|p|pd]\n"); return 1; }
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

    const size_t L = 64;                       // D47 schoolbook 路由阈值 (base-1e4 limb)
    std::vector<std::pair<size_t, size_t>> ta, tb;
    size_t skipped = 0, dec4 = 0, dec19 = 0;
    size_t g_sm = 0, g_sk = 0, g_sq = 0, g_k1 = 0;
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
        dec4 += qn * n2;
        size_t m1 = (la + 18) / 19, m2 = (lb + 18) / 19;
        dec19 += (m1 >= m2 ? (m1 - m2 + 1) : 1) * m2;
        g_sm += m1; g_sk += m2; g_sq += (m1 >= m2 ? m1 - m2 + 1 : 1);
        if (m2 == 1) g_k1++;
    }

    if (!strcmp(mode, "stat"))
    { size_t N = std::max<size_t>(ta.size(), 1);
      printf("cases=%zu (skipped %zu)\n  base-1e4  limb-ops=%zu (x8.1 = %.3g Ir)\n"
             "  base-1e19 limb-ops=%zu  (缩减 %.1fx)\n"
             "  平均 m=%.1f k=%.1f qn=%.1f;  k==1 的 case 数=%zu (%.1f%%)\n"
             "  每 case 固定开销 op: 归一化 m+k=%.1f, 末尾 divrem k=%.1f, qhat qn=%.1f"
             "  => 固定/内环 = %.2f\n",
             ta.size(), skipped, dec4, dec4 * 8.1, dec19, (double)dec4 / (double)std::max<size_t>(dec19, 1),
             (double)g_sm / N, (double)g_sk / N, (double)g_sq / N, g_k1, 100.0 * g_k1 / N,
             (double)(g_sm + g_sk) / N, (double)g_sk / N, (double)g_sq / N,
             (double)(g_sm + 2 * g_sk + g_sq) / (double)std::max<size_t>(dec19, 1));
      return 0; }

    const int PH = !strcmp(mode, "p") ? 1 : (!strcmp(mode, "pd") ? 2 : 3);

    const size_t CAP = 1024;
    std::vector<u64> ua(CAP), ub(CAP), uq(CAP), ur(CAP), wu(CAP), wv(CAP), a2(CAP), chk(CAP * 2);
    std::vector<char> ob(CAP * 24);
    u64 sink = 0; size_t bad = 0;

    for (size_t i = 0; i < ta.size(); i++)
    {
        const char *sa = buf.data() + ta[i].first; size_t la = ta[i].second;
        const char *sb = buf.data() + tb[i].first; size_t lb = tb[i].second;
        size_t m = str2dec19(sa, la, ua.data());
        size_t k = str2dec19(sb, lb, ub.data());
        if (m < k) { sink++; continue; }
        if (PH == 1) { sink += ua[m - 1] + ub[k - 1] + m + k; continue; }

        dec19_divrem(ua.data(), m, ub.data(), k, uq.data(), ur.data(), wu.data(), wv.data());
        size_t nq = m - k + 1; while (nq > 1 && uq[nq - 1] == 0) nq--;
        size_t nr = k;         while (nr > 1 && ur[nr - 1] == 0) nr--;

        if (!strcmp(mode, "verify"))
        {
            // chk = b*q + r, 与 a 比对; 并查 r < b
            std::fill(chk.begin(), chk.begin() + m + 2, 0ull);
            for (size_t j = 0; j < nq; j++)
            {
                u64 c = 0, x = uq[j];
                for (size_t t = 0; t < k; t++)
                {
                    u128 p = (u128)ub[t] * x + c;
                    u64 qi, ri; divrem_D19(qi, ri, (u64)(p >> 64), (u64)p);
                    c = qi;
                    u64 a0 = chk[j + t], s2 = a0 + ri;
                    u64 cc;
                    if (s2 < a0) { chk[j + t] = s2 + NEGD; cc = 1; }
                    else if (s2 >= D19) { chk[j + t] = s2 - D19; cc = 1; }
                    else { chk[j + t] = s2; cc = 0; }
                    if (cc) { size_t z = j + t + 1;
                              while (true) { u64 w = chk[z] + 1; if (w >= D19) { chk[z] = w - D19; z++; } else { chk[z] = w; break; } } }
                }
                size_t z = j + k;
                while (c)
                { u64 a0 = chk[z], s2 = a0 + c;
                  if (s2 < a0) { chk[z] = s2 + NEGD; c = 1; }
                  else if (s2 >= D19) { chk[z] = s2 - D19; c = 1; }
                  else { chk[z] = s2; c = 0; }
                  z++; }
            }
            u64 c = 0;
            for (size_t t = 0; t < nr; t++)
            { u64 a0 = chk[t] + c, s2 = a0 + ur[t];
              if (s2 < a0) { chk[t] = s2 + NEGD; c = 1; }
              else if (s2 >= D19) { chk[t] = s2 - D19; c = 1; }
              else { chk[t] = s2; c = 0; } }
            size_t z = nr;
            while (c)
            { u64 w = chk[z] + 1; if (w >= D19) { chk[z] = w - D19; z++; } else { chk[z] = w; c = 0; } }

            size_t m2 = str2dec19(sa, la, a2.data());
            bool ok = (m2 <= m + 1);
            for (size_t t2 = 0; ok && t2 < m + 2; t2++)
            { u64 av = (t2 < m2) ? a2[t2] : 0; if (chk[t2] != av) ok = false; }
            if (ok)
            { // r < b ?
              if (nr > k) ok = false;
              else if (nr == k)
              { bool lt = false;
                for (size_t t2 = k; t2-- > 0;)
                { if (ur[t2] != ub[t2]) { lt = (ur[t2] < ub[t2]); break; } }
                ok = lt; }
            }
            if (!ok) { bad++; if (bad < 5) fprintf(stderr, "MISMATCH case %zu (la=%zu lb=%zu)\n", i, la, lb); }
        }

        if (PH == 2) { sink += uq[nq - 1] + ur[nr - 1] + nq + nr; continue; }
        size_t lq = dec19str(uq.data(), nq, ob.data());
        size_t lr = dec19str(ur.data(), nr, ob.data() + lq + 1);
        sink += lq + lr + (u64)(unsigned char)ob[0];
    }
    fprintf(stderr, "cases=%zu skipped=%zu sink=%llu bad=%zu\n",
            ta.size(), skipped, (unsigned long long)sink, bad);
    return bad ? 2 : 0;
}
