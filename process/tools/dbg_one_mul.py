import sys, random, vm_ssh
sys.path.insert(0, '.')
HEX='0123456789abcdef'
def rh(rng,n): 
    s=''.join(rng.choice(HEX) for _ in range(n)); return rng.choice('123456789abcdef')+s[1:]
rng=random.Random(12345)
na,nb=150,150
a=rh(rng,16*na); b=rh(rng,16*nb)
exp=int(a,16)*int(b,16)
open('D:/precious_speed/tools/_m.txt','w').write(f"{a}\n{b}\n")
vm_ssh.put('D:/precious_speed/tools/_m.txt','/home/azzr/precious_speed/dbg/_m.txt')
rc,o,e=vm_ssh.run("cd /home/azzr/precious_speed/dbg && ./mulgtest.bin < _m.txt")
got=o.strip()
print("got_len",len(got),"exp_len",len(format(exp,'x')))
try:
    g=int(got,16); print("VALUE MATCH" if g==exp else "VALUE MISMATCH")
    if g!=exp:
        d=g-exp
        print("diff bits set:", d.bit_length())
        print("diff top hex:", format(d if d>0 else -d,'x')[:60])
        # compare limb-wise
        import math
        gl=[(g>>(64*i))&((1<<64)-1) for i in range((g.bit_length()+63)//64)]
        el=[(exp>>(64*i))&((1<<64)-1) for i in range((exp.bit_length()+63)//64)]
        print("got limbs",len(gl),"exp limbs",len(el))
        # find first differing limb
        for i in range(max(len(gl),len(el))):
            gv=gl[i] if i<len(gl) else 0; ev=el[i] if i<len(el) else 0
            if gv!=ev:
                print(f"first diff limb {i}: got={gv:#018x} exp={ev:#018x} xor={gv^ev:#018x}"); break
except ValueError as ex:
    print("parse err", ex, "got[:80]=", got[:80])
