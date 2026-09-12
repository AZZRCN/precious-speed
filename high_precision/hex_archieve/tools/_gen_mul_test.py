import os

ROOT = 'D:/hex_precious_speed'
lines = open(os.path.join(ROOT, 'work/div/v9.cpp')).read().splitlines()
# cut at the `int main()` of div.cpp (keep everything before it)
cut = None
for i, l in enumerate(lines):
    if l.startswith('int main('):
        cut = i
        break
assert cut is not None, 'no main found'
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
    '    int fails=0;\n'
    '    for (int trial=0; trial<300; ++trial){\n'
    '        int na = 1 + (trial % 7), nb = 1 + ((trial*3) % 7);\n'
    '        std::vector<u64> a(na), b(nb), c(na+nb), cref(na+nb);\n'
    '        for(int i=0;i<na;++i) a[i]=(u64)0x9e3779b97f4a7c15*(u64)(trial*7+i+1) ^ ((u64)0x123456789abcdef0 >> (i%7));\n'
    '        for(int i=0;i<nb;++i) b[i]=(u64)0x85ebca77c2b2ae63*(u64)(trial*5+i+2) ^ ((u64)0xfedcba9876543210 >> (i%11));\n'
    '        mul_fft(a.data(), na, b.data(), nb, c.data());\n'
    '        school(a.data(), na, b.data(), nb, cref.data());\n'
    '        int bad=0;\n'
    '        for(int i=0;i<na+nb;++i) if (c[i]!=cref[i]) { bad=1; break; }\n'
    '        if (bad){ fails++; if (fails<=5) printf("FAIL trial=%d na=%d nb=%d\\n", trial, na, nb); }\n'
    '    }\n'
    '    printf("FAILS=%d / 300\\n", fails);\n'
    '    return fails?1:0;\n'
    '}\n'
)
out = os.path.join(ROOT, 'tools/mul_test.cpp')
open(out, 'w').write(kept + '\n' + test_main)
print('written', out, 'cut at line', cut + 1)
