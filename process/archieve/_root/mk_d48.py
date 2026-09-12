#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""mk_d48.py —— 由 best/div_D47.cpp 生成 best/div_D48.cpp

D48 = D47 + 超优化器(superopt) 搜出的 SWAR 进位/借位链恒等式 (7 个站点)。

--------------------------------------------------------------------------
超优化器结论 (cand_pool/swar_carry16.json, superopt_results.json = VERIFIED):
    cout = shr(xor(add(P,cin), lea2(P,G)), 1)        uops=4  cyc=3
--------------------------------------------------------------------------

恒等式 (前提: G & P == 0 恒成立 —— G = "t > BASE-1", P = "t == BASE-1", 互斥):

    旧 (D47, 8 条):
        A = G;  B = G | P;
        s   = A + B + c;          // = 2G + P + c
        cin = s ^ A ^ B;          // bit j = 进入 limb j 的进位
        cout= cin >> 1;
        c'  = (s >> 16) & 1;

    新 (D48, 6 条):
        z    = P + 2G;            // 与 c 无关 -> **不在关键路径上**, 可提前算
        cin  = (P + c) ^ z;
        cout = cin >> 1;
        c'   = (z + c) >> 16;     // z + c 恒等于旧的 s

关键: **c 是循环携带依赖 (loop-carried)**, 决定内层循环的最短周期。
    旧关键路径: c -> add(1) -> shr16(1) -> and1(1)   = 3 cyc
    新关键路径: c -> add(z+c)(1) -> shr16(1)         = 2 cyc     ← -1 拍
    (若图省事写成 c' = cout>>15 会变 4 拍, 反而更慢 —— 踩过这个坑)
    `& 1` 可省: G,P 互斥 16 位 => z = 2G+P <= 0x1FFFE => (z+c)>>16 天然 ∈ {0,1}

数值验证 (2026-08-07 实跑):
    nbits=12 **全域穷举** 所有不相交 (G,P) 对 × c∈{0,1} = 1,062,882 组
        -> cin 向量逐位相等 + c' 相等, 不符 = 0, max z = 0x1FFE = 2^(nb+1)-2 ✓
    nbits=16 随机 300,000 组 -> 不符 = 0
    整程序: local_verify.py D48 D47 -> 26 官方例 + 400 随机 fuzz 逐字节一致

7 个站点 (最初只数到 5 个, 漏了 A2/B2 那两处):
    site1 absAdd_avx2      site2 absSub_avx2       site3 divmod 主循环
    site4 乘减 a           site5 乘减 b (A2/B2)
    site6 乘减尾 a         site7 乘减尾 b (A2/B2)
    site6/site7 位于提前 return 的尾块, 原本就不更新进位变量 -> 保持不动。
"""
import os
import re
import sys

PS = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(PS, "best", "div_D47.cpp")
DST = os.path.join(PS, "best", "div_D48.cpp")

N1 = "// [D48/SO] z=P+2G 离开关键路径; cin=(P+c)^z 与 s^A^B 恒等 (G&P==0); 8->6 条, 环路 3->2 拍"

# 顺序敏感: site4 是 site6 的超集, site5 是 site7 的超集, 必须先替换长的。
REPL = [
    # ---- site1  absAdd_avx2 ----
    ("""                uint32_t A = G, B = G | P;
                uint32_t s = A + B + carry32;
                uint32_t cin = s ^ A ^ B; // bit j = 进入 limb j 的进位; bit16 = 总进位
                uint32_t cout = cin >> 1; // bit j = limb j 的进位输出
                carry32 = (s >> 16) & 1u;""",
     """                %s
                uint32_t z = P + 2u * G;
                uint32_t cin = (P + carry32) ^ z; // bit j = 进入 limb j 的进位
                uint32_t cout = cin >> 1;         // bit j = limb j 的进位输出
                carry32 = (z + carry32) >> 16;    // == 旧 (s>>16)&1, 环路仅 2 拍""" % N1),

    # ---- site2  absSub_avx2 ----
    ("""                uint32_t A = G, B = G | P;
                uint32_t s = A + B + borrow;
                uint32_t cin = s ^ A ^ B; // bit j = 进入 limb j 的借位; bit16 = 总借出
                uint32_t cout = cin >> 1; // bit j = limb j 的借出 (bit15 取自 cin.bit16)
                borrow = (s >> 16) & 1u;""",
     """                %s
                uint32_t z = P + 2u * G;
                uint32_t cin = (P + borrow) ^ z; // bit j = 进入 limb j 的借位
                uint32_t cout = cin >> 1;        // bit j = limb j 的借出
                borrow = (z + borrow) >> 16;     // == 旧 (s>>16)&1, 环路仅 2 拍""" % N1),

    # ---- site3  divmod 主循环 ----
    ("""                    uint32_t A = G, B = G | P;
                    uint32_t sw = A + B + carry_rip;
                    uint32_t cin = sw ^ A ^ B; // bit j = 进入 limb j 的进位
                    uint32_t cout = cin >> 1;  // bit j = limb j 的进位输出
                    carry_rip = (sw >> 16) & 1u;""",
     """                    %s
                    uint32_t z = P + 2u * G;
                    uint32_t cin = (P + carry_rip) ^ z;
                    uint32_t cout = cin >> 1;
                    carry_rip = (z + carry_rip) >> 16;""" % N1),

    # ---- site4  乘减 a (含 carry_rip 更新) ----
    ("""                    uint32_t A = G, B = G | P;
                    uint32_t sw = A + B + carry_rip;
                    uint32_t cin = sw ^ A ^ B;
                    uint32_t cout = cin >> 1;
                    carry_rip = (sw >> 16) & 1u;""",
     """                    %s
                    uint32_t z = P + 2u * G;
                    uint32_t cin = (P + carry_rip) ^ z;
                    uint32_t cout = cin >> 1;
                    carry_rip = (z + carry_rip) >> 16;""" % N1),

    # ---- site5  乘减 b, A2/B2 (含 borrow 更新) ----
    ("""                    uint32_t A2 = G2, B2 = G2 | P2;
                    uint32_t s2 = A2 + B2 + borrow;
                    uint32_t cin2 = s2 ^ A2 ^ B2;
                    uint32_t cout2 = cin2 >> 1;
                    borrow = (s2 >> 16) & 1u;""",
     """                    %s
                    uint32_t z2 = P2 + 2u * G2;
                    uint32_t cin2 = (P2 + borrow) ^ z2;
                    uint32_t cout2 = cin2 >> 1;
                    borrow = (z2 + borrow) >> 16;""" % N1),

    # ---- site6  乘减尾 a (无 carry_rip 更新, 提前 return) ----
    ("""                    uint32_t A = G, B = G | P;
                    uint32_t sw = A + B + carry_rip;
                    uint32_t cin = sw ^ A ^ B;
                    uint32_t cout = cin >> 1;""",
     """                    %s
                    uint32_t cin = (P + carry_rip) ^ (P + 2u * G);
                    uint32_t cout = cin >> 1;""" % N1),

    # ---- site7  乘减尾 b, A2/B2 (borrow 用 (cin2>>(rem+1))&1, 原样保留) ----
    ("""                    uint32_t A2 = G2, B2 = G2 | P2;
                    uint32_t s2 = A2 + B2 + borrow;
                    uint32_t cin2 = s2 ^ A2 ^ B2;
                    uint32_t cout2 = cin2 >> 1;""",
     """                    %s
                    uint32_t cin2 = (P2 + borrow) ^ (P2 + 2u * G2);
                    uint32_t cout2 = cin2 >> 1;""" % N1),
]


