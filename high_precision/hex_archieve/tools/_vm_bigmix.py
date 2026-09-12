# AZZRCN
# https://github.com/AZZRCN
# 关键验证: 在 headline 量级 (c~90万) 上, 混合档 (radix-3, lm=786432) 相对
# 2 幂档 (lm=1048576) 的真实 cycles 比。若接近长度比 0.75, 说明大长度下顶层
# 蝶形常数劣势被 ~20 层摊薄, radix-7 (0.875) 稳赚; 若远高于 0.75, 则不值得。
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl
H = "/home/azzr/hexbench"
gen = r'''
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
'''
open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "_genbig.py"), "w").write(gen)
vmctl.put(os.path.join(os.path.dirname(os.path.abspath(__file__)), "_genbig.py"), f"{H}/genbig.py")
vmctl.run(f"cd {H} && python3 genbig.py && ls -la data/_big/")
vmctl.run(f"cd {H} && python3 casedist.py data/_big")
# 用已验证的 v13w 生成 .exp
vmctl.run(f"cd {H} && for f in data/_big/*.in; do build/v13w < $f > ${{f%.in}}.exp; done; ls -la data/_big/")
vmctl.run(f"cd {H} && python3 perfab.py data/_big {H}/build/v13w {H}/build/v16 -r 7", timeout=3600)
