import sys, random, vm_ssh
sys.path.insert(0,'.')
HEX='0123456789abcdef'
def rh(rng,n):
    s=''.join(rng.choice(HEX) for _ in range(n)); return rng.choice('123456789abcdef')+s[1:]
rng=random.Random(999)
def check(a,b):
    exp=int(a,16)*int(b,16)
    open('D:/precious_speed/tools/_m.txt','w').write(f"{a}\n{b}\n")
    vm_ssh.put('D:/precious_speed/tools/_m.txt','/home/azzr/precious_speed/dbg/_m.txt')
    rc,o,e=vm_ssh.run("cd /home/azzr/precious_speed/dbg && ./mulgtest.bin < _m.txt")
    got=o.strip()
    try: g=int(got,16)
    except: return f"PARSE_FAIL got[:40]={got[:40]}"
    if g==exp: return "OK"
    ratio = g/exp if exp else 0
    return f"BAD ratio={ratio} (got/exp)"
for na,nb in [(2,2),(10,10),(49,49),(50,50),(60,60),(100,100),(149,149),(150,150),(200,200)]:
    a=rh(rng,16*na); b=rh(rng,16*nb)
    print(f"na={na} nb={nb}: {check(a,b)}")
