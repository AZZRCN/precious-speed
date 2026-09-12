import sys, random, vm_ssh
sys.path.insert(0,'.')
HEX='0123456789abcdef'
def rh(rng,n):
    s=''.join(rng.choice(HEX) for _ in range(n)); return rng.choice('123456789abcdef')+s[1:]
rng=random.Random(999)
a=rh(rng,32); b=rh(rng,32)  # na=nb=2
exp=int(a,16)*int(b,16)
open('D:/precious_speed/tools/_m.txt','w').write(f"{a}\n{b}\n")
vm_ssh.put('D:/precious_speed/tools/_m.txt','/home/azzr/precious_speed/dbg/_m.txt')
rc,o,e=vm_ssh.run("cd /home/azzr/precious_speed/dbg && ./mulgtest.bin < _m.txt")
got=o.strip()
print("a   =",a)
print("b   =",b)
print("exp =",format(exp,'x'))
print("got =",got)
print("got_int/exp =", int(got,16)//exp if exp else 'inf')
