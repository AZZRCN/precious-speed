import io

src = open(r'D:\precious_speed\hex_best\div.cpp', 'r').read()
idx = src.index('int main() {')
head = src[:idx]

# harness: each input line is "na nb a_hex b_hex" (already normalized not required)
# compute mulg(a, na, b, nb) and print product hex; python compares.
diag = r'''
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

    static char outb[1<<20];
    for (u32 t = 0; t < T; ++t) {
        while (*p <= ' ') ++p;
        const char* a0 = p; int la = tok_len(p); p += la;
        while (*p <= ' ') ++p;
        const char* b0 = p; int lb = tok_len(p); p += lb;

        int na = parse_limbs(a0, la, A);
        int nb = parse_limbs(b0, lb, B);
        while (na>1 && A[na-1]==0) --na;
        while (nb>1 && B[nb-1]==0) --nb;
        static u64 C[2*MAXC+16];
        std::memset(C, 0, (size_t)(na+nb+2)*8);
        mulg(A, na, B, nb, C);
        int lp = snprintf(outb, sizeof outb, "%u %d %d ", t, na, nb); write(1, outb, lp);
        char* e = put_big(outb, C, na+nb); write(1, outb, (int)(e-outb)); write(1, "\n", 1);
    }
    return 0;
}
'''

out = head.replace('#include <cmath>', '#include <cmath>\n#include <cstdio>') + diag
open(r'D:\precious_speed\tools\ndiag7.cpp', 'w').write(out)
print("written", len(out))
