import sys, random, vm_ssh
sys.path.insert(0,'.')
HEX='0123456789abcdef'
def rh(rng,n):
    s=''.join(rng.choice(HEX) for _ in range(n)); return rng.choice('123456789abcdef')+s[1:]
rng=random.Random(999)
a=rh(rng,32); b=rh(rng,32)
exp=int(a,16)*int(b,16)
exp_hex=format(exp,'x')
def run(mode):
    open('D:/precious_speed/tools/_m.txt','w').write(f"{a}\n{b}\n{mode}\n")
    vm_ssh.put('D:/precious_speed/tools/_m.txt','/home/azzr/precious_speed/dbg/_m.txt')
    rc,o,e=vm_ssh.run("cd /home/azzr/precious_speed/dbg && ./dbgmul.bin < _m.txt")
    got=o.strip()
    g=int(got,16)
    return got, ("OK" if g==exp else f"BAD ratio={g//exp if exp else 0}")
for mode in ["bf","fft","mulg"]:
    got,st=run(mode)
    print(f"mode={mode}: {st}")
    if st!="OK":
        print(f"   exp={exp_hex}")
        print(f"   got={got}")
