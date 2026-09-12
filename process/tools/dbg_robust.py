import sys, random, vm_ssh, time
sys.path.insert(0,'.')
HEX='0123456789abcdef'
def rh(rng,n):
    s=''.join(rng.choice(HEX) for _ in range(n)); return rng.choice('123456789abcdef')+s[1:]
rng=random.Random(12345)
def run_case(a,b,mode,ws="/home/azzr/precious_speed/dbg"):
    local='D:/precious_speed/tools/_m.txt'
    with open(local,'w') as f: f.write(f"{a}\n{b}\n{mode}\n")
    vm_ssh.put(local,f"{ws}/_m.txt")
    # verify remote file size
    rc,o,e=vm_ssh.run(f"cd {ws} && wc -c _m.txt")
    rcnt=o.strip().split()[0]
    expect=len(a)+len(b)+len(mode)+3
    if int(rcnt)!=expect:
        return f"SIZE_MISMATCH got={rcnt} exp={expect}"
    rc,o,e=vm_ssh.run(f"cd {ws} && ./dbgmul.bin < _m.txt")
    try: g=int(o.strip(),16)
    except: return f"PARSE got[:60]={o.strip()[:60]}"
    exp=int(a,16)*int(b,16)
    return "OK" if g==exp else f"BAD ratio={g//exp}"
for (na,nb) in [(2,2),(16,16),(50,50),(3292,3010),(6250,6250)]:
    a=rh(rng,16*na); b=rh(rng,16*nb)
    for mode in ["bf","fft","mulg"]:
        print(f"na={na} nb={nb} {mode}: {run_case(a,b,mode)}")
