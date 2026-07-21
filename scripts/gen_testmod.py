#!/usr/bin/env python3
"""生成 fftMulModBm1 测试数据.
格式: t / a b m / a b m / ...
m 必须是 2 的幂. a, b 为十进制字符串.
BASE = 10^4, 所以 m 个 limb = 4*m 位十进制.
"""
import random
import sys

def rand_dec(ndigits):
    """生成 ndigits 位十进制数 (字符串)."""
    if ndigits <= 0:
        return "0"
    return str(random.randint(10**(ndigits-1), 10**ndigits - 1))

def main():
    seed = int(sys.argv[1]) if len(sys.argv) > 1 else 42
    random.seed(seed)
    cases = []
    # 1. 小数据 m=4 (16 位)
    cases.append((rand_dec(8), rand_dec(8), 4))
    cases.append((rand_dec(16), rand_dec(16), 4))
    # 2. m=16 (64 位)
    cases.append((rand_dec(32), rand_dec(32), 16))
    cases.append((rand_dec(64), rand_dec(64), 16))
    # 3. m=64 (256 位), a+b > m 触发折叠
    cases.append((rand_dec(128), rand_dec(128), 64))
    cases.append((rand_dec(256), rand_dec(256), 64))
    # 4. m=128, a_len = m, b_len = m (最大折叠)
    cases.append((rand_dec(4*128), rand_dec(4*128), 128))
    # 5. m=256, a_len+b_len-1 = m (无折叠, cyclic=linear)
    cases.append((rand_dec(4*128), rand_dec(4*129), 256))
    # 6. m=256, a_len+b_len-1 = m+1 (最小折叠)
    cases.append((rand_dec(4*129), rand_dec(4*129), 256))
    # 7. m=1024, 大数据
    cases.append((rand_dec(4*1024), rand_dec(4*1024), 1024))
    # 8. m=1024, a_len=1 (单 limb)
    cases.append((rand_dec(4), rand_dec(4*1024), 1024))
    # 9. m=4096, 大数据
    cases.append((rand_dec(4*4096), rand_dec(4*4096), 4096))
    # 10. m=4096, a_len=4097, b_len=4097 (超过 m, 但 assert 要求 <= m, 跳过)
    # 11. 边界: a=0
    cases.append(("0", rand_dec(64), 16))
    # 12. 边界: b=0
    cases.append((rand_dec(64), "0", 16))
    # 13. 边界: a=b
    a = rand_dec(256)
    cases.append((a, a, 128))
    # 14. m=16384, 中等规模
    cases.append((rand_dec(4*16384), rand_dec(4*16384), 16384))
    # 15. m=8192, a_len+b_len 略大于 m
    cases.append((rand_dec(4*4097), rand_dec(4*4097), 8192))

    print(len(cases))
    for a, b, m in cases:
        print(a, b, m)

if __name__ == "__main__":
    main()
