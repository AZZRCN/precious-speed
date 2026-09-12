import io

src = open(r'D:\precious_speed\hex_best\div.cpp', 'r').read()
idx = src.index('int main() {')
head = src[:idx]

diag = r'''
static u64 VOUT[2*MAXC+16];

int main() {
    hugify(inbuf_, sizeof inbuf_);
    hugify(B, sizeof B);
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
        const char* b0 = p; int lb = tok_len(p); p += lb;
        int nb = parse_limbs(b0, lb, B);
        while (nb>1 && B[nb-1]==0) --nb;
        int n = nb;
        // already normalized by generator (top bit set)
        u64* dn = VOUT;
        std::memcpy(dn, B, (size_t)n*8);
        wp = WORK;
        u64* vv = dn + n + 8;
        invertappr(dn, n, vv);
        lp = snprintf(line, sizeof line, "%u %d ", t, n);
        write(1, line, lp);
        char* e = put_big(line, vv, n);
        write(1, line, (int)(e-line));
        write(1, "\n", 1);
    }
    return 0;
}
'''

out = head.replace('#include <cmath>', '#include <cmath>\n#include <cstdio>') + diag
open(r'D:\precious_speed\tools\ndiag4.cpp', 'w').write(out)
print("written", len(out))
