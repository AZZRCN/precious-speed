import random, sys
sys.set_int_max_str_digits(2000000)
random.seed(20260813)
def gen_case(nb, r):
    B = random.getrandbits(64*nb)
    if B < (1 << (64*(nb-1))): B |= (1 << (64*(nb-1)))
    na = max(3*nb, int(nb*r))
    A = random.getrandbits(64*na)
    if A < (1 << (64*(na-1))): A |= (1 << (64*(na-1)))
    if A < B: A += B
    return A,B
nbs=[200,256,300,400,500,512,640,800,1000,1024,1280,1500,2000,2500,3000]
cases=[]
for nb in nbs:
    for r in (3.0,5.0,8.0): cases.append((nb,r))
    cases.append((nb,2.0+10.0/nb))
for nb in (4000,5000,8000): cases.append((nb,4.0))
FAILS={2,3,7,9,13,14,16,23,28,29,35,37,40,43,44,45,47,60}
print("idx  nb     sigma  fail?")
for i,(nb,r) in enumerate(cases):
    A,B=gen_case(nb,r)
    top=(B>>(64*(nb-1)))&0xFFFFFFFFFFFFFFFF
    s=0; v=top
    while v and not (v & (1<<63)): v<<=1; s+=1
    carry = ((A<<s).bit_length()+63)//64 > max(3*nb,int(nb*r))
    print(f"{i:3d} {nb:5d} {s:5d}  {'FAIL' if i in FAILS else 'ok  '}  Zcarry={carry}")
