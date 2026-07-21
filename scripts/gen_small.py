#!/usr/bin/env python3
"""生成 ADD 100k 和 MUL 100k 测试数据 (LC 格式: 计数行 + 数据)"""
import sys
sys.set_int_max_str_digits(200000)
import random
random.seed(2026)

def rand_dec(n):
    """生成 n 位十进制数 (字符串), 避免大整数转换"""
    digits = [str(random.randint(0, 9)) for _ in range(n)]
    digits[0] = str(random.randint(1, 9))  # 首位非零
    return ''.join(digits)

# ADD 100k+100k
with open('/tmp/bench/add_100k.txt', 'w') as f:
    f.write("1\n")
    f.write(rand_dec(100000) + "\n")
    f.write(rand_dec(100000) + "\n")

# MUL 100k*100k
with open('/tmp/bench/mul_100k.txt', 'w') as f:
    f.write("1\n")
    f.write(rand_dec(100000) + "\n")
    f.write(rand_dec(100000) + "\n")

print("生成完成: add_100k.txt, mul_100k.txt")
