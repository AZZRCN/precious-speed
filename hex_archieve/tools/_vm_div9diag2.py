import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl
H = "/home/azzr/divbench"; B = f"{H}/bin"
# 用 VM 端 heredoc 生成输入与黄金，避免本地 repr 转义
heredoc = r'''
import re
a=int('f'*256,16); b=int('f'*128,16)
open('/tmp/diag.in','w').write('1\n'+format(a,'x')+' '+format(b,'x')+'\n')
open('/tmp/diag_gold.txt','w').write(format(a//b,'x')+' '+format(a%b,'x')+'\n')
'''
vmctl.run("cat > /tmp/mkdiag.py <<'PYEOF'\n"+heredoc+"PYEOF\npython3 /tmp/mkdiag.py")
def runbin(tag, binpath):
    rc,o,_ = vmctl.run(f"cd {H} && {binpath} < /tmp/diag.in")
    return o
o9 = runbin('v9', f'{B}/div_v9'); oo = runbin('orig', f'{B}/div_orig')
gold = vmctl.run("cat /tmp/diag_gold.txt")[1]
def norm(s): return re.sub(r'(?i)(?<![0-9a-f])0+(?=[0-9a-f])','', s.strip().lower())
print("V9   :", norm(o9)); print("ORIG :", norm(oo)); print("GOLD :", norm(gold))
print("v9==gold:", norm(o9)==norm(gold), " orig==gold:", norm(oo)==norm(gold))
