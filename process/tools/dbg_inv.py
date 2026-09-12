import sys, random, vm_ssh
sys.path.insert(0,'.')
HEX='0123456789abcdef'
def rh(rng,n):
    s=''.join(rng.choice(HEX) for _ in range(n)); return rng.choice('123456789abcdef')+s[1:]
def run(dhex):
    open('D:/precious_speed/tools/_inv.txt','wb').write((dhex+'\n').encode())
    vm_ssh.put('D:/precious_speed/tools/_inv.txt','/home/azzr/precious_speed/dbg/_inv.txt')
    rc,o,e=vm_ssh.run("cd /home/azzr/precious_speed/dbg && ./invtest.bin < _inv.txt")
    return o.strip()
def check(n):
    rng=random.Random(123)
    dhex=rh(rng,16*n)
    d=int(dhex,16)
    beta=1<<64
    trueX=(beta**(2*n))//d
    vtrue=trueX - beta**n
    got=int(run(dhex),16)
    diff=vtrue-got
    print(f"n={n}: got_vs_true diff_bits={diff.bit_length()}  (v<=vtrue means diff>=0)")
    print(f"   got={got.bit_length()}bits vtrue={vtrue.bit_length()}bits  d*v_got vs beta^2n close? diff={diff}")
for n in [48,100,500,1000,2524,2653,3750]:
    check(n)
