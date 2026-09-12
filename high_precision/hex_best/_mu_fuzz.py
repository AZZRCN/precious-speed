import subprocess, random, os, sys
sys.set_int_max_str_digits(20000000)
EXE = r"D:\precious_speed\hex_best\_cyc_test.exe"
random.seed(int(sys.argv[1]) if len(sys.argv)>1 else 7)
N = int(sys.argv[2]) if len(sys.argv)>2 else 300
cases=[]
for _ in range(N):
    nb = random.choice([random.randint(2,8), random.randint(50,300),
                        random.randint(300,1200), random.choice([256,512,1024,2048])])
    # na 覆盖 2nb+1 .. 9nb (mu 路由需 na>2nb)
    na = random.choice([2*nb+1, 2*nb+2, 3*nb-1, 3*nb, 3*nb+1,
                        random.randint(2*nb+1, max(2*nb+2, 9*nb))])
    B = random.getrandbits(64*nb) | (1 << (64*(nb-1)))
    # 强制多样 sigma: 随机右移顶 limb
    sh = random.randint(0,63)
    B >>= sh
    B |= (1 << (64*(nb-1)))              # 保持 nb limb
    A = random.getrandbits(64*na) | (1 << (64*(na-1)))
    cases.append((A,B,nb,na))
inp = [str(len(cases))] + [format(A,'X')+" "+format(B,'X') for A,B,_,_ in cases]
src = "\n".join(inp)+"\n"
r = subprocess.run([EXE], input=src, capture_output=True, text=True,
                   env={**os.environ, "MU":"1"})
if r.returncode != 0:
    print("RUN FAILED rc=",r.returncode); print(r.stderr[:1500]); sys.exit(1)
out = r.stdout.strip().split("\n")
assert len(out)==len(cases), (len(out),len(cases))
fails=0
for i,(A,B,nb,na) in enumerate(cases):
    p=out[i].split()
    qg,rg=int(p[0],16),int(p[1],16)
    if qg!=A//B or rg!=A%B:
        fails+=1
        top=(B>>(64*(nb-1)))&0xFFFFFFFFFFFFFFFF
        s=0;v=top
        while v and not (v&(1<<63)): v<<=1; s+=1
        if fails<=8: print(f"FAIL {i} nb={nb} na={na} sigma={s} q_ok={qg==A//B} r_ok={rg==A%B}")
print(f"FUZZ {len(cases)} cases FAILS={fails}")
