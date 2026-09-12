import io, sys, re

src = open(r'D:\precious_speed\hex_best\div.cpp', 'r').read()
# locate the real main
idx = src.index('int main() {')
head = src[:idx]

diag = r'''
static u64 Qk[MAXC+8], Rk[MAXC+8], Qn[MAXC+8], Rn[MAXC+8];
static u64 T2[2*MAXC+16];

int main() {
    hugify(inbuf_, sizeof inbuf_);
    hugify(A, sizeof A); hugify(B, sizeof B);
    hugify(AN, sizeof AN); hugify(BN, sizeof BN);
    hugify(AS, sizeof AS); hugify(BS, sizeof BS); hugify(VB, sizeof VB);
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
        int qn = na - nb + 1;

        if (na==1 && A[0]==0) { continue; }
        if (mag_cmp(A,na,B,nb) < 0) { continue; }
        if (nb==1) { continue; }

        knuthD(A, na, B, nb, Qk, Rk);
        bool newton_valid = (na <= 2*nb);
        bool match = true; bool consist = true;
        if (newton_valid) {
            wp = WORK;
            newton_divide(A, na, B, nb, Qn, Rn);
            int nQk=qn, nQn=qn; while(nQk>1&&Qk[nQk-1]==0)--nQk; while(nQn>1&&Qn[nQn-1]==0)--nQn;
            if (nQk!=nQn || std::memcmp(Qk,Qn,(size_t)nQk*8)!=0) match=false;
            int nRk=nb,nRn=nb; while(nRk>1&&Rk[nRk-1]==0)--nRk; while(nRn>1&&Rn[nRn-1]==0)--nRn;
            if (nRk!=nRn || std::memcmp(Rk,Rn,(size_t)nRk*8)!=0) match=false;
            std::memset(T2,0,(size_t)(qn+nb+2)*8);
            mulg(Qn,qn,B,nb,T2);
            unsigned char c=0;
            for(int i=0;i<nb;++i){ u64 s; c=_addcarry_u64(c,T2[i],Rn[i],(unsigned long long*)&s); T2[i]=s; }
            if(c){ for(int i=nb;c&&i<qn+nb+1;++i){ u64 s; c=_addcarry_u64(c,T2[i],0,(unsigned long long*)&s); T2[i]=s; } }
            int nT=qn+nb+1; while(nT>1&&T2[nT-1]==0)--nT;
            int nA=na; while(nA>1&&A[nA-1]==0)--nA;
            if(nT!=nA || std::memcmp(T2,A,(size_t)nA*8)!=0) consist=false;
        } else {
            lp = snprintf(line, sizeof line, "CASE %u NA=%d NB=%d qn=%d NEWTON_NA (skipped)\n", t, na, nb, na-nb+1);
            write(1, line, lp);
            continue;
        }
        lp = snprintf(line, sizeof line, "CASE %u NA=%d NB=%d qn=%d MATCH=%d CONSIST=%d\n",
                      t, na, nb, na-nb+1, (int)match, (int)consist);
        write(1, line, lp);
        if (!match || !consist) {
            char* e;
            lp = snprintf(line, sizeof line, "  Qk="); write(1,line,lp);
            e = put_big(line, Qk, qn); write(1, line, (int)(e-line)); write(1, "\n",1);
            lp = snprintf(line, sizeof line, "  Qn="); write(1,line,lp);
            e = put_big(line, Qn, qn); write(1, line, (int)(e-line)); write(1, "\n",1);
            lp = snprintf(line, sizeof line, "  Rk="); write(1,line,lp);
            e = put_big(line, Rk, nb); write(1, line, (int)(e-line)); write(1, "\n",1);
            lp = snprintf(line, sizeof line, "  Rn="); write(1,line,lp);
            e = put_big(line, Rn, nb); write(1, line, (int)(e-line)); write(1, "\n",1);
        }
    }
    return 0;
}
'''

out = head.replace('#include <cmath>', '#include <cmath>\n#include <cstdio>') + diag
open(r'D:\precious_speed\tools\ndiag2.cpp', 'w').write(out)
print("written", len(out), "bytes")
