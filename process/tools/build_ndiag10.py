import sys

src = open(r'D:\precious_speed\hex_best\div.cpp', 'r').read()
idx = src.index('int main() {')
head = src[:idx]

# globals + helpers (after u64 typedef)
head = head.replace('#include <cmath>', '#include <cmath>\n#include <cstdio>')
head = head.replace('using u64 = uint64_t;',
    'using u64 = uint64_t;\n'
    'static u64 gDN[8000], gAN[8000], gV[8000], gL[12000], gQe[8000], gRt[8000];\n'
    'static u64 gV2[8000], gL2[12000];\n'
    'static u64 gA0[8000], gB0[8000];\n'
    'static u64 gD_in[8000], gA_in[8000];\n'
    'static int gN=0,gNA=0,gNAAN=0,gQN=0,gSig=0,gNAin=0,gNBin=0;\n'
    'static FILE* gf = nullptr;\n'
    'static void wru32(u32 x){ fwrite(&x,4,1,gf); }\n'
    'static void wru64arr(const u64* p, int n){ fwrite(p,8,(size_t)n,gf); }\n')

# capture input divisor/dividend at start of newton_divide
head = head.replace('    const int sigma = (int)_lzcnt_u64(d[n - 1]);   // 0..63',
    '    const int sigma = (int)_lzcnt_u64(d[n - 1]);   // 0..63\n'
    '    std::memcpy(gD_in, d, (size_t)n * 8);\n'
    '    std::memcpy(gA_in, a, (size_t)na * 8);\n')

# capture copies inside newton_divide (before final wp=save)
anchor = '    } else {\n        std::memcpy(r, Rt, (size_t)n * 8);\n    }\n    wp = save;\n}'
assert anchor in head
repl = ('    } else {\n        std::memcpy(r, Rt, (size_t)n * 8);\n    }\n'
        '    std::memcpy(gDN, dn, (size_t)n * 8);\n'
        '    std::memcpy(gAN, an, (size_t)na_an * 8);\n'
        '    std::memcpy(gV, v, (size_t)n * 8);\n'
        '    std::memcpy(gL, L, (size_t)(na_an + n) * 8);\n'
        '    std::memcpy(gQe, qe, (size_t)qn * 8);\n'
        '    std::memcpy(gRt, Rt, (size_t)na_an * 8);\n'
        '    gN = n; gNA = na; gNAAN = na_an; gQN = qn;\n'
        '    gSig = sigma; gNAin = na; gNBin = nb;\n'
        '    wp = save;\n}')
head = head.replace(anchor, repl)

diag = r'''
static u64 Qk[MAXC+8], Rk[MAXC+8], Qn[MAXC+8], Rn[MAXC+8];
static u64 T2[2*MAXC+16];
static u64 dn2[8000], v2[8000];

int main() {
    gf = fopen("ndiag10.dat", "wb");
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

    wru32(T);
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
        std::memcpy(gA0, A, (size_t)na*8);
        std::memcpy(gB0, B, (size_t)nb*8);

        wru32(t); wru32(na); wru32(nb);

        if (na==1 && A[0]==0) { wru32(2); fwrite("SKIP0",1,5,gf); continue; }
        if (mag_cmp(A,na,B,nb) < 0) { wru32(2); fwrite("SKIPA",1,5,gf); continue; }
        if (nb==1) { wru32(2); fwrite("SKIP1",1,5,gf); continue; }

        knuthD(A, na, B, nb, Qk, Rk);
        bool newton_valid = (na <= 2*nb);
        if (!newton_valid) { wru32(3); fwrite("NBZXX",1,5,gf); continue; }

        wp = WORK;
        newton_divide(A, na, B, nb, Qn, Rn);

        // standalone re-runs on captured dn/v
        std::memcpy(dn2, gDN, (size_t)gN*8);
        wp = WORK; invertappr(dn2, gN, v2);
        wp = WORK; mulg(gAN, gNAAN, gV, gN, gL2);

        int nQk=qn, nQn=qn; while(nQk>1&&Qk[nQk-1]==0)--nQk; while(nQn>1&&Qn[nQn-1]==0)--nQn;
        bool match = (nQk==nQn) && (std::memcmp(Qk,Qn,(size_t)nQk*8)==0);

        wru32(0);           // status 0 = newton record
        wru32(gN);          // n
        wru32(gNAAN);       // na_an
        wru32(gQN);         // qn
        wru32(match?1u:0u); // match flag
        wru64arr(gDN, gN);
        wru64arr(gAN, gNAAN);
        wru64arr(gV, gN);
        wru64arr(v2, gN);
        wru64arr(gL, gNAAN+gN);
        wru64arr(gL2, gNAAN+gN);
        wru64arr(gQe, gQN);
        wru64arr(gRt, gNAAN);
        wru64arr(Qn, gQN);
        wru64arr(Qk, gQN);
        wru64arr(Rn, nb);
        wru64arr(Rk, nb);
        wru64arr(gA0, na);  // pristine A (post-trim, pre-knuthD)
        wru64arr(gB0, nb);  // pristine B (post-trim, pre-knuthD)
        wru64arr(gD_in, n); // actual divisor d passed to newton_divide
        wru64arr(gA_in, na);// actual dividend a passed to newton_divide
        wru32(gSig);        // sigma used inside newton_divide
    }
    fclose(gf);
    return 0;
}
'''

out = head + diag
open(r'D:\precious_speed\tools\ndiag10.cpp', 'w').write(out)
print("written", len(out), "bytes")
