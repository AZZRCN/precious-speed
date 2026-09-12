#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# AZZRCN
# https://github.com/AZZRCN
"""
mk_v14.py : v13 -> v14   分梯度调度 (graded dispatch) + 分块 schoolbook

两处改动, 可用 -D 开关独立开合, 便于拆分归因:

[1] 分块 schoolbook (mul_bf)
    旧实现外层遍历 b、内层遍历整个 a, 每个 i 都完整刷一遍 c[0..na+nb)。
    na 很大时 (large x small 测试点) c 远超 L2, nb 趟 = nb*(na+nb)*8 字节访存。
      例 na=100000 limb, nb=48 -> 48 * 800KB = 38MB 流量, 全是 L2/L3 miss。
    新实现按 a 分块 (BF_BLK limb 一块), 每块内跑完整个 b, c 的活动窗口
    只有 (BF_BLK+nb)*8 字节, 常驻 L1。总流量降到 a、c 各一趟。
    BF_BLK >= na 时行为退化为旧实现 (只多一次 carry 分支), 因此对小规模零影响。

[2] 分梯度阈值 (use_bf)
    旧调度是硬阈值 `mb <= 48`, 与 ma 无关。
    但两条路的代价量纲不同:
        schoolbook ~ ma * mb          (64x64 乘加次数)
        FFT        ~ C * u * log2(u)  (u = ma + mb)
    所以交叉点 mb* ~ C * log2(ma) 应随 ma 增长, 固定 48 在大 ma 侧过早切 FFT。
    新调度:
        mb <= BF_HARD                      -> 无条件 schoolbook (保持旧行为)
        BF_HARD < mb <= BF_CAP 且 代价占优 -> schoolbook
        其余                                -> FFT
    BF_C = 0 时第二条永不成立, 即完全等价旧调度 —— 默认值就是 0,
    这样 v14(默认) 与 v13 的唯一差异就是 [1], 归因干净。

编译期旋钮:
    -DBF_BLK=2048   a 的分块大小 (limb)
    -DBF_HARD=48    无条件 schoolbook 的短边上限
    -DBF_C=0        梯度系数, 0 = 关闭梯度
    -DBF_CAP=512    schoolbook 短边硬上限 (防极端退化)
"""
import io
import sys

src = sys.argv[1] if len(sys.argv) > 1 else "work/mul/v13.cpp"
dst = sys.argv[2] if len(sys.argv) > 2 else "work/mul/v14.cpp"
s = io.open(src, encoding="utf-8").read()

OLD_BF = """static void mul_bf(const u64* a, int na, const u64* b, int nb, u64* c) {
    std::memset(c, 0, (size_t)(na + nb) * 8);
    for (int i = 0; i < nb; ++i) {
        u64 carry = 0, bi = b[i];
        for (int j = 0; j < na; ++j) {
            u128 t = (u128)a[j] * bi + c[i + j] + carry;
            c[i + j] = (u64)t;
            carry = (u64)(t >> 64);
        }
        c[i + na] = carry;
    }
}"""

NEW_BF = """#ifndef BF_BLK
#define BF_BLK 2048
#endif
#ifndef BF_HARD
#define BF_HARD 48
#endif
#ifndef BF_C
#define BF_C 0
#endif
#ifndef BF_CAP
#define BF_CAP 512
#endif
// 分块 schoolbook: 按 a 切块, 让 c 的活动窗口常驻 L1。
// 块外进位用一次短 ripple 收掉; 块大小 >= na 时与朴素实现完全等价。
static void mul_bf(const u64* a, int na, const u64* b, int nb, u64* c) {
    std::memset(c, 0, (size_t)(na + nb) * 8);
    for (int j0 = 0; j0 < na; j0 += BF_BLK) {
        const int jn = (na - j0 < BF_BLK) ? (na - j0) : BF_BLK;
        const u64* aa = a + j0;
        for (int i = 0; i < nb; ++i) {
            const u64 bi = b[i];
            u64* cc = c + (size_t)i + j0;
            u64 carry = 0;
            for (int j = 0; j < jn; ++j) {
                u128 t = (u128)aa[j] * bi + cc[j] + carry;
                cc[j] = (u64)t;
                carry = (u64)(t >> 64);
            }
            for (int p = jn; carry; ++p) {
                u128 t = (u128)cc[p] + carry;
                cc[p] = (u64)t;
                carry = (u64)(t >> 64);
            }
        }
    }
}
// 分梯度调度判据: schoolbook 代价 ma*mb 对比 FFT 代价 BF_C*u*log2(u)。
static inline bool use_bf(size_t ma, size_t mb) {
    if (mb <= (size_t)BF_HARD) return true;
#if BF_C > 0
    if (mb > (size_t)BF_CAP) return false;
    const size_t u = ma + mb;
    const size_t lg = (size_t)(64 - __builtin_clzll(u));
    return ma * mb <= (size_t)BF_C * u * lg;
#else
    return false;
#endif
}"""

assert s.count(OLD_BF) == 1, "mul_bf anchor"
s = s.replace(OLD_BF, NEW_BF)

OLD_D = """        if (mb <= 48) mul_bf(pa, ma, pb, mb, Rr);
        else mul_fft(pa, ma, pb, mb, Rr);"""
NEW_D = """        if (use_bf((size_t)ma, (size_t)mb)) mul_bf(pa, ma, pb, mb, Rr);
        else mul_fft(pa, ma, pb, mb, Rr);"""
assert s.count(OLD_D) == 1, "dispatch anchor"
s = s.replace(OLD_D, NEW_D)

io.open(dst, "w", encoding="utf-8", newline="\n").write(s)
print("wrote", dst, len(s))
