#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
mk_v12.py : v11 -> v12   FFT 长度计算的两个实现缺陷修正

缺陷 1：ceil 写错
    coeffs = u * 64 / k + 1          <-- k 整除 u*64 时多算 1 个系数
    正确    coeffs = (u * 64 + k - 1) / k

缺陷 2：next_pow2 写错
    lm = 2u << (31 - clz(coeffs))    <-- coeffs 恰为 2 的幂时结果翻倍
    正确    lm = is_pow2(coeffs) ? coeffs : (2u << (31 - clz(coeffs)))

两个缺陷叠加后，当 u*64/k 恰为 2 的幂时，FFT 长度整整多用一倍。
命中点（k 由 gk 表定）：
    u = 76, 152, 304, 576, 1152, 2176, 4352, 8192, 16384, 30720, 61440, 114688
每个命中点省 50% FFT 工作量。

其中 u=8192（la=lb=65536 hex）正是 VM 扫描里吞吐掉到 357 hexPerUs 的那个深谷
（相邻档位是 486）。

零精度风险：修正后 lm >= coeffs 依然严格成立，只是不再多要一倍。
"""
import io
import sys

src = sys.argv[1] if len(sys.argv) > 1 else "work/mul/v11.cpp"
dst = sys.argv[2] if len(sys.argv) > 2 else "work/mul/v12.cpp"
s = io.open(src, encoding="utf-8").read()

HELPER = """// FFT 实系数槽位数: 严格 ceil + 严格 next_pow2。
// 旧实现 (u*64/k + 1) 与 (2u << (31-clz(c))) 在 c 恰为 2 的幂时会白白多要一倍长度。
static inline u32 fft_len_for(size_t u, int k) {
    const u32 c = (u32)((u * 64 + (size_t)k - 1) / (size_t)k);
    return (c & (c - 1)) ? (2u << (31 - __builtin_clz(c))) : c;
}
static inline int pick_k(size_t u) {"""
assert s.count("static inline int pick_k(size_t u) {") == 1
s = s.replace("static inline int pick_k(size_t u) {", HELPER)

# mul_fft
OLD1 = """    const u32 coeffs = (u32)(u * 64 / k) + 1;
    const u32 lm = 2u << (31 - __builtin_clz(coeffs));
    const bool same = (a == b) && (na == nb);"""
NEW1 = """    const u32 lm = fft_len_for(u, k);
    const bool same = (a == b) && (na == nb);"""
assert s.count(OLD1) == 1, "mul_fft len anchor"
s = s.replace(OLD1, NEW1)

# mul_unbal
OLD2 = """    const u32 coeffs = (u32)(u2 * 64 / k) + 1;
    const u32 lm = 2u << (31 - __builtin_clz(coeffs));
    const u32 ts = lm >> 1;"""
NEW2 = """    const u32 lm = fft_len_for(u2, k);
    const u32 ts = lm >> 1;"""
assert s.count(OLD2) == 1, "mul_unbal len anchor"
s = s.replace(OLD2, NEW2)

io.open(dst, "w", encoding="utf-8", newline="\n").write(s)
print("wrote", dst, len(s))
