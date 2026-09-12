#include <cstdio>
#include <cstdlib>
#include <cmath>
int main() {
    uint64_t s = 12345;
    auto rnd = [&](){ s ^= s<<13; s ^= s>>7; s ^= s<<17; return s; };
    const u32 sizes[] = {64,128,256,512,1024,2048,4096,8192};
    for (u32 ts : sizes) {
        double* buf = (double*)std::aligned_alloc(16, (size_t)ts*2*sizeof(double));
        double* ref = (double*)std::aligned_alloc(16, (size_t)ts*2*sizeof(double));
        for (u32 i=0;i<ts*2;i++){ double v=(double)(rnd()&0xffff); buf[i]=v; ref[i]=v; }
        fft::resize(ts);
        fft::difRec((fft::cpx*)buf, ts, 0);
        fft::ditRec((fft::cpx*)buf, ts, 0);
        double maxerr=0;
        for (u32 i=0;i<ts*2;i++){ double e=std::fabs(buf[i]-ref[i]*(double)ts); if(e>maxerr)maxerr=e; }
        printf("ts=%-5u roundtrip_maxerr=%.3e %s\n", ts, maxerr, maxerr<1.0?"OK":"FAIL");
        std::free(buf); std::free(ref);
    }
    return 0;
}
