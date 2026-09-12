import io, sys, re

src = open(r'D:\precious_speed\hex_best\div.cpp', 'r').read()
idx = src.index('int main() {')
head = src[:idx]

# inject capture globals + helpers (after u64 typedef so u64 is known)
head = head.replace('#include <cmath>', '#include <cmath>\n#include <cstdio>')
head = head.replace('using u64 = uint64_t;',
    'using u64 = uint64_t;\n'
    'static u64 gDN[8000], gAN[8000], gV[8000], gL[12000], gQe[8000], gRt[8000];\n'
    'static u64 gV2[8000], gL2[12000];\n'
    'static bool gCaptured=false; static int gN=0,gNA=0,gNAAN=0,gQN=0;\n'
    'static void dumphex(const char* tag, const u64* p, int n){\n'
    '  fputs(tag, stdout); fputc(\':\', stdout);\n'
    '  for (int i=n-1;i>=0;--i) printf("%016llx", (unsigned long long)p[i]);\n'
    '  fputc(\'\\n\', stdout);\n'
    '}\n')

# inject capture copies before final wp=save in newton_divide
anchor = '    } else {\n        std::memcpy(r, Rt, (size_t)n * 8);\n    }\n    wp = save;\n}'
assert anchor in head, "newton_divide anchor not found"
repl = ('    } else {\n        std::memcpy(r, Rt, (size_t)n * 8);\n    }\n'
        '    std::memcpy(gDN, dn, (size_t)n * 8);\n'
        '    std::memcpy(gAN, an, (size_t)na_an * 8);\n'
        '    std::memcpy(gV, v, (size_t)n * 8);\n'
        '    std::memcpy(gL, L, (size_t)(na_an + n) * 8);\n'
        '    std::memcpy(gQe, qe, (size_t)qn * 8);\n'
        '    std::memcpy(gRt, Rt, (size_t)na_an * 8);\n'
        '    gCaptured = true; gN = n; gNA = na; gNAAN = na_an; gQN = qn;\n'
        '    wp = save;\n}')
head = head.replace(anchor, repl)

diag = r'''
static u64 Qk[MAXC+8], Rk[MAXC+8], Qn[MAXC+8], Rn[MAXC+8];
static u64 T2[2*MAXC+16];
static u64 dn2[8000], v2[8000];

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
        if (!newton_valid) {
            lp = snprintf(line, sizeof line, "CASE %u NA=%d NB=%d qn=%d NEWTON_NA\n", t, na, nb, na-nb+1);
            write(1, line, lp);
            continue;
        }
        wp = WORK;
        newton_divide(A, na, B, nb, Qn, Rn);

        // standalone re-run on captured dn/v to test context sensitivity
        std::memcpy(dn2, gDN, (size_t)gN * 8);
        wp = WORK;
        invertappr(dn2, gN, v2);

        int nQk=qn, nQn=qn; while(nQk>1&&Qk[nQk-1]==0)--nQk; while(nQn>1&&Qn[nQn-1]==0)--nQn;
        bool match = (nQk==nQn) && (std::memcmp(Qk,Qn,(size_t)nQk*8)==0);
        // V context check
        bool vctx = (std::memcmp(gV, v2, (size_t)gN*8)==0);

        if (match) {
            lp = snprintf(line, sizeof line, "CASE %u NA=%d NB=%d qn=%d PASS VCTX=%d\n", t, na, nb, qn, (int)vctx);
            write(1, line, lp);
            continue;
        }
        // FAIL: dump internals
        lp = snprintf(line, sizeof line, "CASE %u NA=%d NB=%d qn=%d FAIL VCTX=%d\n", t, na, nb, qn, (int)vctx);
        write(1, line, lp);
        dumphex("DN", gDN, gN);
        dumphex("AN", gAN, gNAAN);
        dumphex("V",  gV,  gN);
        dumphex("V2", gV2, gN);
        dumphex("L",  gL,  gNAAN+gN);
        dumphex("QE", gQe, gQN);
        dumphex("RT", gRt, gNAAN);
        dumphex("Qn", Qn, qn);
        dumphex("Qk", Qk, qn);
        dumphex("Rk", Rk, nb);
    }
    return 0;
}
'''

out = head + diag
open(r'D:\precious_speed\tools\ndiag9.cpp', 'w').write(out)
print("written", len(out), "bytes")