def main():
    # newline="" => 不做换行转换 (原文件 CRLF), 否则 diff 会整文件飘红
    src = open(SRC, encoding="utf-8", newline="").read()
    crlf = "\r\n" in src
    if crlf:
        src = src.replace("\r\n", "\n")

    for i, (old, new) in enumerate(REPL, 1):
        cnt = src.count(old)
        if cnt != 1:
            print("[FATAL] site%d 匹配 %d 次 (应为 1), 中止" % (i, cnt))
            sys.exit(1)
        src = src.replace(old, new, 1)
        print("[ok] site%d 已替换" % i)

    # 收尾自检: 不应再残留任何旧式借位链 (纯注释行不算)
    leftover = [ln for ln in src.splitlines()
                if re.search(r"\^ A2? \^ B2?", ln) and not ln.lstrip().startswith("//")]
    if leftover:
        print("[FATAL] 仍残留 %d 处旧式 s^A^B:" % len(leftover))
        for ln in leftover:
            print("   ", ln.strip())
        sys.exit(1)

    out = src.replace("\n", "\r\n") if crlf else src
    open(DST, "w", encoding="utf-8", newline="").write(out)
    print("\n生成 %s  (%d 站点, 每站 8->6 条 + 环路 3->2 拍, 换行=%s)"
          % (DST, len(REPL), "CRLF" if crlf else "LF"))


if __name__ == "__main__":
    main()
