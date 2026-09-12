import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl
H = "/home/azzr/divbench"; B = f"{H}/bin"
GEN = "/home/azzr/lcgen/division_of_big_integers/gen"; COM = "/home/azzr/lcgen/common"
NL = chr(10)
HERE = os.path.dirname(os.path.abspath(__file__))
vmctl.put(os.path.join(HERE, '_gold_lri.py'), '/tmp/_gold_lri.py')
vmctl.run(f"cd {GEN} && g++ -O2 -std=c++17 -I {COM} length_ratio_integer.cpp -o /tmp/gen_lri 2>&1 | head -3")
vmctl.run("/tmp/gen_lri 1 > /tmp/lri1.in")
rc, fmt, _ = vmctl.run("head -c 160 /tmp/lri1.in; echo; wc -l /tmp/lri1.in")
print("FMT:", fmt.replace(NL, ' | '))
o9 = vmctl.run(f"cd {H} && {B}/div_v9 < /tmp/lri1.in")[1]
oo = vmctl.run(f"cd {H} && {B}/div_orig < /tmp/lri1.in")[1]
vmctl.run("python3 /tmp/_gold_lri.py")
gold = vmctl.run("cat /tmp/lri1_gold.txt")[1]
v9l = o9.split(NL); ool = oo.split(NL); gl = gold.split(NL)
def n(s): return s.strip().lower()
print("V9[:2]:", v9l[:2]); print("ORIG[:2]:", ool[:2]); print("GOLD[:2]:", gl[:2])
print("v9==gold:", all(n(a) == n(b) for a, b in zip(v9l, gl)))
print("orig==gold:", all(n(a) == n(b) for a, b in zip(ool, gl)))
for i, (a, b, c) in enumerate(zip(v9l, ool, gl)):
    if n(a) != n(c) or n(b) != n(c):
        print(f"FIRST DIFF row {i}: v9={a[:40]}.. orig={b[:40]}.. gold={c[:40]}..")
        break
else:
    print("ALL CONSISTENT")
