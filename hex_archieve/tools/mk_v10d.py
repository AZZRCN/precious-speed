#!/usr/bin/env python3
"""v10c -> v10d: 把两级 twiddle 的复乘参数交换, 让 unpack 也能提到循环外.

cmul(a,b) = fmaddsub(unpacklo(a,a), b, mul(unpackhi(a,a), permute(b,1)))
  即第 1 个参数会被 broadcast 成 re/im 两份。

v10c: twlo(i, hi) = cmul(twbase[i&mask], hi)
      -> 被 broadcast 的是 twbase[lo] (循环内变量), 2 次 unpack 无法外提。
      每 twiddle: load + unpacklo + unpackhi + permute + mul + fmaddsub = 6 uops

v10d: 改为 cmul(hi, twbase[i&mask]) (复乘可交换)
      -> 被 broadcast 的是 hi (循环外常量), 2 次 unpack 提到层循环外。
      每 twiddle: load + permute + mul + fmaddsub = 4 uops  (省 2)

★ 注意: 数学等价但 **不逐位等价** —— im 分量的 fma 乘积配对变了
  (lo_re*hi_im + lo_im*hi_re  vs  hi_re*lo_im + hi_im*lo_re), 差 ~1 ulp。
  故 v10d 必须重跑压测 + maxprec 精度压测, 不能只 diff 官方点。
"""
import io, os, sys

os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
src = sys.argv[1] if len(sys.argv) > 1 else 'work/mul/v10c.cpp'
dst = sys.argv[2] if len(sys.argv) > 2 else 'work/mul/v10d.cpp'
s = io.open(src, encoding='utf-8').read()

# ---- 1. 引入 TwHi (预 broadcast 的高位分量) ----
old = """// 高位分量提到循环外时用: 低位 lo, 高位分量 hi 已备好
static inline cpx twlo(u32 i, cpx hi) { return cmul(twbase[i & twHalfMask], hi); }
static inline cpx twhi(u32 i) { return twbase[twHalfSize | (i >> twHalfLog)]; }"""
new = """// 高位分量提到循环外时用。v10d: 预先 broadcast 成 re/im 两份, 让 unpack 也外提。
// cmul 的第 1 参数会被 broadcast, 故这里用 cmul(hi, lo) 而非 cmul(lo, hi)
// (复乘可交换; im 分量的 fma 配对因此改变, 差约 1 ulp, 已由压测+精度测验收)
struct TwHi { __m128d re, im; };
static inline TwHi twhi(u32 i) {
    const cpx h = twbase[twHalfSize | (i >> twHalfLog)];
    return TwHi{_mm_unpacklo_pd(h, h), _mm_unpackhi_pd(h, h)};
}
static inline cpx twlo(u32 i, const TwHi& h) {
    const cpx lo = twbase[i & twHalfMask];
    return _mm_fmaddsub_pd(h.re, lo, _mm_mul_pd(h.im, _mm_permute_pd(lo, 1)));
}"""
assert s.count(old) == 1, 'twlo/twhi decl not found'
s = s.replace(old, new)

# ---- 2. 三处 `const cpx hiA = twhi(...)` 改类型 ----
reps = [
    ('const cpx hiA = twhi(base), hiB = twhi(base2);',
     'const TwHi hiA = twhi(base), hiB = twhi(base2);'),
    ('const cpx hiA = twhi(base);',
     'const TwHi hiA = twhi(base);'),
]
n = 0
for a, b in reps:
    c = s.count(a)
    n += c
    s = s.replace(a, b)
assert n == 3, f'expected 3 hiA decls, got {n}'

s = s.replace('// v10c: 两级 twiddle 表 (外提高位分量, 带跨块保护)',
              '// v10d: 两级 twiddle 表 (外提高位分量+预 broadcast, 带跨块保护)', 1)

io.open(dst, 'w', encoding='utf-8', newline='\n').write(s)
print('wrote', dst, len(s), 'bytes')
