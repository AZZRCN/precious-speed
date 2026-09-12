#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
D49 base-1e16 大基数除法 —— 针对性随机压力用例生成器。

目标: 把 absDivBasicCore 的 big16 分支打满, 覆盖所有会出错的结构性边界。
LC 输入格式: 第一行 T, 之后 T 行 "a b"。

覆盖维度 (每条都是独立的一族):
  A. len2 % 4 各余数      -> pad ∈ {0,1,2,3} 全覆盖 (打包对齐)
  B. len1 % 4 各余数      -> N1 高位补零分支
  C. 精确整除 (r == 0)    -> 余数解包的低 pad limb 必为 0 这条 assert
  D. qhat 估大需修正       -> b 高位 = 5000...0001 之类, 逼 D3 的 v[n-2] 测试
  E. b 顶部 limb 恰为 9999 -> u1 >= vtop 分支 (qhat = BIG16-1)
  F. 余数近零 (r_nearly_zero 仿真) -> a = b*q + tiny
  G. 极窄商 (quot_idx 刚过阈值) 与极宽商
  H. 归一化 factor != 1 的输入 (b 顶位 < 5000) -> 走 divisorNormalizeFactor

用法: python gen_divstress.py <out.in> [seed]
"""
import random
import sys

sys.set_int_max_str_digits(2000000)

# base-1e4 的 limb 数 -> 十进制位数区间
# absDivBasicCore 路由条件: len2 <= 64 或 (len1-len2) <= 64  (单位: limb)
LIMB = 4


def digits_for_limbs(nl, top_lo=1000, top_hi=9999):
    """构造恰好 nl 个 base-1e4 limb 的整数 (最高 limb 在 [top_lo, top_hi])"""
    top = random.randint(top_lo, top_hi)
    rest = [random.randint(0, 9999) for _ in range(nl - 1)]
    v = top
    for x in reversed(rest):
        v = v * 10000 + x
    return v


def with_top_limb(nl, top):
    rest = [random.randint(0, 9999) for _ in range(nl - 1)]
    v = top
    for x in reversed(rest):
        v = v * 10000 + x
    return v


def gen(seed=12345):
    random.seed(seed)
    cases = []

    def add(a, b):
        if b <= 0 or a < 0:
            return
        cases.append((a, b))

    # ---- A/B: len2 % 4 与 len1 % 4 全组合 ----
    for l2mod in range(4):
        for qmod in range(4):
            for base2 in (12, 13, 16, 20, 33, 48, 61, 64):
                l2 = base2 - (base2 % 4) + l2mod
                if l2 < 12:
                    continue
                for qi in (4, 5, 7, 8, 17, 33, 64):
                    q_l = qi - (qi % 4) + qmod
                    if q_l < 4:
                        continue
                    b = digits_for_limbs(l2, 5000, 9999)
                    a = digits_for_limbs(l2 + q_l, 1, 9999)
                    add(a, b)

    # ---- C: 精确整除 ----
    for _ in range(400):
        l2 = random.randint(12, 64)
        q_l = random.randint(4, 64)
        b = digits_for_limbs(l2, 5000, 9999)
        q = digits_for_limbs(q_l, 1, 9999)
        add(b * q, b)

    # ---- D: qhat 估大, 逼 v[n-2] 测试与加回路径 ----
    for _ in range(400):
        l2 = random.randint(12, 64)
        q_l = random.randint(4, 64)
        # b = 5000 0000 ... 0001  (顶 limb 恰为 HALF_BASE, 次高全 0)
        b = 5000 * (10000 ** (l2 - 1)) + random.choice([1, 2, 9999])
        q = digits_for_limbs(q_l, 1, 9999)
        r = random.randint(0, b - 1)
        add(b * q + r, b)

    # ---- E: b 顶 limb = 9999 -> u1 >= vtop 分支 ----
    for _ in range(300):
        l2 = random.randint(12, 64)
        q_l = random.randint(4, 64)
        b = with_top_limb(l2, 9999)
        q = with_top_limb(q_l, 9999)
        r = random.randint(0, b - 1)
        add(b * q + r, b)

    # ---- F: 余数近零 (主战场 r_nearly_zero) ----
    for _ in range(500):
        l2 = random.randint(12, 64)
        q_l = random.randint(4, 64)
        b = digits_for_limbs(l2, 5000, 9999)
        q = digits_for_limbs(q_l, 1, 9999)
        add(b * q + random.randint(0, 3), b)

    # ---- G: 阈值附近 (刚过 / 刚不过) + 极端形状 ----
    for l2 in range(10, 20):
        for q_l in range(2, 10):
            b = digits_for_limbs(l2, 5000, 9999)
            a = digits_for_limbs(l2 + q_l, 1, 9999)
            add(a, b)
    for _ in range(200):
        # 极窄除数 + 极宽商 (quot_idx >> len2)
        l2 = random.randint(12, 20)
        q_l = random.randint(200, 900)
        b = digits_for_limbs(l2, 5000, 9999)
        a = digits_for_limbs(l2 + q_l, 1, 9999)
        add(a, b)
    for _ in range(200):
        # 极宽除数 + 极窄商 (走 len1-len2 <= 64 那支)
        l2 = random.randint(200, 900)
        q_l = random.randint(4, 60)
        b = digits_for_limbs(l2, 5000, 9999)
        a = digits_for_limbs(l2 + q_l, 1, 9999)
        add(a, b)

    # ---- H: b 顶 limb < 5000 -> 触发 divisorNormalizeFactor (factor != 1) ----
    for _ in range(400):
        l2 = random.randint(12, 64)
        q_l = random.randint(4, 64)
        b = digits_for_limbs(l2, 1, 4999)
        a = digits_for_limbs(l2 + q_l, 1, 9999)
        add(a, b)
    # 顶 limb 极小 (1..9) -> shift 最大
    for _ in range(200):
        l2 = random.randint(12, 64)
        q_l = random.randint(4, 64)
        b = with_top_limb(l2, random.randint(1, 9))
        a = digits_for_limbs(l2 + q_l, 1, 9999)
        add(a, b)

    # ---- I: 触发 absInvNewton 基例 (k<=64) 需要更大规模走 Newton 路径 ----
    for _ in range(30):
        l2 = random.randint(300, 800)
        q_l = random.randint(300, 800)
        b = digits_for_limbs(l2, 5000, 9999)
        a = digits_for_limbs(l2 + q_l, 1, 9999)
        add(a, b)

    # ---- J: 纯随机兜底 ----
    for _ in range(1500):
        l2 = random.randint(1, 120)
        q_l = random.randint(1, 120)
        b = digits_for_limbs(l2, 1, 9999)
        a = digits_for_limbs(l2 + q_l, 1, 9999)
        add(a, b)
        add(b, a)  # 反向: dividend < divisor 的早退路径

    return cases


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "divstress.in"
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 12345
    cases = gen(seed)
    with open(out, "w", newline="\n") as f:
        f.write("%d\n" % len(cases))
        for a, b in cases:
            f.write("%d %d\n" % (a, b))
    # 同时写 Python 参考答案 (逐字节基准)
    with open(out + ".ans", "w", newline="\n") as f:
        for a, b in cases:
            f.write("%d %d\n" % (a // b, a % b))
    print("cases=%d  ->  %s (+ .ans)" % (len(cases), out))


if __name__ == "__main__":
    main()
