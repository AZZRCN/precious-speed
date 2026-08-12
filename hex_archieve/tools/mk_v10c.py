#!/usr/bin/env python3
"""v10b -> v10c: 修复 twhi 循环外提在 2*bc > halfSize 时跨块失效的 BUG.

BUG: difFlat/ditFlat 把 twg(i) 的高位分量 twhi(base) 提到 j 循环外, 前提是
     (base+j) >> halfLog 在 j<bc 内恒定. base 天然对齐 bc, 故仅当 bc <= halfSize
     (hiA) / 2*bc <= halfSize (hiB, 步长 2 范围 2bc) 才成立.
     halfSize = 1<<((31-clz(n))>>1), 小 FFT (n=2^10/2^11 -> halfSize=32) 必越界.
修法: 层循环内加一次 safe 判定 (每层一次, 开销可忽略), 越界层退回完整 twg().
"""
import io, os, sys

os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
src = sys.argv[1] if len(sys.argv) > 1 else 'work/mul/v10b.cpp'
dst = sys.argv[2] if len(sys.argv) > 2 else 'work/mul/v10c.cpp'
s = io.open(src, encoding='utf-8').read()   # universal newlines -> \n

# ---- 1. difFlat 主循环 ----
old1 = """        const u32 q = bs >> 1, base = bb * bc, base2 = base << 1;
        // bc 为 2 的幂且 base 是 bc 的倍数 => j<bc 时 (base+j)>>halfLog 恒定; base2 同理
        const cpx hiA = twhi(base), hiB = twhi(base2);
        u32 j = 0;
        cpx* s = d;
        if (base == 0) { bf2FwdOne(s, q, twg(1)); j = 1, s += st; }
        for (; j != bc; ++j, s += st) {
            const cpx w1 = twlo(base2 + 2 * j, hiB);
            bf2Fwd(s, q, twlo(base + j, hiA), w1, mulI(w1));
        }"""
new1 = """        const u32 q = bs >> 1, base = bb * bc, base2 = base << 1;
        u32 j = 0;
        cpx* s = d;
        if (base == 0) { bf2FwdOne(s, q, twg(1)); j = 1, s += st; }
        // base/base2 天然对齐 bc/2bc; 仅当整块不跨 halfSize 边界才可外提高位分量
        if ((bc << 1) <= twHalfSize) {
            const cpx hiA = twhi(base), hiB = twhi(base2);
            for (; j != bc; ++j, s += st) {
                const cpx w1 = twlo(base2 + 2 * j, hiB);
                bf2Fwd(s, q, twlo(base + j, hiA), w1, mulI(w1));
            }
        } else {
            for (; j != bc; ++j, s += st) {
                const cpx w1 = twg(base2 + 2 * j);
                bf2Fwd(s, q, twg(base + j), w1, mulI(w1));
            }
        }"""
assert s.count(old1) == 1, 'difFlat main loop not found'
s = s.replace(old1, new1)

# ---- 2. difFlat 残余单层 (bs == 1) ----
old2 = """        const u32 base = bb * bc;
        const cpx hiA = twhi(base);
        u32 j = 0;
        cpx* s = d;
        if (base == 0) { bfPlain(s, 1); j = 1, s += 2; }
        for (; j != bc; ++j, s += 2) bfFwd(s, 1, twlo(base + j, hiA));"""
new2 = """        const u32 base = bb * bc;
        u32 j = 0;
        cpx* s = d;
        if (base == 0) { bfPlain(s, 1); j = 1, s += 2; }
        if (bc <= twHalfSize) {
            const cpx hiA = twhi(base);
            for (; j != bc; ++j, s += 2) bfFwd(s, 1, twlo(base + j, hiA));
        } else {
            for (; j != bc; ++j, s += 2) bfFwd(s, 1, twg(base + j));
        }"""
assert s.count(old2) == 1, 'difFlat tail layer not found'
s = s.replace(old2, new2)

# ---- 3. ditFlat 主循环 ----
old3 = """        const u32 st = q << 2, base = bb * bcOut, base2 = base << 1;
        const cpx hiA = twhi(base), hiB = twhi(base2);
        u32 j = 0;
        cpx* s = d;
        if (base == 0) { bf2InvOne(s, q, twg(1)); j = 1, s += st; }
        for (; j != bcOut; ++j, s += st) {
            const cpx w1 = twlo(base2 + 2 * j, hiB);
            bf2Inv(s, q, twlo(base + j, hiA), w1, mulI(w1));
        }"""
new3 = """        const u32 st = q << 2, base = bb * bcOut, base2 = base << 1;
        u32 j = 0;
        cpx* s = d;
        if (base == 0) { bf2InvOne(s, q, twg(1)); j = 1, s += st; }
        if ((bcOut << 1) <= twHalfSize) {
            const cpx hiA = twhi(base), hiB = twhi(base2);
            for (; j != bcOut; ++j, s += st) {
                const cpx w1 = twlo(base2 + 2 * j, hiB);
                bf2Inv(s, q, twlo(base + j, hiA), w1, mulI(w1));
            }
        } else {
            for (; j != bcOut; ++j, s += st) {
                const cpx w1 = twg(base2 + 2 * j);
                bf2Inv(s, q, twg(base + j), w1, mulI(w1));
            }
        }"""
assert s.count(old3) == 1, 'ditFlat main loop not found'
s = s.replace(old3, new3)

s = s.replace('// v10: 两级 twiddle 表',
              '// v10c: 两级 twiddle 表 (外提高位分量, 带跨块保护)', 1)

io.open(dst, 'w', encoding='utf-8', newline='\n').write(s)
print('wrote', dst, len(s), 'bytes')
