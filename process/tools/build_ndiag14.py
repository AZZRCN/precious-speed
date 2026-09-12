import sys

src = open(r'D:\precious_speed\hex_best\div.cpp', 'r').read()
idx = src.index('int main() {')
head = src[:idx]

head = head.replace('#include <cmath>', '#include <cmath>\n#include <cstdio>')
head = head.replace('using u64 = uint64_t;',
    'using u64 = uint64_t;\n'
    'static u64 gAN[8000], gDN[8000], gD_in[8000], gB0cmp[8000], gDNearly[8000];\n'
    'static int gNAANc=0, gNc=0;\n')
head = head.replace('    const int sigma = (int)_lzcnt_u64(d[n - 1]);   // 0..63',
    '    const int sigma = (int)_lzcnt_u64(d[n - 1]);   // 0..63\n'
    '    std::memcpy(gD_in, d, (size_t)n * 8);\n')
anchor = '    } else {\n        std::memcpy(r, Rt, (size_t)n * 8);\n    }\n    wp = save;\n}'
assert anchor in head
repl = ('    } else {\n        std::memcpy(r, Rt, (size_t)n * 8);\n    }\n'
        '    std::memcpy(gDN, dn, (size_t)n * 8);\n'
        '    std::memcpy(gAN, an, (size_t)na_an * 8);\n'
        '    gNAANc = na_an; gNc = n;\n'
        '    wp = save;\n}')
head = head.replace('    const int na_an = na + 1;\n    invertappr(dn, n, v);',
    '    const int na_an = na + 1;\n    std::memcpy(gDNearly, dn, (size_t)n * 8);\n    invertappr(dn, n, v);')
head = head.replace(anchor, repl)

diag = r'''
static u64 Qk[MAXC+8], Rk[MAXC+8], Qn[MAXC+8], Rn[MAXC+8];

static void prhex(const u64* p, int n) {
    for (int i = n-1; i >= 0; --i) printf("%016llx", (unsigned long long)p[i]);
}

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
        std::memcpy(gB0cmp, B, (size_t)nb*8);
        if (na==1 && A[0]==0) { printf("C %u S0\n", t); continue; }
        if (mag_cmp(A,na,B,nb) < 0) { printf("C %u SA\n", t); continue; }
        if (nb==1) { printf("C %u S1\n", t); continue; }
        knuthD(A, na, B, nb, Qk, Rk);
        bool newton_valid = (na <= 2*nb);
        if (!newton_valid) { printf("C %u NBZ\n", t); continue; }
        wp = WORK;
        newton_divide(A, na, B, nb, Qn, Rn);
        int n = nb;
        int sig = (int)_lzcnt_u64(B[nb-1]);
        printf("C %u na=%d nb=%d n=%d na_an=%d sigma=%d\n", t, na, nb, n, gNAANc, sig);
        printf("AN "); prhex(gAN, gNAANc); printf("\n");
        printf("DN "); prhex(gDN, gNc); printf("\n");
        printf("DE "); prhex(gDNearly, gNc); printf("\n");
        printf("DI "); prhex(gD_in, gNc); printf("\n");
        printf("BO "); prhex(gB0cmp, nb); printf("\n");
    }
    return 0;
}
'''

out = head + diag
open(r'D:\precious_speed\tools\ndiag14.cpp', 'w').write(out)
print("written", len(out), "bytes")
