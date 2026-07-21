#!/usr/bin/env python3
# gen_big_data.py
# 生成接近 LC 上限的大测试数据
import random
import os

OUT = "/tmp/bench2"

def rand_digits(n):
    """生成 n 位随机数字字符串，首位非零"""
    s = str(random.randint(1, 9))
    s += ''.join(random.choices('0123456789', k=n-1))
    return s

# ADD/MUL: 2M+2M 位 (接近 4M 总字符限制)
print("Generating ADD/MUL 2M+2M...")
a = rand_digits(2000000)
b = rand_digits(2000000)
with open(f"{OUT}/big_add_2M_2M.txt", "w") as f:
    f.write(f"1\n{a}\n{b}\n")
# MUL 用相同数据
os.symlink(f"{OUT}/big_add_2M_2M.txt", f"{OUT}/big_mul_2M_2M.txt")

# DIV: 2M / 1M (商约 1M 位，计算量最大)
print("Generating DIV 2M/1M...")
a = rand_digits(2000000)
b = rand_digits(1000000)
with open(f"{OUT}/big_div_2M_1M.txt", "w") as f:
    f.write(f"1\n{a}\n{b}\n")

# DIV: 2M / 500k (更不平衡，测试 Core2 路径)
print("Generating DIV 2M/500k...")
a = rand_digits(2000000)
b = rand_digits(500000)
with open(f"{OUT}/big_div_2M_500k.txt", "w") as f:
    f.write(f"1\n{a}\n{b}\n")

# DIV: 2M / 2M (商=1，最快路径)
print("Generating DIV 2M/2M...")
a = rand_digits(2000000)
b = rand_digits(2000000)
# 确保 a >= b（否则商=0）
if a < b:
    a, b = b, a
with open(f"{OUT}/big_div_2M_2M.txt", "w") as f:
    f.write(f"1\n{a}\n{b}\n")

# ADD/MUL: 1M+1M (已有，跳过)
# ADD/MUL: 500k+500k (已有，跳过)

print("Done.")
print(f"Files in {OUT}:")
for f in sorted(os.listdir(OUT)):
    if f.startswith("big_"):
        size = os.path.getsize(f"{OUT}/{f}")
        print(f"  {f}: {size} bytes ({size/1000000:.1f}MB)")
