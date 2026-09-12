import sys

src = open(r'D:\precious_speed\hex_best\div.cpp', 'r').read()
idx = src.index('int main() {')
head = src[:idx]

head = head.replace('#include <cmath>', '#include <cmath>\n#include <cstdio>')

diag = r'''
static u64 Qk[MAXC+8], Rk[MAXC+8], Qn[MAXC+8], Rn[MAXC+8];

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

    printf("T=%u\n", T);
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

        if (na==1 && A[0]==0) { printf("CASE %u SKIP0\n", t); continue; }
        if (mag_cmp(A,na,B,nb) < 0) { printf("CASE %u SKIPA\n", t); continue; }
        if (nb==1) { printf("CASE %u SKIP1\n", t); continue; }

        knuthD(A, na, B, nb, Qk, Rk);
        bool newton_valid = (na <= 2*nb);
        if (!newton_valid) { printf("CASE %u NBZ\n", t); continue; }

        wp = WORK;
        newton_divide(A, na, B, nb, Qn, Rn);

        // ground-truth: Qn*B + Rn == A ?  Rn < B ?
        u64* P = wp; wp += na + nb + 2; std::memset(P, 0, (size_t)(na+nb+2)*8);
        mulg(Qn, qn, B, nb, P);                 // P = Qn * B
        unsigned char ad = 0;
        for (int i = 0; i < nb; ++i) { u64 s; ad = _addcarry_u64(ad, P[i], Rn[i], (unsigned long long*)&s); P[i] = s; }
        for (int i = nb; i < na+nb+1 && ad; ++i) { u64 s; ad = _addcarry_u64(ad, P[i], 0, (unsigned long long*)&s); P[i] = s; }
        bool prod_eq_A = (std::memcmp(P, A, (size_t)na*8)==0) && (P[na]==0);
        bool r_lt_b = (mag_cmp(Rn, nb, B, nb) < 0);
        int nQk=qn, nQn=qn; while(nQk>1&&Qk[nQk-1]==0)--nQk; while(nQn>1&&Qn[nQn-1]==0)--nQn;
        bool match = (nQk==nQn) && (std::memcmp(Qk,Qn,(size_t)nQk*8)==0);

        // find first differing limb between Qn and Qk
        int diff_limb = -1;
        for (int i = 0; i < qn; ++i) if (Qn[i] != Qk[i]) { diff_limb = i; break; }
        printf("CASE %u na=%d nb=%d qn=%d prod_eq_A=%d r_lt_b=%d match=%d diff_limb=%d\n",
               t, na, nb, qn, prod_eq_A?1:0, r_lt_b?1:0, match?1:0, diff_limb);
    }
    return 0;
}
'''

out = head + diag
open(r'D:\precious_speed\tools\ndiag12.cpp', 'w').write(out)
print("written", len(out), "bytes")
