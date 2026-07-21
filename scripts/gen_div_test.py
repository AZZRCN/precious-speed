import random
import sys

seed = int(sys.argv[1]) if len(sys.argv) > 1 else 42
a_digits = int(sys.argv[2]) if len(sys.argv) > 2 else 100
b_digits = int(sys.argv[3]) if len(sys.argv) > 3 else 50

random.seed(seed)
a = random.randint(10**(a_digits-1), 10**a_digits - 1)
b = random.randint(10**(b_digits-1), 10**b_digits - 1)
q = a // b
r = a % b

# 输出格式: 第一行用例数, 然后 a, b 交替
print(1)
print(a)
print(b)

# 写入预期结果到 stderr
import sys as _sys
_sys.stderr.write(f"EXPECTED_Q={q}\n")
_sys.stderr.write(f"EXPECTED_R={r}\n")
