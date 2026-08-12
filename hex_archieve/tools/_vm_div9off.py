import os, sys, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl
H = "/home/azzr/divbench"; B = f"{H}/bin"; GEN="/home/azzr/lcgen/division_of_big_integers/gen"; COM="/home/azzr/lcgen/common"
NL = chr(10)
# 1) 编译官方生成器
vmctl.run(f"cd {GEN} && g++ -O2 -std=c++17 -I {COM} length_ratio_integer.cpp -o /tmp/gen_lri 2>&1 | head -5")
# 2) 生成真 hex 输入
vmctl.run("/tmp/gen_lri 1 > /tmp/lri1.in")
rc,fmt,_ = vmctl.run("head -c 160 /tmp/lri1.in; echo; echo '---'; wc -l /tmp/lri1.in")
print("FMT:", fmt.replace(NL,' | '))
# 3) 三向对比
o9 = vmctl.run(f"cd {H} && {B}/div_v9 < /tmp/lri1.in")[1]
oo = vmctl.run(f"cd {H} && {B}/div_orig < /tmp/lri1.in")[1]
# python 真值（按 hex；第一行假设 T）
py = ("import sys"+NL
      "data=open('/tmp/lri1.in').read().split()"+NL
      "T=int(data[0]); idx=1; out=[]"+NL
      "for _ in range(T):"+NL
      "  a=int(data[idx],16); b=int(data[idx+1],16); idx+=2"+NL
      "  out.append(format(a//b,'x')+' '+format(a%b,'x'))"+NL
      "open('/tmp/lri1_gold.txt','w').write(chr(10).join(out)+chr(10))"+NL)
vmctl.run("python3 - <<'PYEOF'"+NL+py+"PYEOF")
gold = vmctl.run("cat /tmp/lri1_gold.txt")[1]
v9l=o9.split(NL); ool=oo.split(NL); gl=gold.split(NL)
def n(s): return s.strip().lower()
print("V9  [:2]:", v9l[:2])
print("ORIG[:2]:", ool[:2])
print("GOLD[:2]:", gl[:2])
print("v9==gold :", all(n(a)==n(b) for a,b in zip(v9l,gl)))
print("orig==gold:", all(n(a)==n(b) for a,b in zip(ool,gl)))
# 找第一个不一致的行
for i,(a,b,c) in enumerate(zip(v9l,ool,gl)):
    if n(a)!=n(c) or n(b)!=n(c):
        print(f"FIRST DIFF row {i}: v9={a[:40]}.. orig={b[:40]}.. gold={c[:40]}..")
        break
else:
    print("ALL ROWS CONSISTENT (within printed range)")
