#!/usr/bin/env python3
# 模拟 cyclic + unwrap 的误差
import random

BASE = 10000

def int_to_limbs(x, n):
    limbs = []
    for i in range(n):
        limbs.append(x % BASE)
        x //= BASE
    return limbs

def limbs_to_int(limbs):
    x = 0
    for l in reversed(limbs):
        x = x * BASE + l
    return x

def test_unwrap(len2, this_in):
    # 随机 divisor (len2 位)
    divisor = random.randint(BASE**(len2-1) * 5000, BASE**len2 - 1)  # 最高位 >= HALF_BASE
    # 随机 qhat (this_in+1 位)
    qhat = random.randint(0, BASE**(this_in+1) - 1)
    # window = qhat * divisor + remainder, remainder < divisor
    remainder = random.randint(0, divisor - 1)
    # window 有 len2 + this_in 位
    window = qhat * divisor + remainder
    # window 的高 len2 位 = rp_old
    rp_old = (window >> (this_in * 133))  # 不对，应该用 BASE 移位
    # 等等，BASE=10^4，每 limb = 4 位十进制
    # 移位 this_in 个 limb = 乘以 BASE^this_in
    rp_old = window // (BASE ** this_in)  # 高 len2 位
    rp_old_low = rp_old % BASE**len2  # 取低 len2 位（就是 rp_old 本身，因为 rp_old 只有 len2 位）
    
    # 真实 product
    prod = qhat * divisor
    
    # cyclic_m = 2^ceil(log2(len2 + 1))
    cyclic_m = 1
    while cyclic_m < len2 + 1:
        cyclic_m *= 2
    
    # 限制测试大小，避免数字过大
    if len2 > 2000:
        return 0, 0, 0, len2, this_in, cyclic_m, 0
    
    # cyclic result = prod mod (B^cyclic_m - 1)
    mod = BASE**cyclic_m - 1
    cyc = prod % mod
    
    # wn = len2 + this_in - cyclic_m
    wn = len2 + this_in - cyclic_m
    
    # unwrap: cyc_low_wn -= rp_old_high_wn
    rp_old_high_wn = rp_old // (BASE ** (len2 - wn))  # rp_old 的高 wn 位
    cyc_low_wn = cyc % (BASE ** wn)  # cyc 的低 wn 位
    cyc_rest = cyc // (BASE ** wn)   # cyc 的高位部分
    
    # 低 wn 位相减，带借位
    if cyc_low_wn >= rp_old_high_wn:
        new_low = cyc_low_wn - rp_old_high_wn
        borrow = 0
    else:
        new_low = cyc_low_wn + BASE**wn - rp_old_high_wn
        borrow = 1
    
    # 借位传播到高位
    if borrow:
        if cyc_rest >= 1:
            new_rest = cyc_rest - 1
        else:
            # 高位也是 0，需要再借位（wrap around？）
            # 但 cyc_rest 有 cyclic_m - wn 位，不应该为 0 除非 cyc < BASE^wn
            # 这种情况下 borrow 会 wrap，这里简化处理
            new_rest = BASE**(cyclic_m - wn) - 1
            # 实际上 mod B^m-1 下借位会 wrap，但我们这里不考虑 wrap（误差小）
    
    unwrapped = new_low + new_rest * (BASE ** wn)
    
    # 比较 unwrapped 和 prod 的低 len2 位
    prod_low = prod % (BASE ** len2)
    unwrapped_low = unwrapped % (BASE ** len2)
    
    diff = abs(prod_low - unwrapped_low)
    
    return diff, prod_low, unwrapped_low, len2, this_in, cyclic_m, wn

# 测试多组
random.seed(42)
print("Testing unwrap accuracy...")
max_diff = 0
max_diff_case = None
for _ in range(1000):
    len2 = random.choice([100, 200, 500, 1000, 2000, 5000, 10000])
    this_in = random.choice([len2//4, len2//3, len2//2, len2-1])
    this_in = max(this_in, 10)
    diff, pl, ul, l2, ti, cm, wn = test_unwrap(len2, this_in)
    if diff > max_diff:
        max_diff = diff
        max_diff_case = (l2, ti, cm, wn, pl, ul, diff)
    if _ < 5:
        print(f"  len2={l2} this_in={ti} cyclic_m={cm} wn={wn} diff={diff}")

print(f"\nMax diff: {max_diff}")
print(f"Case: len2={max_diff_case[0]} this_in={max_diff_case[1]} cyclic_m={max_diff_case[2]} wn={max_diff_case[3]}")
print(f"  prod_low   = {max_diff_case[4]}")
print(f"  unwrapped  = {max_diff_case[5]}")
print(f"  diff       = {max_diff_case[6]}")

# 误差相当于多少个 limb
import math
if max_diff > 0:
    diff_limbs = int(math.log(max_diff) / math.log(BASE)) + 1
    print(f"  diff ~ {diff_limbs} limbs")
