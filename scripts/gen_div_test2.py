import random
import sys

sys.set_int_max_str_digits(100000)  # 允许大整数转字符串

seed = int(sys.argv[1]) if len(sys.argv) > 1 else 42
a_digits = int(sys.argv[2]) if len(sys.argv) > 2 else 100
b_digits = int(sys.argv[3]) if len(sys.argv) > 3 else 50

random.seed(seed)
a = random.randint(10**(a_digits-1), 10**a_digits - 1)
b = random.randint(10**(b_digits-1), 10**b_digits - 1)
q = a // b
r = a % b

# 写入测试数据 (UTF-8, 无 BOM)
with open("test_div_data.txt", "w", encoding="utf-8") as f:
    f.write("1\n")
    f.write(str(a) + "\n")
    f.write(str(b) + "\n")

# 写入预期结果
with open("test_div_expected.txt", "w", encoding="utf-8") as f:
    f.write(f"{q} {r}\n")

print(f"Generated: a={a_digits}digits, b={b_digits}digits")
print(f"Expected: q={q}")
print(f"          r={r}")
