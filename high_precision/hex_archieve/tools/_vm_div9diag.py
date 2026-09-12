import os, sys, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl
H = "/home/azzr/divbench"; B = f"{H}/bin"; S = f"{H}/src"
vmctl.put(os.path.join(R:='') , '') if False else None
R = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# 构造一个会触发 FFT 的大 hex 输入，对照 python 真值
mk = (
"import sys\n"
"a=int('f'*256,16); b=int('f'*128,16)\n"
"open('/tmp/diag.in','w').write('1\n'+format(a,'x')+' '+format(b,'x')+'\n')\n"
"open('/tmp/diag_gold.txt','w').write(format(a//b,'x')+' '+format(a%b,'x')+'\n')\n"
)
rc,o,_ = vmctl.run(f"python3 -c {repr(mk)}")
print("mk:", o.strip()[:200])
def runbin(tag, binpath):
    rc,o,_ = vmctl.run(f"cd {H} && {binpath} < /tmp/diag.in")
    open(f'/tmp/diag_{tag}.txt','w').write(o)
    return o
o9 = runbin('v9', f'{B}/div_v9')
oo = runbin('orig', f'{B}/div_orig')
gold = vmctl.run(f"cat /tmp/diag_gold.txt")[1]
# 归一化 hex 比对（去前导零/小写）
def norm(s):
    return re.sub(r'(?i)(?<![0-9a-f])0+(?=[0-9a-f])','', s.strip().lower())
print("V9   :", norm(o9))
print("ORIG :", norm(oo))
print("GOLD :", norm(gold))
print("MATCH v9==gold:", norm(o9)==norm(gold), " orig==gold:", norm(oo)==norm(gold))
