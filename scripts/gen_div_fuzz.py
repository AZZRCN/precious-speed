#!/usr/bin/env python3
# Fuzz 测试: 生成多种尺寸的除法用例, 覆盖 2NXN 循环卷积的边界情况
# 边界: wn=0 (len2+this_in == cyclic_m), wn>0 (wrap), this_in < in (最后一块)
import random
import sys

def gen_case(a_digits, b_digits, seed):
    random.seed(seed)
    # b 归一化: 最高位 5-9 (最高 limb >= 5000), 位数是 4 的倍数
    b_digits = (b_digits + 3) // 4 * 4
    a_digits = max(a_digits, b_digits + 4)
    a_digits = (a_digits + 3) // 4 * 4
    b_high = str(random.randint(5, 9))
    b_rest = ''.join(str(random.randint(0, 9)) for _ in range(b_digits - 1))
    b = b_high + b_rest
    a_high = str(random.randint(1, 9))
    a_rest = ''.join(str(random.randint(0, 9)) for _ in range(a_digits - 1))
    a = a_high + a_rest
    return a, b

# 测试用例: (a_digits, b_digits, 描述)
# 目标覆盖不同 len2/this_in/cyclic_m 关系
cases = [
    # 小规模: base case 路径 (len2 <= 64 或 len1-len2 <= 64)
    (100, 50, "small base case"),
    (200, 100, "small"),
    # 中规模: absDivMu 路径, 单 block
    (1000, 500, "single block, wn>0"),
    (2000, 1000, "single block, wn>0"),
    # 多 block: qn > in
    (4000, 1000, "multi block, qn=3000, in~500"),
    (8000, 2000, "multi block"),
    # 边界: wn 接近 0
    # len2=12500, cyclic_m=int_ceil2(12501)=16384, in=6250
    # wn = 12500 + 6250 - 16384 = 2366 > 0
    (50000, 25000, "wn>0 medium"),
    # 大规模
    (100000, 50000, "100k/50k"),
    (200000, 100000, "200k/100k"),
    # 非整数倍 qn (最后一块 this_in < in)
    # len2=5000, in=2500, qn=7500 -> blocks: this_in=2500, 2500, 2500 (整数倍)
    # len2=5000, in=2500, qn=6000 -> blocks: this_in=2500, 2500 (整数倍)
    # 需要 qn 不是 in 整数倍: a_digits 使 qn = a-b 不是 in 倍数
    (15000, 5000, "non-multiple qn"),  # qn=10000, in=2500, 4 blocks exact
    (13000, 5000, "non-multiple qn v2"),  # qn=8000, in=2500, this_in=2500,2500,2500 (exact)
    # 真正的非整数倍: qn % in != 0
    # len2=4000, in=2000, qn=5000 -> this_in=2000, 2000, 1000
    (9000, 4000, "last block partial"),  # qn=5000, in=2000, last this_in=1000
]

with open('div_fuzz.in', 'w', encoding='ascii', newline='\n') as f:
    f.write(f"{len(cases)}\n")
    for i, (a_d, b_d, desc) in enumerate(cases):
        a, b = gen_case(a_d, b_d, seed=42 + i)
        f.write(f"{a}\n{b}\n")
print(f"Generated {len(cases)} cases to div_fuzz.in")
