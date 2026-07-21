#!/usr/bin/env python3
# 模拟 cyclic + unwrap 的误差
import random

BASE = 10000

def big_pow(base, exp):
    result = 1
    for _ in range(exp):
        result *= base
    return result

def test_unwrap(len2, this_in):
    # 限制大小
    if len2 > 500:
        return 0, 0, 0, len2, this_in, 0, 0
    
    # 随机 divisor (len2 位), 最高位 >= HALF_BASE
    div_high = random.randint(5000, 9999)
    divisor = div_high * big_pow(BASE, len2-1)
    for i in range(len2-1):
        divisor += random.randint(0, 9999) * big_pow(BASE, i)
    
    # 随机 qhat (this_in+1 位)
    qhat = 0
    for i in range(this_in+1):
        qhat += random.randint(0, 9999) * big_pow(BASE, i)
    
    # remainder < divisor
    remainder = random.randint(0, divisor - 1)
    
    # window = qhat * divisor + remainder (有 len2 + this_in 位)
    window = qhat * divisor + remainder
    
    # rp_old = window 高 len2 位 = window / BASE^this_in
    rp_old = window // big_pow(BASE, this_in)
    
    # 真实 product = qhat * divisor
    prod = qhat * divisor
    
    # cyclic_m = 2^ceil(log2(len2 + 1))
    cyclic_m = 1
    while cyclic_m < len2 + 1:
        cyclic_m *= 2
    
    # cyclic result = prod mod (B^cyclic_m - 1)
    Bm = big_pow(BASE, cyclic_m)
    mod = Bm - 1
    cyc = prod % mod
    
    # wn = len2 + this_in - cyclic_m
    wn = len2 + this_in - cyclic_m
    
    if wn <= 0:
        # wn <= 0 表示不需要 unwrap，cyclic_m >= len2+this_in
        unwrapped = cyc
    else:
        # unwrap: cyc_low_wn -= rp_old_high_wn
        # rp_old_high_wn = rp_old 高 wn 位 = rp_old / BASE^(len2 - wn)
        rp_high_wn = rp_old // big_pow(BASE, len2 - wn)
        
        # cyc_low_wn = cyc 低 wn 位
        cyc_low = cyc % big_pow(BASE, wn)
        cyc_high = cyc // big_pow(BASE, wn)
        
        # 低 wn 位相减
        if cyc_low >= rp_high_wn:
            new_low = cyc_low - rp_high_wn
            borrow = 0
        else:
            new_low = cyc_low + big_pow(BASE, wn) - rp_high_wn
            borrow = 1
        
        # 借位传播
        new_high = cyc_high - borrow
        
        unwrapped = new_low + new_high * big_pow(BASE, wn)
    
    # 比较 unwrapped 和 prod 的低 len2 位
    prod_low = prod % big_pow(BASE, len2)
    unwrapped_low = unwrapped % big_pow(BASE, len2)
    
    diff = abs(prod_low - unwrapped_low)
    
    return diff, prod_low, unwrapped_low, len2, this_in, cyclic_m, wn

# 测试
random.seed(42)
print("Testing unwrap accuracy...")
max_diff = 0
max_diff_case = None
count = 0
for _ in range(200):
    len2 = random.choice([50, 100, 200, 300, 400, 500])
    this_in = random.choice([len2//4, len2//3, len2//2, len2-1])
    this_in = max(this_in, 10)
    diff, pl, ul, l2, ti, cm, wn = test_unwrap(len2, this_in)
    if l2 > 500:
        continue
    count += 1
    if diff > max_diff:
        max_diff = diff
        max_diff_case = (l2, ti, cm, wn, diff)
    if count <= 5:
        print(f"  len2={l2} this_in={ti} cyclic_m={cm} wn={wn} diff={diff}")

print(f"\nTotal tests: {count}")
print(f"Max diff: {max_diff}")
if max_diff_case:
    print(f"Case: len2={max_diff_case[0]} this_in={max_diff_case[1]} cyclic_m={max_diff_case[2]} wn={max_diff_case[3]} diff={max_diff_case[4]}")
