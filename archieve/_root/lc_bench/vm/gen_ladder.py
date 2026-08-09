"""生成"尺寸递增阶梯"用例，专门逼出 FFT 双缓冲的反复重分配路径。

大页分支在 allocatedSize 增长时不 munmap 旧映射，需确认泄漏有界（不超内存限制）。
每组数据长度依次翻倍，覆盖 transformLength 的每一级增长。
"""
import random
import sys

random.seed(7)

sizes = []
n = 1
while n <= 600000:
    sizes.append(n)
    n *= 2
sizes.append(600000)

D = "0123456789"


def num(length):
    return random.choice("123456789") + "".join(
        random.choice(D) for _ in range(length - 1))


out = [str(len(sizes))]
for s in sizes:
    out.append(num(s) + " " + num(s))

path = sys.argv[1] if len(sys.argv) > 1 else "cases_extra_ladder.in"
with open(path, "w") as fh:
    fh.write("\n".join(out) + "\n")

print(f"cases={len(sizes)} maxsize={max(sizes)} sizes={sizes}")
