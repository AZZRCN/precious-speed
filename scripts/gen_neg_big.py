#!/usr/bin/env python3
import random, sys
sys.set_int_max_str_digits(0)
random.seed(99)
def rd(n):
    s = str(random.randint(1, 9))
    for _ in range(n - 1):
        s += str(random.randint(0, 9))
    return s
a = "-" + rd(90000)
b = rd(45000)
print(1)
print(a, b)
