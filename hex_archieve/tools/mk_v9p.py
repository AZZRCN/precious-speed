#!/usr/bin/env python3
"""Generate work/ref/v9p.cpp = v9.cpp + segment timing instrumentation."""
import io, os, sys

SRC = r'D:\hex_precious_speed\work\mul\v9.cpp'
DST = r'D:\hex_precious_speed\work\ref\v9p.cpp'

s = io.open(SRC, encoding='utf-8').read()

# 1) header + timer globals, inserted right before "#define FFT_LEAF_LOG"
TIMER = r'''
// ==================== instrumentation (NOT for submission) ====================
#include <chrono>
#include <cstdio>
static double T_read, T_parse, T_split, T_dif, T_pw, T_dit, T_merge, T_emit, T_write;
using Clk = std::chrono::steady_clock;
static Clk::time_point g_t0;
#define PB() g_t0 = Clk::now()
#define PE(acc) acc += std::chrono::duration<double, std::milli>(Clk::now() - g_t0).count()
// =============================================================================

'''
anchor = '#define FFT_LEAF_LOG'
assert anchor in s
s = s.replace(anchor, TIMER + anchor, 1)

# 2) instrument mul_fft
old = '''    const size_t ta = split_b2(a, FB, na, k, deep ? half : lm);'''
new = '''    PB();
    const size_t ta = split_b2(a, FB, na, k, deep ? half : lm);'''
assert old in s
s = s.replace(old, new, 1)

old = '''    fft::resize(ts);
    if (hiA) fft::difRecZeroHi((fft::cpx*)FB, ts); else fft::difRec((fft::cpx*)FB, ts, 0);
    if (same) {
        fft::pointwiseSq((fft::cpx*)FB, ts);
    } else {
        if (hiB) fft::difRecZeroHi((fft::cpx*)GB, ts); else fft::difRec((fft::cpx*)GB, ts, 0);
        fft::pointwise((fft::cpx*)FB, (fft::cpx*)GB, ts);
    }
    fft::ditRec((fft::cpx*)FB, ts, 0);
    merge_b2(c, FB, u, k);'''
new = '''    PE(T_split);
    fft::resize(ts);
    PB();
    if (hiA) fft::difRecZeroHi((fft::cpx*)FB, ts); else fft::difRec((fft::cpx*)FB, ts, 0);
    if (!same) { if (hiB) fft::difRecZeroHi((fft::cpx*)GB, ts); else fft::difRec((fft::cpx*)GB, ts, 0); }
    PE(T_dif);
    PB();
    if (same) fft::pointwiseSq((fft::cpx*)FB, ts);
    else      fft::pointwise((fft::cpx*)FB, (fft::cpx*)GB, ts);
    PE(T_pw);
    PB();
    fft::ditRec((fft::cpx*)FB, ts, 0);
    PE(T_dit);
    PB();
    merge_b2(c, FB, u, k);
    PE(T_merge);'''
assert old in s
s = s.replace(old, new, 1)

# 3) instrument main: read
old = '''    int len = 0;
    for (;;) {
        long r = read(0, inbuf + len, INCAP - len);'''
new = '''    int len = 0;
    PB();
    for (;;) {
        long r = read(0, inbuf + len, INCAP - len);'''
assert old in s
s = s.replace(old, new, 1)

old = '''    std::memset(inbuf + len, 0, 96);
    inbuf[len] = '\\n';'''
new = '''    PE(T_read);
    std::memset(inbuf + len, 0, 96);
    inbuf[len] = '\\n';'''
assert old in s
s = s.replace(old, new, 1)

# 4) parse_limbs timing
old = '''        int na = parse_limbs(a0, la, A);
        int nb = parse_limbs(b0, lb, B);'''
new = '''        PB();
        int na = parse_limbs(a0, la, A);
        int nb = parse_limbs(b0, lb, B);
        PE(T_parse);'''
assert old in s
s = s.replace(old, new, 1)

# 5) emit timing
old = '''        out = emit(out, Rr, ma + mb, neg);'''
new = '''        PB();
        out = emit(out, Rr, ma + mb, neg);
        PE(T_emit);'''
assert old in s
s = s.replace(old, new, 1)

# 6) write timing + report
old = '''    long off = 0, n = out - outbuf;
    while (off < n) {
        long w = write(1, outbuf + off, n - off);
        if (w <= 0) break;
        off += w;
    }'''
new = '''    PB();
    long off = 0, n = out - outbuf;
    while (off < n) {
        long w = write(1, outbuf + off, n - off);
        if (w <= 0) break;
        off += w;
    }
    PE(T_write);
    {
        double fftcore = T_dif + T_pw + T_dit;
        double mulall  = T_split + fftcore + T_merge;
        double total   = T_read + T_parse + mulall + T_emit + T_write;
        std::fprintf(stderr,
            "V9P read=%.2f parse=%.2f split=%.2f dif=%.2f pw=%.2f dit=%.2f "
            "merge=%.2f emit=%.2f write=%.2f | FFTCORE=%.2f MULALL=%.2f TOTAL=%.2f\\n",
            T_read, T_parse, T_split, T_dif, T_pw, T_dit,
            T_merge, T_emit, T_write, fftcore, mulall, total);
    }'''
assert old in s
s = s.replace(old, new, 1)

os.makedirs(os.path.dirname(DST), exist_ok=True)
io.open(DST, 'w', encoding='utf-8', newline='\n').write(s)
print('wrote', DST, len(s), 'bytes')
