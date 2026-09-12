// gen_shape.cpp — 生成 faithful-shape 的 add 测试输入
// 用法: gen_shape <base 16|10> <T> <char_budget> <seed> > out.in
// 第一行 = 实际组数 T'，随后 T' 行 "A B"
// HEX: 每位 '0'-'9','A'-'F'；DEC: 每位 '0'-'9'。首位非 0(除单字符"0")，~50% 带负号。
#include <cstdio>
#include <random>
#include <vector>
#include <string>

static char hexd(int v){ return v < 10 ? char('0'+v) : char('A'+v-10); }

int main(int argc, char** argv){
    int base   = argc>1 ? atoi(argv[1]) : 16;
    long long T       = argc>2 ? atoll(argv[2]) : 160000;
    long long budget = argc>3 ? atoll(argv[3]) : 3200002LL;
    unsigned long long seed = argc>4 ? strtoull(argv[4],nullptr,10) : 12345ULL;
    int fixed = argc>5 ? atoi(argv[5]) : 0;  // >0 : 每数固定位数

    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> dlen(1,18);
    std::uniform_int_distribution<int> neg(0,1);
    std::uniform_int_distribution<int> dg(0, base==16 ? 15 : 9);

    std::vector<std::string> A,B;
    long long sm=0;
    for(long long t=0;t<T;++t){
        int na = fixed>0 ? fixed : dlen(rng);
        int nb = fixed>0 ? fixed : dlen(rng);
        std::string a,b;
        for(int i=0;i<na;++i) a += (base==16?hexd(dg(rng)):char('0'+dg(rng)));
        for(int i=0;i<nb;++i) b += (base==16?hexd(dg(rng)):char('0'+dg(rng)));
        if(na>=2) a[0] = (base==16?hexd(1+dg(rng)%15):char('1'+dg(rng)%9));
        if(nb>=2) b[0] = (base==16?hexd(1+dg(rng)%15):char('1'+dg(rng)%9));
        if(a=="0") a="0"; if(b=="0") b="0";
        if(neg(rng) && a!="0") a = "-"+a;
        if(neg(rng) && b!="0") b = "-"+b;
        long long add = (long long)a.size() + (long long)b.size() + 3; // + " \n"
        if(sm + add > budget) break;
        sm += add;
        A.push_back(a); B.push_back(b);
    }
    printf("%d\n", (int)A.size());
    for(size_t i=0;i<A.size();++i) printf("%s %s\n", A[i].c_str(), B[i].c_str());
    return 0;
}
