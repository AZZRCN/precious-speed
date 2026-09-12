#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
mk_v11.py : v10c -> v11  不平衡乘法分块 (Unbalanced split with DIF cache reuse)

原理
----
当 ma >> mb 时，v10c 会按 (ma+mb) 的规模选 FFT 长度，长度 = next_pow2 后可能远大于必要。
例：ma=4096 limb, mb=256 limb → FFT 覆盖 4352 limb；而真正的"信息量"只有 256 limb 宽。
拆成 ceil(ma/mb) 个 mb×mb 乘法后，每块 FFT 只需覆盖 512 limb，
总数据流量 = ceil(ma/mb) * 512 vs 原 8192，且每块都能塞进 L2。

关键优化：DIF(pb) 只做一次。所有块的 FFT 长度相同（因为 take <= mb，
结果长度 take+mb <= 2*mb 恒定），故 GB 中的 DIF(pb) 可被全部块复用。
pointwise(FB, GB) 只写 FB 不写 GB —— 这是复用成立的前提。

阈值（可用 -D 覆盖，便于 VM 上做参数扫描）
  UNBAL_MIN_LIMB   小操作数至少这么多 limb 才启用     默认 192
  UNBAL_RATIO_N/D  ma*D >= mb*N 才启用（默认 2/1）
"""
import io
import sys

src_path = sys.argv[1] if len(sys.argv) > 1 else "work/mul/v10c.cpp"
dst_path = sys.argv[2] if len(sys.argv) > 2 else "work/mul/v11.cpp"

s = io.open(src_path, encoding="utf-8").read()

# ---------------------------------------------------------------- 1. merge_add_b2
OLD_MERGE_ANCHOR = "static inline int pick_k(size_t u) {"
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
static inline int pick_k(size_t u) {"""
assert s.count(OLD_MERGE_ANCHOR) == 1, "pick_k anchor"
s = s.replace(OLD_MERGE_ANCHOR, NEW_MERGE)

# ---------------------------------------------------------------- 2. mul_unbal
OLD_FFT_ANCHOR = "// ============================ output ============================"
NEW_UNBAL = r"""// ==================== unbalanced split multiply ====================
#ifndef UNBAL_MIN_LIMB
#define UNBAL_MIN_LIMB 192
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
    const u32 coeffs = (u32)(u2 * 64 / k) + 1;
    const u32 lm = 2u << (31 - __builtin_clz(coeffs));
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

// ============================ output ============================"""
assert s.count(OLD_FFT_ANCHOR) == 1, "output anchor"
s = s.replace(OLD_FFT_ANCHOR, NEW_UNBAL)

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
