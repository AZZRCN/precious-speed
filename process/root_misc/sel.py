import glob, os

def pick_k(u):
    gk=[19<<4,18<<6,17<<8,16<<10,15<<12,14<<14,13<<16,12<<18,11<<20,10<<22,(1<<63)]
    i=0
    while u>gk[i]: i+=1
    return 19-i
def nxt_pow2(c):
    if c&(c-1)==0: return c
    return 1<<c.bit_length()
def fft_ceil_tiers(c):
    p=nxt_pow2(c); best=p
    if c!=p and p>=1024:
        h=(p>>2)*3
        if h>=c and h<best: best=h
        h5=(p>>3)*5
        if h5>=c and h5<best: best=h5
    return best
def path_of(lm):
    return 3 if lm%3==0 else (5 if lm%5==0 else 2)
def ceilq(x,k): return (x*64+k-1)//k

res=[]
for f in glob.glob('cases_dir/*.in')+glob.glob('cases_t2/*.in'):
    data=open(f).read().split()
    if not data: continue
    T=int(data[0]); idx=1
    paths=set(); zhB=False; zhA=False
    for _ in range(T):
        a=data[idx]; b=data[idx+1]; idx+=2
        na=len(a)//16; nb=len(b)//16; u=na+nb
        k=pick_k(u); lm=fft_ceil_tiers(ceilq(u,k)); p=path_of(lm)
        ta=ceilq(na,k); tb=ceilq(nb,k); half=lm>>1
        paths.add(p)
        if tb<=half: zhB=True
        if ta<=half: zhA=True
    if (3 in paths or 5 in paths):
        res.append((os.path.getsize(f), f, sorted(paths), zhA, zhB))
res.sort(reverse=True)
print("== 走 path3/5 的文件 (按大小降序, 前12) ==")
for sz,f,paths,zhA,zhB in res[:12]:
    print('%9d  %-38s paths=%s zhA=%s zhB=%s'%(sz, os.path.basename(f), paths, zhA, zhB))
