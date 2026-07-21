#!/usr/bin/env python3
"""生成所有 LC 格式测试数据 (计数行 + 数据)"""
import sys
sys.set_int_max_str_digits(2000000)
import random
random.seed(2026)

def rand_dec(n):
    """生成 n 位十进制数 (字符串)"""
    digits = [str(random.randint(0, 9)) for _ in range(n)]
    digits[0] = str(random.randint(1, 9))
    return ''.join(digits)

import os
os.makedirs('/tmp/bench', exist_ok=True)

# ADD: 1\n a \n b \n
cases = [
    ('add_1M.txt', 1000000, 1000000),
    ('add_100k.txt', 100000, 100000),
    ('mul_500k.txt', 500000, 500000),
    ('mul_100k.txt', 100000, 100000),
    ('div_1M_500k.txt', 1000000, 500000),
    ('div_1M_100k.txt', 1000000, 100000),
    ('div_200k_100k.txt', 200000, 100000),
]

for fname, na, nb in cases:
    with open(f'/tmp/bench/{fname}', 'w') as f:
        f.write("1\n")
        f.write(rand_dec(na) + "\n")
        f.write(rand_dec(nb) + "\n")
    print(f"生成 {fname}: {na} + {nb} 位")

print("全部完成")
