# -*- coding: utf-8 -*-
"""D39 = D38 + 分段并行进位传播 (carryPropSeg).

动机: FFT 后的 base-10^4 进位传播是纯串行依赖链 (s_i -> q_i -> s_{i+1}),
      8-way 展开只帮调度、打不破依赖。lri_00 上 fftMulPre + fftMulModBm1Pre
      的 self = 25.46M Ir (10%), 但因 latency-bound, 实际 cycles 占比更高。
做法: 把 [0,n) 拆成 4 段各自独立传播 (carry_in=0) -> 4 条独立链交错执行,
      再对 3 个段边界做涟漪修正 (进位 c_k <= ~1.1e8, 每次 3 步内收敛)。
      语义与串行链逐位等价。
覆盖: fftMul / absSqr路径 / fftMulPre / fftMulModBm1 / fftMulModBm1Pre 共 5 处。

注意: 源文件是 CRLF, 必须先归一化再匹配, 写回时还原。
"""
import re, io, sys, os

SRC = r'D:\precious_speed\best\div_D38.cpp'
DST = r'D:\precious_speed\best\div_D39.cpp'

raw = io.open(SRC, 'r', encoding='utf-8', newline='').read()
crlf = '\r\n' in raw
s = raw.replace('\r\n', '\n')
print('src bytes=%d  crlf=%s' % (len(raw), crlf))

# ---------- 1) 插入 helper ----------
ANCHOR = '        static void fftMul(View in1, View in2, Span out)\n'
assert s.count(ANCHOR) == 1, 'anchor count=%d' % s.count(ANCHOR)

HELPER = r'''        // ================= D39: 分段并行进位传播 =================
        // 原写法是一条串行链: s_i = carry_{i-1} + round(v[i]); carry_i = s_i / BASE。
        // 每步含一次 128 位乘 (divBASE), 依赖延迟 ~5-6 cycle, 8-way 展开也打不破,
        // 整条链是 latency-bound。
        //
        // 这里把 [0,n) 均分 4 段, 每段以 carry_in = 0 独立传播 -> 4 条互不依赖的
        // 链交错发射, 吞吐提升到接近 4x; 段末残留进位 c0/c1/c2 再逐个涟漪进下一段。
        //
        // 正确性: 记 X = sum_i round(v[i]) * BASE^i。串行链输出的是 X 的低 n 位
        // base-BASE 表示 + 溢出进位。分段版对第 k 段算出 X_k 的低 seg 位与进位 c_k,
        // 由于 X = sum_k X_k * BASE^(k*seg) + tail, 把 c_k 加回位置 (k+1)*seg 即还原;
        // 且必须按 低->高 顺序注入 (c0 涟漪出段1 的溢出要并入 c1 再进段2), 下方即如此。
        // c_k <= s_max / BASE ~ 1.1e8, 涟漪 while 3 步内即归零 (out[p] < BASE),
        // 最坏 (全 BASE-1) 才穿段, 代码也已正确处理。
        static uint64_t carryPropSeg(const double *v, Limb *out, size_t n)
        {
            constexpr size_t MIN_PAR = 2048;   // 小规模并行化不划算, 走原串行路径
            uint64_t carry = 0;
            size_t i = 0;
            if (n >= MIN_PAR)
            {
                const size_t seg = (n >> 2) & ~size_t(7);
                const size_t b1 = seg, b2 = seg * 2, b3 = seg * 3;
                uint64_t c0 = 0, c1 = 0, c2 = 0, c3 = 0;
                for (size_t k = 0; k < seg; ++k)
                {
                    if ((k & 7) == 0)
                    {
                        HINT_PREFETCH(v + k + 32, 0, 0);
                        HINT_PREFETCH(v + b1 + k + 32, 0, 0);
                        HINT_PREFETCH(v + b2 + k + 32, 0, 0);
                        HINT_PREFETCH(v + b3 + k + 32, 0, 0);
                    }
                    uint64_t s0 = c0 + uint64_t(v[k]      + 0.5);
                    uint64_t s1 = c1 + uint64_t(v[b1 + k] + 0.5);
                    uint64_t s2 = c2 + uint64_t(v[b2 + k] + 0.5);
                    uint64_t s3 = c3 + uint64_t(v[b3 + k] + 0.5);
                    uint64_t q0 = divBASE(s0);
                    uint64_t q1 = divBASE(s1);
                    uint64_t q2 = divBASE(s2);
                    uint64_t q3 = divBASE(s3);
                    out[k]      = Limb(s0 - q0 * BASE);
                    out[b1 + k] = Limb(s1 - q1 * BASE);
                    out[b2 + k] = Limb(s2 - q2 * BASE);
                    out[b3 + k] = Limb(s3 - q3 * BASE);
                    c0 = q0; c1 = q1; c2 = q2; c3 = q3;
                }
                // 尾部 [4*seg, n) 紧接段 3, 用 c3 继续串行
                carry = c3;
                for (i = b3 + seg; i < n; ++i)
                {
                    carry += uint64_t(v[i] + 0.5);
                    uint64_t q = divBASE(carry);
                    out[i] = Limb(carry - q * BASE);
                    carry = q;
                }
                // 段边界涟漪 (低 -> 高)
                uint64_t ov = c0;
                for (size_t p = b1; ov > 0 && p < b2; ++p)
                {
                    uint64_t s = uint64_t(out[p]) + ov;
                    uint64_t q = divBASE(s);
                    out[p] = Limb(s - q * BASE);
                    ov = q;
                }
                ov += c1;
                for (size_t p = b2; ov > 0 && p < b3; ++p)
                {
                    uint64_t s = uint64_t(out[p]) + ov;
                    uint64_t q = divBASE(s);
                    out[p] = Limb(s - q * BASE);
                    ov = q;
                }
                ov += c2;
                for (size_t p = b3; ov > 0 && p < n; ++p)
                {
                    uint64_t s = uint64_t(out[p]) + ov;
                    uint64_t q = divBASE(s);
                    out[p] = Limb(s - q * BASE);
                    ov = q;
                }
                return carry + ov;
            }
            for (; i + 7 < n; i += 8)
            {
                HINT_PREFETCH(v + i + 16, 0, 0);
                HINT_PREFETCH(v + i + 24, 0, 0);
                uint64_t s0 = carry + uint64_t(v[i]   + 0.5);
                uint64_t q0 = divBASE(s0);
                uint64_t s1 = q0 + uint64_t(v[i+1] + 0.5);
                uint64_t q1 = divBASE(s1);
                uint64_t s2 = q1 + uint64_t(v[i+2] + 0.5);
                uint64_t q2 = divBASE(s2);
                uint64_t s3 = q2 + uint64_t(v[i+3] + 0.5);
                uint64_t q3 = divBASE(s3);
                uint64_t s4 = q3 + uint64_t(v[i+4] + 0.5);
                uint64_t q4 = divBASE(s4);
                uint64_t s5 = q4 + uint64_t(v[i+5] + 0.5);
                uint64_t q5 = divBASE(s5);
                uint64_t s6 = q5 + uint64_t(v[i+6] + 0.5);
                uint64_t q6 = divBASE(s6);
                uint64_t s7 = q6 + uint64_t(v[i+7] + 0.5);
                uint64_t q7 = divBASE(s7);
                out[i]   = Limb(s0 - q0 * BASE);
                out[i+1] = Limb(s1 - q1 * BASE);
                out[i+2] = Limb(s2 - q2 * BASE);
                out[i+3] = Limb(s3 - q3 * BASE);
                out[i+4] = Limb(s4 - q4 * BASE);
                out[i+5] = Limb(s5 - q5 * BASE);
                out[i+6] = Limb(s6 - q6 * BASE);
                out[i+7] = Limb(s7 - q7 * BASE);
                carry = q7;
            }
            for (; i < n; ++i)
            {
                carry += uint64_t(v[i] + 0.5);
                uint64_t q = divBASE(carry);
                out[i] = Limb(carry - q * BASE);
                carry = q;
            }
            return carry;
        }
'''

