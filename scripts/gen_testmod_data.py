#!/usr/bin/env python3
# 生成 fftMulModBm1 正确性测试数据
# 格式: 第一行 t, 接下来 t 行 "a b m"
# a, b 是非负整数字符串, m 是 2 的幂, a_len <= m, b_len <= m
import sys, random
sys.set_int_max_str_digits(2000000)

random.seed(42)
cases = []

# 1. 小数据用例 (验证基础正确性)
cases.append(("123", "456", 4))
cases.append(("9999", "9999", 4))
cases.append(("12345678", "87654321", 8))
cases.append(("0", "12345", 4))
cases.append(("99999999", "1", 8))

# 2. 中等数据 (验证 cyclic carry 折叠)
# 10^16-1 = 9999999999999999, m=4, B^m-1 = 10^4-1 = 9999
# (10^16-1) mod 9999 = 0 (因为 10^4 ≡ 1 mod 9999, 10^16 = (10^4)^4 ≡ 1)
cases.append(("9" * 16, "1", 4))
cases.append(("9" * 16, "9" * 16, 8))

# 3. 边界: a_len = m, b_len = m
cases.append(("1234" * 4, "5678" * 4, 16))  # 16 位, m=16
cases.append(("9999" * 8, "9999" * 8, 64))  # 32 位, m=64

# 4. 一侧为 0
cases.append(("0", "0", 4))
cases.append(("0", "9999", 4))

# 5. 大数据 (验证 FFT 路径)
# m = 256 (2^8), a 和 b 各 256 位
a_big = "".join(random.choices("0123456789", k=256))
b_big = "".join(random.choices("0123456789", k=256))
cases.append((a_big, b_big, 256))

# m = 1024, a 和 b 各 1024 位
a_big2 = "".join(random.choices("0123456789", k=1024))
b_big2 = "".join(random.choices("0123456789", k=1024))
cases.append((a_big2, b_big2, 1024))

# 6. 大数据 + cyclic 折叠关键场景
# a*b 的线性卷积长度 > m, 触发 cyclic carry
# a=10^200 (1 后面 200 个 0), b=10^200, m=128
# a*b = 10^400, mod (10^128-1)
# 10^400 mod (10^128-1): 400 = 3*128 + 16, 10^400 = 10^(3*128) * 10^16 ≡ 1 * 10^16 = 10^16
a_pow = "1" + "0" * 200
b_pow = "1" + "0" * 200
cases.append((a_pow, b_pow, 128))

# 7. 随机大数 (m=4096)
a_r4 = "".join(random.choices("0123456789", k=4096))
b_r4 = "".join(random.choices("0123456789", k=4096))
cases.append((a_r4, b_r4, 4096))

# 8. 极端: a_len 略大于 m (应该 assert 失败, 但测试 a_len <= m)
# 跳过, fftMulModBm1 要求 a_len <= m

with open("/tmp/bench/testmod_data.txt", "w") as f:
    f.write(f"{len(cases)}\n")
    for a, b, m in cases:
        f.write(f"{a} {b} {m}\n")

print(f"Generated {len(cases)} test cases to /tmp/bench/testmod_data.txt")
print("Cases:")
for i, (a, b, m) in enumerate(cases):
    print(f"  {i+1}: a_len={len(a)}, b_len={len(b)}, m={m}")
