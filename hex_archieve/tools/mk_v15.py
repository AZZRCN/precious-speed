#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# AZZRCN
# https://github.com/AZZRCN
"""
mk_v15.py : v13 -> v15   不平衡分块 (调好阈值的版本)

背景 / 为什么阈值从 192 改成 4096
--------------------------------
v11 用 UNBAL_MIN_LIMB=192, 在低噪声协议下重测:
    large_small_00  30.21 -> 22.65 ms   (0.7498, -25%)   <- 真实大赢
    large_00        16.65 -> 18.62 ms   (1.1179, +12%)   <- 真实回归
    large_01        17.46 -> 18.84 ms   (1.0794,  +8%)
    large_02        17.70 -> 18.35 ms   (1.0367,  +4%)

看官方测试点的形状就明白了:
    large_small_00 : T=2,  两个 case 都是 112500 x 12500 limb (ratio 9.00)
    large_00/01/02 : T~38, ratio 1~25 的混合, 但 ratio>=2 的 case 里
                     短边 mb 只有 200~1600 limb

分块的每块固定开销 (32KB 级 memset + split_b2 + DIT + merge 进位链) 与
块数成正比; mb 小的时候块数多、每块 FFT 又小, 固定开销压倒长度收益。
所以判据不能只看 ratio, 必须给短边一个足够大的下限。

阈值取 4096 的依据 (不是拍脑袋):
    large_* 每个 case 的总 limb <= ~7300。
    若要同时满足 mb >= 4096 且 ma >= 2*mb, 则 ma+mb >= 12288 > 7300，
    **数学上不可能**。也就是说 4096 这个下限能把 large_* 完全排除在外,
    同时 large_small_00 的 mb=12500 仍然命中。
    (DEC 参考实现的 UnbalancedThreshold 恰好也是 4096。)

预期: large_small_00 -25% 且 large_* 零回归 => 总分 -1.6% 左右。

旋钮:
    -DUNBAL_MIN_LIMB=4096
    -DUNBAL_RATIO_N=2 -DUNBAL_RATIO_D=1     (ma*D >= mb*N 才启用)
"""
import io
import sys

src_path = sys.argv[1] if len(sys.argv) > 1 else "work/mul/v13.cpp"
dst_path = sys.argv[2] if len(sys.argv) > 2 else "work/mul/v15.cpp"

s = io.open(src_path, encoding="utf-8").read()

# ---------------------------------------------------------------- 1. merge_add_b2
ANCHOR1 = "// FFT 实系数槽位数: 严格 ceil + 严格 next_pow2。"
NEW_MERGE = r"""// merge + 累加进位版: 用于不平衡分块, 把一块的结果加到 f[] 上而非覆盖。
static void merge_add_b2(u64* f, const double* g, size_t n, int k) {
    size_t i = 0, j = 0;
    int w = 0;
    u128 tmp = 0;
    u64 carry = 0;
    while (i < n) {
        while (w < 64) { tmp += (u128)(u64)(int64_t)(g[j++] + 0.5) << w; w += k; }
        const u64 v = (u64)tmp;
        tmp >>= 64, w -= 64;
        const u128 sacc = (u128)f[i] + v + carry;
        f[i++] = (u64)sacc;
        carry = (u64)(sacc >> 64);
    }
    while (carry) {                       // 进位向高位传播
        const u128 sacc = (u128)f[i] + carry;
        f[i++] = (u64)sacc;
        carry = (u64)(sacc >> 64);
    }
}
""" + ANCHOR1
assert s.count(ANCHOR1) == 1, "fft_len_for anchor"
s = s.replace(ANCHOR1, NEW_MERGE)

# ---------------------------------------------------------------- 2. mul_unbal
ANCHOR2 = "// ============================ output ============================"
NEW_UNBAL = r"""// ==================== unbalanced split multiply ====================
#ifndef UNBAL_MIN_LIMB
#define UNBAL_MIN_LIMB 4096
#endif
#ifndef UNBAL_RATIO_N
#define UNBAL_RATIO_N 2
#endif
#ifndef UNBAL_RATIO_D
#define UNBAL_RATIO_D 1
#endif

// ma >> mb: 把长边切成 mb 长的块, 逐块与短边做 mb x mb FFT 乘并累加。
// FFT 长度由 (ma+mb) 降到 2*mb, 数据流量与 cache 压力同步下降。
// DIF(pb) 只算一次存 GB, 所有块复用 (pointwise 不写 GB)。
static void mul_unbal(const u64* pa, int ma, const u64* pb, int mb, u64* c) {
    const size_t u2 = (size_t)mb * 2;
    const int k = pick_k(u2);
    const u32 lm = fft_len_for(u2, k);
    const u32 ts = lm >> 1;
    const bool deep = ts > (1u << FFT_LEAF_LOG);
    const size_t half = lm >> 1;

    // ---- DIF(pb) 一次性, 缓存在 GB ----
    const size_t tb = split_b2(pb, GB, (size_t)mb, k, deep ? half : lm);
    const bool hiB = deep && tb <= half;
    if (deep && !hiB && tb < lm) std::memset(GB + tb, 0, (lm - tb) * 8);
    fft::resize(ts);
    if (hiB) fft::difRecZeroHi((fft::cpx*)GB, ts); else fft::difRec((fft::cpx*)GB, ts, 0);

    std::memset(c, 0, (size_t)(ma + mb) * 8);
    for (int off = 0; off < ma; off += mb) {
        const int take = (ma - off >= mb) ? mb : (ma - off);
        const size_t ta = split_b2(pa + off, FB, (size_t)take, k, deep ? half : lm);
        const bool hiA = deep && ta <= half;
        if (deep && !hiA && ta < lm) std::memset(FB + ta, 0, (lm - ta) * 8);
        if (hiA) fft::difRecZeroHi((fft::cpx*)FB, ts); else fft::difRec((fft::cpx*)FB, ts, 0);
        fft::pointwise((fft::cpx*)FB, (fft::cpx*)GB, ts);
        fft::ditRec((fft::cpx*)FB, ts, 0);
        merge_add_b2(c + off, FB, (size_t)take + (size_t)mb, k);
    }
}

""" + ANCHOR2
assert s.count(ANCHOR2) == 1, "output anchor"
s = s.replace(ANCHOR2, NEW_UNBAL)

# ---------------------------------------------------------------- 3. 调度
OLD_DISPATCH = """        if (mb <= 48) mul_bf(pa, ma, pb, mb, Rr);
        else mul_fft(pa, ma, pb, mb, Rr);"""
NEW_DISPATCH = """        if (mb <= 48) mul_bf(pa, ma, pb, mb, Rr);
        else if (mb >= UNBAL_MIN_LIMB &&
                 (size_t)ma * UNBAL_RATIO_D >= (size_t)mb * UNBAL_RATIO_N)
            mul_unbal(pa, ma, pb, mb, Rr);
        else mul_fft(pa, ma, pb, mb, Rr);"""
assert s.count(OLD_DISPATCH) == 1, "dispatch anchor"
s = s.replace(OLD_DISPATCH, NEW_DISPATCH)

io.open(dst_path, "w", encoding="utf-8", newline="\n").write(s)
print("wrote %s  (%d bytes)" % (dst_path, len(s)))
