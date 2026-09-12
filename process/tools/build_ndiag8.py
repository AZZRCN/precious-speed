src = open(r'D:\precious_speed\hex_best\div.cpp', 'r').read()
idx = src.index('int main() {')
head = src[:idx]

# inject a dump helper right after the includes / before first static (use the hugify area)
helper = r'''
static void dump(const char* tag, const u64* x, int n) {
    static char buf[1<<20];
    char* e = put_big(buf, x, n);
    fprintf(stderr, "%s=", tag);
    fwrite(buf, 1, (size_t)(e-buf), stderr);
    fprintf(stderr, "\n");
}
'''

# rename newton_divide -> newton_divide_dbg and add cstdio + helper
head = head.replace('#include <cmath>', '#include <cmath>\n#include <cstdio>')
head = head.replace('static void newton_divide(', helper + '\nstatic void newton_divide_dbg(')

# inject dumps at key points (unique substrings)
head = head.replace('invertappr(dn, n, v);',
                    'invertappr(dn, n, v);\n    dump("V", v, n);')
head = head.replace('mulg(an, na_an, v, n, L);',
                    'mulg(an, na_an, v, n, L);\n    dump("L", L, na_an + n);')
head = head.replace('qe[qn] = c;',
                    'qe[qn] = c;\n    dump("QEST", qe, qn + 1);')
head = head.replace('        }\n        // 校正',
                    '        }\n    dump("QE_ifb", qe, qn + 1); dump("RT_ifb", Rt, na_an);\n        // 校正')
head = head.replace('    }\n    std::memcpy(q, qe',
                    '    }\n    dump("QE_FINAL", qe, qn + 1); dump("RT_FINAL", Rt, na_an);\n    std::memcpy(q, qe')

diag = r'''
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

    static u64 Qk[MAXC+8], Rk[MAXC+8];
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
        knuthD(A, na, B, nb, Qk, Rk);
        wp = WORK;
        static u64 Qn[MAXC+8], Rn[MAXC+8];
        newton_divide_dbg(A, na, B, nb, Qn, Rn);
        int qn = na - nb + 1;
        bool ok = true;
        int nQk=qn, nQn=qn; while(nQk>1&&Qk[nQk-1]==0)--nQk; while(nQn>1&&Qn[nQn-1]==0)--nQn;
        if (nQk!=nQn || std::memcmp(Qk,Qn,(size_t)nQk*8)!=0) ok=false;
        int nRk=nb,nRn=nb; while(nRk>1&&Rk[nRk-1]==0)--nRk; while(nRn>1&&Rn[nRn-1]==0)--nRn;
        if (nRk!=nRn || std::memcmp(Rk,Rn,(size_t)nRk*8)!=0) ok=false;
        fprintf(stderr, "==== CASE %u na=%d nb=%d qn=%d %s ====\n", t, na, nb, qn, ok?"OK":"FAIL");
        if (!ok) {
            lp = snprintf(line,sizeof line,"CASE %u na=%d nb=%d qn=%d FAIL\n", t, na, nb, qn); write(1,line,lp);
            lp = snprintf(line,sizeof line,"  Qk="); write(1,line,lp);
            char* e=put_big(line,Qk,qn); write(1,line,(int)(e-line)); write(1,"\n",1);
            lp = snprintf(line,sizeof line,"  Qn="); write(1,line,lp);
            e=put_big(line,Qn,qn); write(1,line,(int)(e-line)); write(1,"\n",1);
        }
    }
    return 0;
}
'''

out = head + diag
open(r'D:\precious_speed\tools\ndiag8.cpp', 'w').write(out)
print("written", len(out))
