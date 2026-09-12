import subprocess, random, sys
sys.set_int_max_str_digits(10**7)

# 生成多组测试数据, 重点覆盖 str32to8limbs (>=32 字节)
# 格式: 第一行 N, 之后每行 A B
tests = []
# 1. 50 个 9 + 1 (触发 32 字节路径, carry chain)
tests.append(("9"*50, "1"))
# 2. 两个 50 位数相加
tests.append(("12345678901234567890123456789012345678901234567890",
              "98765432109876543210987654321098765432109876543210"))
# 3. 100 位数 + 100 位数 (多次触发 32 字节路径)
a = "".join(random.choice("0123456789") for _ in range(100))
b = "".join(random.choice("0123456789") for _ in range(100))
tests.append((a, b))
# 4. 500000 位大数 (跟 LC max_max 类似规模)
a = "".join(random.choice("0123456789") for _ in range(500000))
b = "".join(random.choice("0123456789") for _ in range(500000))
tests.append((a, b))
# 5. 负数测试 (carry_chain 场景)
tests.append(("-" + "9"*40, "1"))
# 6. 两个负数
tests.append(("-" + "1234567890123456789012345678901234567890",
              "-" + "9876543210987654321098765432109876543210"))
# 7. 大数减小数
tests.append(("1" + "0"*40, "1"))
# 8. 64 字节边界 (正好 2 次 str32to8limbs)
tests.append(("12345678901234567890123456789012" + "34567890123456789012345678901234",
              "1"))

with open("test_add_fix.in", "w") as f:
    f.write(f"{len(tests)}\n")
    for a, b in tests:
        f.write(f"{a} {b}\n")

# Python 参考答案
expected = []
for a, b in tests:
    expected.append(str(int(a) + int(b)))
with open("test_add_fix.expected", "w") as f:
    f.write("\n".join(expected) + "\n")

print(f"Generated {len(tests)} test cases")
print(f"Max digits: {max(len(a.strip('-')) for a, _ in tests)}")
