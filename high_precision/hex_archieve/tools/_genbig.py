
import random, os
random.seed(20260811)
D = "/home/azzr/hexbench/data/_big"
os.makedirs(D, exist_ok=True)
def mk(name, limbs_each):
    n = limbs_each * 16
    a = "".join(random.choice("123456789abcdef") + "".join(random.choice("0123456789abcdef") for _ in range(0)) for _ in range(1))
    def h(n):
        s = [random.choice("123456789abcdef")]
        s += [random.choice("0123456789abcdef") for _ in range(n-1)]
        return "".join(s)
    with open(f"{D}/{name}.in","w") as f:
        f.write("1\n%s %s\n" % (h(n), h(n)))
    print(name, "hexlen", n, "limbs_each", limbs_each, "u", 2*limbs_each)
mk("mix3_big", 85312)    # u=170624 -> k=14 -> c=779995 -> lm=786432 (radix-3)
mk("pow2_big", 100000)   # u=200000 -> k=14 -> c=914286 -> lm=1048576 (2 幂)
mk("mix5_big",  71680)   # u=143360 -> k=14 -> c=655360 -> lm=655360 (radix-5, 零浪费)
