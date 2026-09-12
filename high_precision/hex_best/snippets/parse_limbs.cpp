// ===== parse_limbs.cpp =====
// 合同: 把 n 个十六进制字符(大端, 位于 s[0..n)) 解析成 base-2^64 limbs, 小端存入 out[]。
//       返回 limb 数 = (n+15)/16。每个 limb = 16 个 hex 字符。调用方保证 s 长度 >= n 且字符合法。
// 当前实现: SSE2, 每 16 字符 -> 1 个 u64。优化方向: 升级 AVX2(一次 32 字符=2 limb),
//           a2n 用 vpternlogd 单指令完成 ASCII->nibble, 末尾 partial chunk 用 masked load 避免越界读。
#include <cstdint>
#include <immintrin.h>
using u64 = uint64_t;

// ASCII hex 字符(假定合法 0-9a-fA-F) -> nibble 值, SSE2 一次 16 字节
static inline __m128i a2n(__m128i v) {
    __m128i s0 = _mm_sub_epi8(v, _mm_set1_epi8('0'));
    __m128i sa = _mm_sub_epi8(_mm_or_si128(v, _mm_set1_epi8(0x20)), _mm_set1_epi8('a' - 10));
    __m128i gt = _mm_cmpgt_epi8(s0, _mm_set1_epi8(9));
    return _mm_blendv_epi8(s0, sa, gt);
}
// end 之前(不含)的 16 个 hex 字符 -> 1 个 base-2^64 limb (digit 顺序小端)
static inline u64 hexfull(const char* end) {
    __m128i nib = a2n(_mm_loadu_si128((const __m128i*)(end - 16)));
    __m128i b = _mm_maddubs_epi16(nib, _mm_set1_epi16(0x0110));   // 两字节一组乘 0x10 后打包
    b = _mm_packus_epi16(b, b);
    return __builtin_bswap64((u64)_mm_cvtsi128_si64(b));
}
// 前导不足 16 字符的 partial chunk: L = 有效字符数(1..15), 高位补 0
struct MaskTab { uint8_t m[17][16]; constexpr MaskTab():m{} {
    for (int L=0;L<=16;++L) for (int i=0;i<16;++i) m[L][i]=(i>=16-L)?0xFF:0x00; } };
static constexpr MaskTab MT{};
static inline u64 hexpart(const char* end, int L) {
    __m128i nib = a2n(_mm_loadu_si128((const __m128i*)(end - 16)));
    nib = _mm_and_si128(nib, _mm_load_si128((const __m128i*)MT.m[L]));
    __m128i b = _mm_maddubs_epi16(nib, _mm_set1_epi16(0x0110));
    b = _mm_packus_epi16(b, b);
    return __builtin_bswap64((u64)_mm_cvtsi128_si64(b));
}
static inline int parse_limbs(const char* s, int n, u64* out) {
    int nc = (n + 15) >> 4;
    const char* end = s + n;
    int full = nc - 1;
    for (int c = 0; c < full; ++c) out[c] = hexfull(end - (c << 4));
    int rem = n - (full << 4);
    out[full] = hexpart(s + rem, rem);
    return nc;
}
