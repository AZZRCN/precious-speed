import os, sys, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl
H = "/home/azzr/divbench"; B = f"{H}/bin"
NL = chr(10)
bad = "/home/azzr/divbench/logs/bad_d8_small_1.in"
rc,head,_ = vmctl.run(f"head -c 120 {bad}; echo; echo '---LINES---'; wc -l {bad}")
print("BAD HEAD:", head.replace(NL,' | '))
def runbin(bp):
    rc,o,_ = vmctl.run(f"cd {H} && {bp} < {bad}")
    return o
o9 = runbin(f'{B}/div_v9'); oo = runbin(f'{B}/div_orig')
print("V9  out[:2]:", o9.split(NL)[:2])
print("ORIG out[:2]:", oo.split(NL)[:2])
# gold via python: hex parse
py = ("data=open('/home/azzr/divbench/logs/bad_d8_small_1.in').read().split()" + NL
      "T=int(data[0]); idx=1; out=[]" + NL
      "for _ in range(T):" + NL
      "  a=int(data[idx],16); b=int(data[idx+1],16); idx+=2" + NL
      "  out.append(format(a//b,'x')+' '+format(a%b,'x'))" + NL
      "open('/tmp/bad_gold.txt','w').write('".replace("'","'") + NL.join(["'"])*0 + chr(10).join(["x"]) + "')")
# simpler: write via separate echo
vmctl.run("python3 - <<'PYEOF'" + NL
          + "data=open('/home/azzr/divbench/logs/bad_d8_small_1.in').read().split()" + NL
          + "T=int(data[0]); idx=1; out=[]" + NL
          + "for _ in range(T):" + NL
          + "  a=int(data[idx],16); b=int(data[idx+1],16); idx+=2" + NL
          + "  out.append(format(a//b,'x')+' '+format(a%b,'x'))" + NL
          + "open('/tmp/bad_gold.txt','w').write(chr(10).join(out)+chr(10))" + NL
          + "PYEOF")
gold = vmctl.run("cat /tmp/bad_gold.txt")[1]
gl = gold.split(NL)
v9l = o9.split(NL); ool = oo.split(NL)
print("GOLD[:2]:", gl[:2])
def n(s): return s.strip().lower()
print("v9==gold  row0:", n(v9l[0])==n(gl[0]) if gl else 'NA')
print("orig==gold row0:", n(ool[0])==n(gl[0]) if gl else 'NA')
# 看输入第一行 a/b 的 hex 位数
inp = open if False else None
dat = vmctl.run(f"sed -n '2p' {bad}")[1].split()
if len(dat)>=2:
    print("row1 la/lb hexdigits:", len(dat[0]), len(dat[1]))
