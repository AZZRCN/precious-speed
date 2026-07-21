#!/usr/bin/env python3
"""生成 absInvNewtonGMP vs absInvNewton 对比测试数据

输出格式 (对齐 HINT_OP_TESTNEWTON main 的 scanf):
  第一行 t (组数)
  然后每组 2 行: a 和 b 的十进制字符串 (无前导零)

测试用例覆盖:
  - base case (k <= 64 limbs, 即 b_digits <= 256)
  - Newton 1~4 层递归 (k 从 75 到 1250 limbs)
  - 含 cyclic 生效的 k 值 (k=250/500/1000/1250, 后续 cyclic 路径可用)
"""
import random
import sys


def rand_digits(n):
    """生成 n 位十进制数字字符串, 最高位非零 (保证 length() == ceil(n/4))"""
    if n <= 0:
        return "0"
    # 最高位 1-9, 其余 0-9
    head = str(random.randint(1, 9))
    body = ''.join(str(random.randint(0, 9)) for _ in range(n - 1))
    return head + body


def rand_digits_normalized(n):
    """生成归一化的 n 位十进制数字字符串 (最高 limb >= 5000 = HALF_BASE)

    要求: 位数是 4 的倍数 (保证最高 limb 恰好 4 位), 最高位 5-9 (保证 >= 5000)
    这是 absDivBasicCore 的前置条件 (Knuth Algorithm D 归一化)
    """
    if n <= 0:
        return "0"
    # 向上取整到 4 的倍数
    n = ((n + 3) // 4) * 4
    # 前 4 位: 最高位 5-9, 其余 0-9, 组成 5000-9999 (>= HALF_BASE)
    first4 = str(random.randint(5, 9)) + ''.join(str(random.randint(0, 9)) for _ in range(3))
    body = ''.join(str(random.randint(0, 9)) for _ in range(n - 4))
    return first4 + body


def main():
    random.seed(42)  # 固定种子, 可复现

    # (a_digits, b_digits, 说明)
    # b 必须归一化 (最高 limb >= 5000), 否则 absDivBasicCore 触发 assert
    # cyclic 生效区间: n+1 ∈ [2^p*2/3+1, 2^p], 即 k ∈ [2^p*2/3, 2^p-1]
    #   p=10: [682, 1023], p=11: [1365, 2047], p=12: [2730, 4095]
    cases = [
        (100, 52,      "小: b~13 limbs -> base case"),
        (600, 300,     "小-中: b~75 limbs -> Newton 1 层 (退化)"),
        (1000, 500,    "中: b~125 limbs -> Newton 1 层 (cyclic 生效 p=7)"),
        (2000, 1000,   "中: b~250 limbs -> Newton 2 层 (cyclic 生效 p=8)"),
        (4000, 2000,   "中-大: b~500 limbs -> Newton 3 层 (cyclic 生效 p=9)"),
        (8000, 4000,   "大: b~1000 limbs -> Newton 4 层 (cyclic 生效 p=10)"),
        (16000, 8000,  "大: b~2000 limbs -> Newton 5 层 (cyclic 生效 p=11)"),
        (32000, 16000, "大: b~4000 limbs -> Newton 6 层 (cyclic 生效 p=12)"),
    ]

    lines = [str(len(cases))]
    for a_n, b_n, _ in cases:
        lines.append(rand_digits(a_n))
        lines.append(rand_digits_normalized(b_n))

    out = '\n'.join(lines) + '\n'
    # 直接写文件, 避免 PowerShell 管道把 \n 吞掉的问题
    out_path = sys.argv[1] if len(sys.argv) > 1 else 'testnewton_data.txt'
    with open(out_path, 'w', encoding='ascii', newline='\n') as f:
        f.write(out)
    # 同时写一份到 stdout (兼容重定向场景)
    sys.stdout.write(out)


if __name__ == '__main__':
    main()
