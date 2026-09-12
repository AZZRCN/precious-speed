#!/usr/bin/env python3
# 生成对抗输入: 覆盖 div_base16 cyclic unwrap bug 签名
# 格式: 首字段十进制 t, 其后 t 对 hex 串 (a b), 空白分隔
import random, sys

def hx(val, nhex):
    # 返回 nhex 位十六进制串 (小写, 无前缀)
    s = format(val, 'x')
    if len(s) > nhex: s = s[-nhex:]
    return s.rjust(nhex, '0')

def all_digit(d, nhex):
    return (d * nhex)

def power_of(base, exp, nhex):
    return hx(base ** exp, nhex)

cases = []
# 1. 全9 / 全9 (D12 签名 l2=8192,8193)
for L in (8192, 8193, 8191, 16384, 16666, 16667, 66666, 66667, 266666):
    a = all_digit('f', L)          # 全F (hex 全1)
    b = all_digit('f', L)
    cases.append((a, b))
    a2 = all_digit('9', L)          # 全9
    cases.append((a2, b))
    a3 = all_digit('9', L)
    b3 = all_digit('9', L // 2 if L // 2 > 0 else 1)
    cases.append((a3, b3))
# 2. 10^k (pow10) 除数 (D14 签名)
for L in (8192, 16384, 66666, 133333, 266666):
    a = all_digit('f', L)
    b = power_of(10, L // 3, L)     # 10^k 近似
    cases.append((a, b))
# 3. 大 a / 小 b (blocks=1 路径)
for L in (100000, 200000, 333333):
    a = all_digit('a', L)
    b = all_digit('b', 64)
    cases.append((a, b))
# 4. length_ratio 风格 (多块)
for L in (166667, 333333, 666666):
    a = all_digit('c', L)
    b = all_digit('d', L // 5)
    cases.append((a, b))
# 5. 随机大数 (多尺寸)
random.seed(34)
for L in (1000, 5000, 8192, 16384, 66666, 133333, 266666, 333333):
    a = ''.join(random.choice('0123456789abcdef') for _ in range(L))
    b = ''.join(random.choice('0123456789abcdef') for _ in range(max(1, L // 3)))
    cases.append((a, b))

out = [str(len(cases))]
for a, b in cases:
    out.append(f"{a} {b}")
sys.stdout.write("\n".join(out) + "\n")
