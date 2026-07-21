#!/usr/bin/env python3
# 大规模 fuzz: 100 组随机尺寸, 覆盖各种 len2/this_in/cyclic_m 关系
import random

def gen_case(a_digits, b_digits, seed):
    random.seed(seed)
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

random.seed(123)
cases = []
for i in range(100):
    b_d = random.choice([100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000])
    a_d = b_d + random.choice([100, 500, 1000, 5000, 10000, 50000, 100000])
    cases.append((a_d, b_d))

with open('div_fuzz_big.in', 'w', encoding='ascii', newline='\n') as f:
    f.write(f"{len(cases)}\n")
    for i, (a_d, b_d) in enumerate(cases):
        a, b = gen_case(a_d, b_d, seed=1000 + i)
        f.write(f"{a}\n{b}\n")
print(f"Generated {len(cases)} cases to div_fuzz_big.in")
