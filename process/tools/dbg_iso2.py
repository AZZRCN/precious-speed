import sys, random, vm_ssh
sys.path.insert(0,'.')
HEX='0123456789abcdef'
def rh(rng,n):
    s=''.join(rng.choice(HEX) for _ in range(n)); return rng.choice('123456789abcdef')+s[1:]
rng=random.Random(7)
def run(a,b,mode):
    open('D:/precious_speed/tools/_m.txt','w').write(f"{a}\n{b}\n{mode}\n")
    vm_ssh.put('D:/precious_speed/tools/_m.txt','/home/azzr/precious_speed/dbg/_m.txt')
    rc,o,e=vm_ssh.run("cd /home/azzr/precious_speed/dbg && ./dbgmul.bin < _m.txt")
    try: g=int(o.strip(),16)
    except: return "PARSE"
    exp=int(a,16)*int(b,16)
    return "OK" if g==exp else f"x{g//exp}"
cases=[
 ("11","11"),                      # 1 limb, 1 limb
 ("1111111111111111","11"),        # 1 limb, 1 limb
 ("1111111111111111","1111111111111111"),  # 1,1
 ("11111111111111111111111111111111","11"),  # 2 limbs, 1 limb
 ("11111111111111111111111111111111","1111111111111111"),  # 2 limbs, 1 limb
 ("11111111111111111111111111111111","11111111111111111111111111111111"),  # 2,2
 ("1111111111111111111111111111111111111111111111111111111111111111","11"), # 4,1
 ("1111111111111111111111111111111111111111111111111111111111111111","11111111111111111111111111111111"), # 4,2
]
for a,b in cases:
    print(f"alen={len(a):2d} blen={len(b):2d} bf={run(a,b,'bf'):>6} fft={run(a,b,'fft'):>6}")
