import random
rnd = random.Random(20260817)
def rand_hex(nlimbs):
    val = rnd.randrange(1, 1 << (64 * nlimbs))
    return format(val, 'x')
# 26 混合尺寸 (单 limb 快路径 ~ 20000 limb), 总量 < 9MB INCAP (read 截断会崩)
sizes = [1,1,2,3,5,8,12,20,50,100,200,500,1000,2000,5000,10000,
         15000,20000,20000,15000,10000,8000,6000,4000,3000,2000]
with open('cases26.txt', 'w') as f:
    f.write(f"{len(sizes)}\n")
    for n in sizes:
        a = rand_hex(n)
        b = rand_hex(max(1, rnd.randint(1, n)))
        f.write(f"{a} {b}\n")
# heavy: 2 个 MAX 尺寸查询 (~100k limb), 压测 FB/GB/FMG 缓冲
with open('heavy.txt', 'w') as f:
    f.write("2\n")
    for _ in range(2):
        f.write(f"{rand_hex(100000)} {rand_hex(100000)}\n")
print("cases26.txt / heavy.txt written")
