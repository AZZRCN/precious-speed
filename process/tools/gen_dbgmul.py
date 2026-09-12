src = open('../hex_best/div.cpp', encoding='utf-8').read().splitlines()
main_idx = next(i for i,l in enumerate(src) if l.strip().startswith('int main'))
head = src[:main_idx]
main = r'''
#include <cstdio>
#include <cstring>
#include <string>
#include <iostream>
static int hex2limbs(const char* s, int nhex, u64* out){
    int nc = (nhex+15)>>4;
    auto nib = [](char c)->int{ if(c>='0'&&c<='9')return c-'0'; if(c>='a'&&c<='f')return c-'a'+10; if(c>='A'&&c<='F')return c-'A'+10; return 0; };
    for(int c=0;c<nc;++c){ int lo=nhex-(c+1)*16; int start=lo<0?0:lo; int len=nhex-16*c-start; u64 v=0; for(int k=0;k<len;++k) v=(v<<4)|nib(s[start+k]); out[c]=v; }
    int cnt=nc; while(cnt>1&&out[cnt-1]==0)--cnt; return cnt;
}
static void limbs2hex(const u64* V, int n, char* out){
    while(n>1&&V[n-1]==0)--n;
    for(int i=n-1;i>=0;--i){ for(int b=60;b>=0;b-=4){ int d=(V[i]>>b)&0xF; *out++=d<10?('0'+d):('A'+d-10); } }
    *out=0;
}
int main(){
    std::string A, B, mode; std::getline(std::cin, A); std::getline(std::cin, B); std::getline(std::cin, mode);
    int na=hex2limbs(A.c_str(),(int)A.size(),WORK);
    int nb=hex2limbs(B.c_str(),(int)B.size(),WORK+200000);
    u64* a=WORK, *b=WORK+200000, *c=WORK+400000;
    if(mode=="bf") mul_bf(a,na,b,nb,c); else if(mode=="fft") mul_fft(a,na,b,nb,c); else mulg(a,na,b,nb,c);
    char buf[4000000]; limbs2hex(c,na+nb,buf); printf("%s\n",buf); return 0;
}
'''
open('dbgmul.cpp','w',encoding='utf-8').write('\n'.join(head)+main)
print("wrote dbgmul.cpp")
