import os

ROOT = 'D:/hex_precious_speed'
src = open(os.path.join(ROOT, 'work/div/v9.cpp')).read().splitlines()
start = end = None
for i, l in enumerate(src):
    if l.strip() == 'namespace fft {':
        start = i
    elif l.strip().startswith('}  // namespace fft'):
        end = i
        break
assert start is not None and end is not None, (start, end)
ns = '\n'.join(src[start:end + 1])

harness = (
    '#include <cstdio>\n'
    '#include <cstdint>\n'
    '#include <cmath>\n'
    '#include <complex>\n'
    '#include <cstring>\n'
    '#include <vector>\n'
    '#include <immintrin.h>\n'
    'using u32 = uint32_t; using u64 = uint64_t; using u128 = unsigned __int128;\n'
    '#define FFT_LEAF_LOG 11\n'
    + ns + '\n'
    'int main() {\n'
    '    double worst = 0;\n'
    '    for (u32 ts : {4u,8u,16u,32u,64u,128u,256u,1024u,4096u,16384u}) {\n'
    '        int T = (int)ts;\n'
    '        std::vector<double> buf(ts*2), orig(ts*2);\n'
    '        for (int i=0;i<T;++i){ double v=(double)((i*7+3)%T) - T/2.0; buf[2*i]=v; buf[2*i+1]=0; orig[2*i]=v; }\n'
    '        fft::resize(ts);\n'
    '        fft::difRec((fft::cpx*)buf.data(), ts, 0);\n'
    '        fft::ditRec((fft::cpx*)buf.data(), ts, 0);\n'
    '        double err=0;\n'
    '        for (int i=0;i<T;++i){ double got=buf[2*i]; double exp=orig[2*i]*(double)ts; err += fabs(got-exp); }\n'
    '        printf("ts=%-6d roundtrip_err=%.3g\\n", T, err);\n'
    '        if (err>worst) worst=err;\n'
    '    }\n'
    '    printf("WORST=%.3g  %s\\n", worst, worst<1e-6?"OK":"FAIL");\n'
    '    return worst<1e-6?0:1;\n'
    '}\n'
)
out = os.path.join(ROOT, 'tools/fft_test.cpp')
open(out, 'w').write(harness)
print('written', out, 'ns', start + 1, '..', end + 1)
