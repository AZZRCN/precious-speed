#!/usr/bin/env python3
"""v10 -> v10b: 高位 twiddle 循环外提 + tw[2i+1]=tw[2i]*i 恒等式"""
import io, sys, os
os.chdir(os.path.join(os.path.dirname(__file__), '..'))
src = io.open('work/mul/v10.cpp', encoding='utf-8').read()

def sub(old, new):
    global src
    assert old in src, 'NOT FOUND:\n' + old[:120]
    src = src.replace(old, new, 1)

# 1) mulI
sub("static inline cpx twhi(u32 i) { return twbase[twHalfSize | (i >> twHalfLog)]; }",
    "static inline cpx twhi(u32 i) { return twbase[twHalfSize | (i >> twHalfLog)]; }\n"
    "// tw[2i+1] == tw[2i] * i (dump 验证的恒等式) -> 省一次 load + 一次复乘\n"
    "static inline cpx mulI(cpx z) {   // [re,im] -> [-im,re]\n"
    "    return _mm_xor_pd(_mm_shuffle_pd(z, z, 1), _mm_set_pd(0.0, -0.0));\n"
    "}")

# 2) difFlat 主循环
sub("""        const u32 q = bs >> 1, base = bb * bc, base2 = base << 1;
        u32 j = 0;
        cpx* s = d;
        if (base == 0) { bf2FwdOne(s, q, twg(1)); j = 1, s += st; }
        for (; j != bc; ++j, s += st)
            bf2Fwd(s, q, twg(base + j), twg(base2 + 2 * j), twg(base2 + 2 * j + 1));""",
    """        const u32 q = bs >> 1, base = bb * bc, base2 = base << 1;
        // bc 为 2 的幂且 base 是 bc 的倍数 => j<bc 时 (base+j)>>halfLog 恒定; base2 同理
        const cpx hiA = twhi(base), hiB = twhi(base2);
        u32 j = 0;
        cpx* s = d;
        if (base == 0) { bf2FwdOne(s, q, twg(1)); j = 1, s += st; }
        for (; j != bc; ++j, s += st) {
            const cpx w1 = twlo(base2 + 2 * j, hiB);
            bf2Fwd(s, q, twlo(base + j, hiA), w1, mulI(w1));
        }""")

# 3) difFlat 末层
sub("""        const u32 base = bb * bc;
        u32 j = 0;
        cpx* s = d;
        if (base == 0) { bfPlain(s, 1); j = 1, s += 2; }
        for (; j != bc; ++j, s += 2) bfFwd(s, 1, twg(base + j));""",
    """        const u32 base = bb * bc;
        const cpx hiA = twhi(base);
        u32 j = 0;
        cpx* s = d;
        if (base == 0) { bfPlain(s, 1); j = 1, s += 2; }
        for (; j != bc; ++j, s += 2) bfFwd(s, 1, twlo(base + j, hiA));""")

# 4) ditFlat 主循环
sub("""        const u32 st = q << 2, base = bb * bcOut, base2 = base << 1;
        u32 j = 0;
        cpx* s = d;
        if (base == 0) { bf2InvOne(s, q, twg(1)); j = 1, s += st; }
        for (; j != bcOut; ++j, s += st)
            bf2Inv(s, q, twg(base + j), twg(base2 + 2 * j), twg(base2 + 2 * j + 1));""",
    """        const u32 st = q << 2, base = bb * bcOut, base2 = base << 1;
        const cpx hiA = twhi(base), hiB = twhi(base2);
        u32 j = 0;
        cpx* s = d;
        if (base == 0) { bf2InvOne(s, q, twg(1)); j = 1, s += st; }
        for (; j != bcOut; ++j, s += st) {
            const cpx w1 = twlo(base2 + 2 * j, hiB);
            bf2Inv(s, q, twlo(base + j, hiA), w1, mulI(w1));
        }""")

# 5) difRec / ditRec 顶层
sub("""    if (bb == 0) bf2FwdOne(d, q, twg(1));
    else bf2Fwd(d, q, twg(bb), twg(bb << 1), twg((bb << 1) | 1));""",
    """    if (bb == 0) bf2FwdOne(d, q, twg(1));
    else { const cpx w1 = twg(bb << 1); bf2Fwd(d, q, twg(bb), w1, mulI(w1)); }""")

sub("""    if (bb == 0) bf2InvOne(d, q, twg(1));
    else bf2Inv(d, q, twg(bb), twg(bb << 1), twg((bb << 1) | 1));""",
    """    if (bb == 0) bf2InvOne(d, q, twg(1));
    else { const cpx w1 = twg(bb << 1); bf2Inv(d, q, twg(bb), w1, mulI(w1)); }""")

io.open('work/mul/v10b.cpp', 'w', encoding='utf-8').write(src)
print('v10b.cpp written, %d bytes' % len(src))
