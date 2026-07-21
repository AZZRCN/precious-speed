#!/usr/bin/env python3
"""生成 absInvNewtonGMP 大规模测试数据"""
import random

random.seed(42)

def gen(digits):
    """生成 digits 位的十进制字符串, 最高位 5-9 (归一化)"""
    s = str(random.randint(5, 9))
    for _ in range(digits - 1):
        s += str(random.randint(0, 9))
    return s

# (a_digits, b_digits) - b 的 limb 数 k = b_digits/4
cases = [
    (200000, 100000),   # k=25000
    (400000, 200000),   # k=50000
    (500000, 250000),   # k=62500 (1M/500k 实际规模)
]

with open('testnewton_big.txt', 'w') as f:
    f.write(str(len(cases)) + '\n')
    for a_digits, b_digits in cases:
        f.write(gen(a_digits) + '\n')
        f.write(gen(b_digits) + '\n')

print(f'Generated testnewton_big.txt with {len(cases)} cases')
for a_digits, b_digits in cases:
    print(f'  a={a_digits} digits, b={b_digits} digits (k={b_digits//4})')
