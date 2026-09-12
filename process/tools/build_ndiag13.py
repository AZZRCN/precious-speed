import sys

src = open(r'D:\precious_speed\hex_best\div.cpp', 'r').read()
idx = src.index('int main() {')
head = src[:idx]

head = head.replace('#include <cmath>', '#include <cmath>\n#include <cstdio>')
head = head.replace('using u64 = uint64_t;',
    'using u64 = uint64_t;\n'
    'static u64 gAN[8000], gDN[8000];\n'
    'static int gNAANc=0, gNc=0;\n')
# capture an, dn at end of newton_divide
anchor = '    } else {\n        std::memcpy(r, Rt, (size_t)n * 8);\n    }\n    wp = save;\n}'
assert anchor in head
repl = ('    } else {\n        std::memcpy(r, Rt, (size_t)n * 8);\n    }\n'
        '    std::memcpy(gDN, dn, (size_t)n * 8);\n'
        '    std::memcpy(gAN, an, (size_t)na_an * 8);\n'
        '    gNAANc = na_an; gNc = n;\n'
        '    wp = save;\n}')
head = head.replace(anchor, repl)

diag = r'''
static u64 Qk[MAXC+8], Rk[MAXC+8], Qn[MAXC+8], Rn[MAXC+8];
static u64 Qan[8000], Ran[8000];

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

        // true an//dn via knuthD on the captured (normalized) an, dn
        knuthD(gAN, gNAANc, gDN, gNc, Qan, Ran);
        int nQan = qn; while(nQan>1 && Qan[nQan-1]==0) --nQan;
        int nQn = qn;  while(nQn>1 && Qn[nQn-1]==0) --nQn;
        int nQk = qn;  while(nQk>1 && Qk[nQk-1]==0) --nQk;
        bool qn_eq_anDIVdn = (nQan==nQn) && (std::memcmp(Qan,Qn,(size_t)nQn*8)==0);
        bool anDIVdn_eq_AdivB = (nQan==nQk) && (std::memcmp(Qan,Qk,(size_t)nQk*8)==0);
        bool match = (nQk==nQn) && (std::memcmp(Qk,Qn,(size_t)nQn*8)==0);
        printf("CASE %u na=%d nb=%d qn=%d Qn_eq_anDIVdn=%d anDIVdn_eq_AdivB=%d match=%d\n",
               t, na, nb, qn, qn_eq_anDIVdn?1:0, anDIVdn_eq_AdivB?1:0, match?1:0);
    }
    return 0;
}
'''

out = head + diag
open(r'D:\precious_speed\tools\ndiag13.cpp', 'w').write(out)
print("written", len(out), "bytes")
