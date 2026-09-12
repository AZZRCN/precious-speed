#pragma GCC optimize("O3,unroll-loops,tree-vectorize")
#pragma GCC target("avx2,fma,bmi,bmi2")
#include <cstdio>
int main() {
    #ifdef __AVX2__
        printf("AVX2 defined\n");
    #else
        printf("AVX2 NOT defined\n");
    #endif
    #ifdef __FMA__
        printf("FMA defined\n");
    #else
        printf("FMA NOT defined\n");
    #endif
    return 0;
}
