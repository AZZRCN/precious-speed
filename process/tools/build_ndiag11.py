import sys

src = open(r'D:\precious_speed\hex_best\div.cpp', 'r').read()
idx = src.index('int main() {')
head = src[:idx]

# globals + helpers (after u64 typedef)
head = head.replace('#include <cmath>', '#include <cmath>\n#include <cstdio>')
head = head.replace('using u64 = uint64_t;',
    'using u64 = uint64_t;\n'
    'static u64 gD_in[8000], gB0cmp[8000];\n'
    'static int gSig=0, gDEqB0=0, gNormOk=0, gAEqA0=0;\n')

# capture input divisor d at start of newton_divide + record sigma
head = head.replace('    const int sigma = (int)_lzcnt_u64(d[n - 1]);   // 0..63',
    '    const int sigma = (int)_lzcnt_u64(d[n - 1]);   // 0..63\n'
    '    std::memcpy(gD_in, d, (size_t)n * 8);\n'
    '    gSig = sigma;\n')

# at end of newton_divide: verify normalization + d==B0
anchor = '    } else {\n        std::memcpy(r, Rt, (size_t)n * 8);\n    }\n    wp = save;\n}'
assert anchor in head
repl = ('    } else {\n        std::memcpy(r, Rt, (size_t)n * 8);\n    }\n'
        '    gNormOk = 1; gAEqA0 = 1;\n'
        '    if (sigma) {\n'
        '        unsigned char c = 0;\n'
        '        for (int i = 0; i < n; ++i) { u64 cur = d[i]; u64 e = (cur << sigma) | c; c = cur >> (64 - sigma); if (e != dn[i]) { gNormOk = 0; break; } }\n'
        '        c = 0;\n'
        '        for (int i = 0; i < na; ++i) { u64 cur = a[i]; u64 e = (cur << sigma) | c; c = cur >> (64 - sigma); if (e != an[i]) { gAEqA0 = 0; break; } }\n'
        '        if (an[na] != c) gAEqA0 = 0;\n'
        '    } else {\n'
        '        for (int i = 0; i < n; ++i) if (dn[i] != d[i]) { gNormOk = 0; break; }\n'
        '        for (int i = 0; i < na; ++i) if (an[i] != a[i]) { gAEqA0 = 0; break; }\n'
        '        if (an[na] != 0) gAEqA0 = 0;\n'
        '    }\n'
        '    gDEqB0 = 1;\n'
        '    for (int i = 0; i < n; ++i) if (gD_in[i] != gB0cmp[i]) { gDEqB0 = 0; break; }\n'
        '    wp = save;\n}')
head = head.replace(anchor, repl)

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
        std::memcpy(gB0cmp, B, (size_t)nb*8);

        if (na==1 && A[0]==0) { printf("CASE %u SKIP0 na=%d nb=%d\n", t, na, nb); continue; }
        if (mag_cmp(A,na,B,nb) < 0) { printf("CASE %u SKIPA na=%d nb=%d\n", t, na, nb); continue; }
        if (nb==1) { printf("CASE %u SKIP1 na=%d nb=%d\n", t, na, nb); continue; }

        knuthD(A, na, B, nb, Qk, Rk);
        bool newton_valid = (na <= 2*nb);
        if (!newton_valid) { printf("CASE %u NBZ na=%d nb=%d\n", t, na, nb); continue; }

        wp = WORK;
        newton_divide(A, na, B, nb, Qn, Rn);

        int nQk=qn, nQn=qn; while(nQk>1&&Qk[nQk-1]==0)--nQk; while(nQn>1&&Qn[nQn-1]==0)--nQn;
        bool match = (nQk==nQn) && (std::memcmp(Qk,Qn,(size_t)nQk*8)==0);
        int n = nb;
        printf("CASE %u na=%d nb=%d n=%d sigma=%d d_eq_B0=%d norm_ok=%d a_eq_A0=%d match=%d\n",
               t, na, nb, n, gSig, gDEqB0, gNormOk, gAEqA0, match?1:0);
    }
    return 0;
}
'''

out = head + diag
open(r'D:\precious_speed\tools\ndiag11.cpp', 'w').write(out)
print("written", len(out), "bytes")
