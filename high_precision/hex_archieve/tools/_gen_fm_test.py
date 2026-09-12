import os

ROOT = 'D:/hex_precious_speed'
lines = open(os.path.join(ROOT, 'work/div/v9.cpp')).read().splitlines()
cut = None
for i, l in enumerate(lines):
    if l.startswith('int main('):
        cut = i
        break
kept = '\n'.join(lines[:cut])

test_main = (
    '#include <cstdio>\n'
    '#include <cstdint>\n'
    '#include <vector>\n'
    '#include <cstring>\n'
    'using u64 = uint64_t; using u128 = unsigned __int128;\n'
    'static void school(const u64* a, int na, const u64* b, int nb, u64* c) {\n'
    '    std::memset(c,0,(size_t)(na+nb)*8);\n'
    '    for(int i=0;i<nb;++i){ u64 carry=0; for(int j=0;j<na;++j){ u128 t=(u128)a[j]*b[i]+c[i+j]+carry; c[i+j]=(u64)t; carry=(u64)(t>>64);} c[i+na]=carry; }\n'
    '}\n'
    'int main(){\n'
    '    int fails=0, tested=0;\n'
    '    for (int trial=0; trial<250; ++trial){\n'
    '        int n = 60 + (trial % 50);\n'
    '        std::vector<u64> a(n), b(n), c(2*n), cref(2*n);\n'
    '        for(int i=0;i<n;++i){ a[i]=(u64)0x9e3779b97f4a7c15*(u64)(trial*7+i+1) ^ ((u64)0x123456789abcdef0 >> (i%7)); b[i]=(u64)0x85ebca77c2b2ae63*(u64)(trial*5+i+2) ^ ((u64)0xfedcba9876543210 >> (i%11)); }\n'
    '        FixedFFT FF; fm_prep(FF, FMG1, b.data(), n, n);\n'
    '        if (!FF.ok) { printf("skip trial=%d n=%d (ok=false)\\n", trial, n); continue; }\n'
    '        fm_mul(FF, FMG1, a.data(), n, c.data());\n'
    '        school(a.data(), n, b.data(), n, cref.data());\n'
    '        int bad=0; for(int i=0;i<2*n;++i) if (c[i]!=cref[i]) { bad=1; break; }\n'
    '        ++tested;\n'
    '        if (bad){ fails++; if (fails<=5) printf("FAIL trial=%d n=%d\\n", trial, n); }\n'
    '    }\n'
    '    printf("TESTED=%d FAILS=%d\\n", tested, fails);\n'
    '    return fails?1:0;\n'
    '}\n'
)
out = os.path.join(ROOT, 'tools/fm_test.cpp')
open(out, 'w').write(kept + '\n' + test_main)
print('written', out)
