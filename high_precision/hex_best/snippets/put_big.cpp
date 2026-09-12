// ===== put_big.cpp =====
// 合同: 把 n 个 base-2^64 limbs(小端, V[0]=最低位) 序列化成十六进制字符串写入 out。
//       自动去除前导零 limb; 全零输出 "0"。返回写入后的终点指针(out 由调用方保证足够长)。
// 当前实现: SSE2, 每 limb -> 16 hex 字符。优化方向: 升级 AVX2(一次 32 字符=2 limb),
//           nib->ascii 用查表(__m128i 16 项 LUT) 替代 cmpgt+blend 分支。
#include <cstdint>
#include <immintrin.h>
using u64 = uint64_t;

// 1 个 base-2^64 limb -> 16 个 hex 字符(big-endian within limb) 写入 out
static inline void hex16_store(u64 val, char* out) {
    val = __builtin_bswap64(val);
    __m128i v = _mm_cvtsi64_si128((long long)val);
    __m128i hi = _mm_and_si128(_mm_srli_epi16(v, 4), _mm_set1_epi8(0x0F));
    __m128i lo = _mm_and_si128(v, _mm_set1_epi8(0x0F));
    __m128i nib = _mm_unpacklo_epi8(hi, lo);
    __m128i gt9 = _mm_cmpgt_epi8(nib, _mm_set1_epi8(9));
    __m128i asc = _mm_add_epi8(nib, _mm_set1_epi8('0'));
    __m128i alp = _mm_add_epi8(nib, _mm_set1_epi8('A' - 10));
    _mm_storeu_si128((__m128i*)out, _mm_or_si128(_mm_andnot_si128(gt9, asc), _mm_and_si128(gt9, alp)));
}
static inline char* put_u64(char* out, u64 v) {
    int d = v ? 16 - (int)(_lzcnt_u64(v) >> 2) : 1;   // 有效 hex 位数
    hex16_store(v << (64 - 4 * d), out);
    return out + d;
}
static inline char* put_big(char* out, const u64* V, int n) {
    while (n > 1 && V[n - 1] == 0) --n;
    if (n <= 0 || (n == 1 && V[0] == 0)) { *out++ = '0'; return out; }
    u64 top = V[n - 1];
    int d = 16 - (int)(_lzcnt_u64(top) >> 2);
    hex16_store(top << (64 - 4 * d), out);
    out += d;
    for (int c = n - 2; c >= 0; --c) { hex16_store(V[c], out); out += 16; }
    return out;
}
