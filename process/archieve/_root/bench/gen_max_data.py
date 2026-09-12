#!/usr/bin/env python3
"""生成有效的 max 规模测速数据
add: t=2, 两行各一个 N 位正整数 -> 真正做加法
mul: t=2, 两行各一个 N 位正整数 -> 真正做乘法
div: t=1, 一行 a b, a 2N 位 b N 位 -> 真正做除法
"""
import os, random, sys

OUT = r"d:\precious_speed\bench_data"

def rand_digits(n, first_nz=True):
    """生成 n 位随机十进制数字符串, 首位非零"""
    s = ''.join(random.choices('0123456789', k=n))
    if first_nz and n > 1:
        # 首位替换为 1-9
        s = str(random.randint(1, 9)) + s[1:]
    return s

def gen_add(path, n=2_000_000):
    print(f"[gen] add: t=2, each {n} digits -> {path}")
    with open(path, 'w') as f:
        f.write("2\n")
        f.write(rand_digits(n) + "\n")
        f.write(rand_digits(n) + "\n")

def gen_mul(path, n=2_000_000):
    print(f"[gen] mul: t=2, each {n} digits -> {path}")
    with open(path, 'w') as f:
        f.write("2\n")
        f.write(rand_digits(n) + "\n")
        f.write(rand_digits(n) + "\n")

def gen_div(path, a_n=4_000_000, b_n=2_000_000):
    print(f"[gen] div: t=1, a={a_n} b={b_n} digits -> {path}")
    with open(path, 'w') as f:
        f.write("1\n")
        f.write(f"{rand_digits(a_n)} {rand_digits(b_n)}\n")

if __name__ == "__main__":
    random.seed(20260730)
    # LC max 规格: add 总位<=2M, mul 每数<=1M, div A<=2M B<=1M
    gen_add(os.path.join(OUT, "add_max_0.in"), n=1_000_000)  # 2 个 100万位
    gen_add(os.path.join(OUT, "add_max_1.in"), n=1_000_000)
    gen_mul(os.path.join(OUT, "mul_max_0.in"), n=1_000_000)  # 2 个 100万位
    gen_mul(os.path.join(OUT, "mul_max_1.in"), n=1_000_000)
    gen_div(os.path.join(OUT, "div_max_0.in"), a_n=2_000_000, b_n=1_000_000)  # a 200万 b 100万
    gen_div(os.path.join(OUT, "div_max_2.in"), a_n=2_000_000, b_n=1_000_000)
    print("[done]")
