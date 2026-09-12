import os, sys, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl
H = "/home/azzr/divbench"; B = f"{H}/bin"
o9 = vmctl.run(f"cd {H} && {B}/div_v9 < /tmp/diag.in")[1]
oo = vmctl.run(f"cd {H} && {B}/div_orig < /tmp/diag.in")[1]
gold = vmctl.run("cat /tmp/diag_gold.txt")[1]
def norm(s): return re.sub(r'(?i)(?<![0-9a-f])0+(?=[0-9a-f])','', s.strip().lower())
print("V9   :", norm(o9))
print("ORIG :", norm(oo))
print("GOLD :", norm(gold))
print("v9==gold:", norm(o9)==norm(gold), " orig==gold:", norm(oo)==norm(gold))
# 长度
print("len v9/orig/gold:", len(norm(o9)), len(norm(oo)), len(norm(gold)))
