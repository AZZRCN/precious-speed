#!/usr/bin/env python3
# _mkv10.py — 生成 work/div/v10.cpp
# 策略(Option A, 最小变更回溯, 低风险):
#   取 submit_ready/div.cpp (v8, radix-2, AC 61ms 基线), 仅把全尺寸 tw[n>>1] 平表
#   换成 MUL v19 的两级 twbase[1<<12] 表 (twg 查表)。
#   蝴蝶(radix-2)、pointwise(scalar)、mul_fft/fm_mul/除法集成 全部保留 v8 原貌。
# 数学等价性: twg(i) 构造式 == v8 的 tw[i] (同 halfLog/halfSize/同 base 表),
#   故 v10 的 FFT 输出与 v8 逐位相同 => v10 必然正确, 只改 twiddle 缓存布局。
# 副作用: 把 v9 的失败干净隔离到 radix-4 蝴蝶 (v10=radix-2+两级表 能过; v9=radix-4+两级表 失败)。
import re, sys

SRC = 'submit_ready/div.cpp'
OUT = 'work/div/v10.cpp'

src = open(SRC, 'r', encoding='utf-8').read()

# 1) 声明: 全尺寸动态平表 -> 两级 16KiB 常驻表
old_decl = "static cpx* tw = nullptr;\nstatic u32 twlen = 0;\n"
new_decl = ("alignas(64) static __m128d twbase[1u << 12];\n"
            "static u32 twHalfLog = 0, twHalfSize = 0, twHalfMask = 0, twN = 0;\n")
assert old_decl in src, "decl block not found"
src = src.replace(old_decl, new_decl, 1)

# 2) resize: 重建全表 -> 只建 base 表 + 加 twg 两级查表
old_resize = '''static void resize(u32 n) {
    if (n <= (twlen << 1)) return;
    u32 halfLog = (u32)(31 - __builtin_clz(n)) >> 1, halfSize = 1u << halfLog;
    cpx* base = new cpx[(size_t)halfSize << 1];
    const double a0 = std::acos(-1.0) / halfSize, a1 = a0 / halfSize;
    for (u32 i = 0, j = (halfSize * 3) >> 1, p = 0; i != halfSize; p -= halfSize - (j >> __builtin_ctz(++i))) {
        int32_t sp = (int32_t)p;
        std::complex<double> f = std::polar(1.0, sp * a0), s = std::polar(1.0, sp * a1);
        base[i] = _mm_set_pd(f.imag(), f.real());
        base[i | halfSize] = _mm_set_pd(s.imag(), s.real());
    }
    cpx* nf = new cpx[n >> 1];
    if (twlen) std::memcpy(nf, tw, (size_t)twlen * 16);
    delete[] tw;
    tw = nf;
    for (u32 i = twlen; i != (n >> 1); ++i)
        tw[i] = cmul(base[i & (halfSize - 1)], base[halfSize | (i >> halfLog)]);
    delete[] base;
    twlen = n >> 1;
}'''
new_resize = '''static void resize(u32 n) {  // n = 复数点数; 只建 16 KiB 的 base 表
    if (n == twN) return;
    twN = n;
    const u32 halfLog = (u32)(31 - __builtin_clz(n)) >> 1, halfSize = 1u << halfLog;
    twHalfLog = halfLog; twHalfSize = halfSize; twHalfMask = halfSize - 1;
    const double a0 = std::acos(-1.0) / halfSize, a1 = a0 / halfSize;
    for (u32 i = 0, j = (halfSize * 3) >> 1, p = 0; i != halfSize; p -= halfSize - (j >> __builtin_ctz(++i))) {
        int32_t sp = (int32_t)p;
        std::complex<double> f = std::polar(1.0, sp * a0), s = std::polar(1.0, sp * a1);
        twbase[i] = _mm_set_pd(f.imag(), f.real());
        twbase[i | halfSize] = _mm_set_pd(s.imag(), s.real());
    }
}
// 两级查表: 1 次 L1 load(低位) + 1 次 L1 load(高位) + 1 复乘
static inline cpx twg(u32 i) {
    return cmul(twbase[i & twHalfMask], twbase[twHalfSize | (i >> twHalfLog)]);
}'''
assert old_resize in src, "resize block not found"
src = src.replace(old_resize, new_resize, 1)

# 3) 蝴蝶里的 tw[...] -> twg(...)
src = src.replace('tw[base + j]', 'twg(base + j)')
src = src.replace('tw[bb]', 'twg(bb)')
src = src.replace('tw[f >> 1]', 'twg(f >> 1)')

# 完整性自检: 不应残留 tw[ 或 twlen
stray = re.findall(r'\btw\[', src)
assert not stray, f"stray tw[ left: {stray}"
assert 'twlen' not in src, "twlen still present"

open(OUT, 'w', encoding='utf-8').write(src)
print(f"[ok] wrote {OUT} ({len(src)} bytes)")
# 统计替换是否生效
print("[chk] twg occurrences:", src.count('twg('))
print("[chk] twbase decl:", 'twbase[1u << 12]' in src)
