import io

src = open(r'D:\precious_speed\hex_best\div.cpp', 'r').read()
idx = src.index('int main() {')
head = src[:idx]

diag = r'''
static u64 TMP[2*MAXC+16];

int main() {
    hugify(inbuf_, sizeof inbuf_);
    hugify(A, sizeof A); hugify(B, sizeof B);
    hugify(WORK, sizeof WORK);
    hugify(FB, sizeof FB); hugify(GB, sizeof GB);
    hugify(FMG1, sizeof FMG1); hugify(FMG2, sizeof FMG2);
    int len = 0;
    for (;;) { long r = read(0, inbuf+len, INCAP-len); if (r<=0) break; len += (int)r; }
    std::memset(inbuf+len, 0, 96);
    inbuf[len] = '\n';
    const char* p = inbuf;
    while (*p < '0') ++p;
    u32 T = 0;
    while (*p > ' ') T = T*10 + (u32)(*p++ - '0');

    char line[1<<16]; int lp;
    for (u32 t = 0; t < T; ++t) {
        while (*p <= ' ') ++p;
        const char* a0 = p; int la = tok_len(p); p += la;
        while (*p <= ' ') ++p;
        const char* b0 = p; int lb = tok_len(p); p += lb;

        int na = parse_limbs(a0, la, A);
        int nb = parse_limbs(b0, lb, B);
        while (na>1 && A[na-1]==0) --na;
        while (nb>1 && B[nb-1]==0) --nb;
        if (na==1 && A[0]==0) continue;
        if (mag_cmp(A,na,B,nb)<0) continue;
        if (nb==1) continue;

        const int n = nb;
        const int sigma = (int)_lzcnt_u64(B[n-1]);
        u64* dn = TMP;
        u64* an = TMP + n + 4;
        if (sigma) {
            unsigned char c=0;
            for (int i=0;i<n;++i){ u64 cur=B[i]; dn[i]=(cur<<sigma)|c; c=cur>>(64-sigma); }
            c=0;
            for (int i=0;i<na;++i){ u64 cur=A[i]; an[i]=(cur<<sigma)|c; c=cur>>(64-sigma); }
            an[na]=c;
        } else {
            std::memcpy(dn,B,(size_t)n*8);
            std::memcpy(an,A,(size_t)na*8); an[na]=0;
        }
        const int na_an = na+1;
        wp = WORK;
        u64* v = WORK + 0;
        u64* vv = v + n + 4;
        invertappr(dn, n, vv);
        // L = an * vv
        u64* L = vv + n + 4;
        mulg(an, na_an, vv, n, L);
        // true q = knuthD
        u64* Qk = L + na_an + n + 4;
        u64* Rk = Qk + (na-nb+1) + 4;
        knuthD(A, na, B, nb, Qk, Rk);
        // compute q_est
        int qn = na - n + 1;
        u64* qe = Rk + nb + 4;
        std::memset(qe,0,(size_t)(qn+1)*8);
        unsigned char c=0;
        for (int i=0;i<qn;++i){
            u64 x=(n+i<na_an)?an[n+i]:0;
            u64 y=(2*n+i<na_an+n)?L[2*n+i]:0;
            u64 s; c=_addcarry_u64(c,x,y,(unsigned long long*)&s); qe[i]=s;
        }
        qe[qn]=c;
        { // mod carry
            unsigned char cc=0;
            for (int i=0;i<2*n;++i){
                u64 x=(i>=n)?an[i-n]:0;
                u64 s; cc=_addcarry_u64(cc,x,L[i],(unsigned long long*)&s);
            }
            if (cc){ unsigned char c2=1; for(int i=0;i<qn+1&&c2;++i){ u64 cur=qe[i]; qe[i]=cur+c2; c2=(cur+c2<cur);} }
        }
        // output q_est and true q
        lp = snprintf(line,sizeof line,"CASE %u n=%d qn=%d QEST=",t,n,qn); write(1,line,lp);
        char* e=put_big(line,qe,qn); write(1,line,(int)(e-line)); write(1,"\n",1);
        lp = snprintf(line,sizeof line,"  TRUEQ="); write(1,line,lp);
        e=put_big(line,Qk,qn); write(1,line,(int)(e-line)); write(1,"\n",1);
    }
    return 0;
}
'''

out = head.replace('#include <cmath>', '#include <cmath>\n#include <cstdio>') + diag
open(r'D:\precious_speed\tools\ndiag6.cpp', 'w').write(out)
print("written", len(out))