s = s.replace(ANCHOR, HELPER + ANCHOR, 1)
print('[1] helper inserted')

# ---------- 2) 替换 5 处串行链 ----------
PAT = re.compile(
    r'uint64_t carry = 0;\n'
    r'[ \t]*size_t i = 0;\n'
    r'[ \t]*for \(; i \+ 7 < (\w+); i \+= 8\)\n'
    r'[ \t]*\{.*?\n'
    r'[ \t]*carry = q7;\n'
    r'[ \t]*\}\n'
    r'[ \t]*for \(; i < \1; i\+\+\)\n'
    r'[ \t]*\{\n'
    r'[ \t]*carry \+= uint64_t\((\w+)\[i\] \+ 0\.5\);\n'
    r'[ \t]*uint64_t q = divBASE\(carry\);\n'
    r'[ \t]*out\[i\] = Limb\(carry - q \* BASE\);\n'
    r'[ \t]*carry = q;\n'
    r'[ \t]*\}',
    re.S)

hits = list(PAT.finditer(s))
print('[2] serial-chain sites found = %d' % len(hits))
for h in hits:
    print('    len=%-10s arr=%-4s  @off %d' % (h.group(1), h.group(2), h.start()))
assert len(hits) == 5, 'expect 5 sites, got %d' % len(hits)

s = PAT.sub(lambda m: 'uint64_t carry = carryPropSeg(%s, out.ptr, %s);'
            % (m.group(2), m.group(1)), s)
print('[3] replaced')

# ---------- 3) 自检 ----------
assert s.count('carryPropSeg(') == 6, 'carryPropSeg refs = %d' % s.count('carryPropSeg(')
assert 'carry = q7;' in s, 'fallback path lost'
assert s.count('carry = q7;') == 1, 'leftover serial chains = %d' % s.count('carry = q7;')

out = s.replace('\n', '\r\n') if crlf else s
io.open(DST, 'w', encoding='utf-8', newline='').write(out)
print('[4] wrote %s  (%d bytes, was %d)' % (DST, len(out), len(raw)))
