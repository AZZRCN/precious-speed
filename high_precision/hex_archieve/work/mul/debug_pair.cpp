// Focused: run the exact NTT pipeline on the FIRST pair of a .in file and
// compare the result (converted back to base-2^64) against schoolbook base-2^64.
#include <bits/stdc++.h>
using namespace std;
using u64 = uint64_t; using u128 = unsigned __int128;

static const u64 P0=998244353ULL,P1=1004535809ULL,P2=167772161ULL,G=3ULL;
static const int L=27; static const u64 MASKL=(1ULL<<L)-1;
static const u128 ONE64=(u128)1<<64; static const u64 MASK64=0xFFFFFFFFFFFFFFFFULL;
static inline u64 mulmod(u64 a,u64 b,u64 p){return (u64)((u128)a*b%p);}
static inline u64 addmod(u64 a,u64 b,u64 p){u64 r=a+b;return r>=p?r-p:r;}
static inline u64 submod(u64 a,u64 b,u64 p){return a>=b?a-b:a+p-b;}
static u64 powmod(u64 a,u64 e,u64 p){u64 r=1;a%=p;while(e){if(e&1)r=mulmod(r,a,p);a=mulmod(a,a,p);e>>=1;}return r;}
static void ntt(vector<u64>& a,u64 p,bool invert){
    int n=(int)a.size();
    for(int i=1,j=0;i<n;i++){int bit=n>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j)swap(a[i],a[j]);}
    for(int len=2;len<=n;len<<=1){u64 wlen=powmod(G,(p-1)/(u64)len,p);if(invert)wlen=powmod(wlen,p-2,p);
        for(int i=0;i<n;i+=len){u64 w=1;for(int k=0;k<len/2;k++){u64 u=a[i+k],v=mulmod(a[i+k+len/2],w,p);a[i+k]=addmod(u,v,p);a[i+k+len/2]=submod(u,v,p);w=mulmod(w,wlen,p);}}}
    if(invert){u64 invn=powmod((u64)n,p-2,p);for(int i=0;i<n;i++)a[i]=mulmod(a[i],invn,p);}
}
static vector<u64> to_L(const vector<u64>& a){vector<u64> out;u128 acc=0;for(int i=(int)a.size()-1;i>=0;i--){acc=(acc<<64)|a[i];while(acc>MASKL){out.push_back((u64)(acc&MASKL));acc>>=L;}}while(acc>0){out.push_back((u64)(acc&MASKL));acc>>=L;}return out;}
static vector<u128> from_L_to_u128(const vector<u64>& out){vector<u128> r(out.size());u128 acc=0;for(size_t i=0;i<out.size();i++){acc|=out[i];r[i]=acc;acc=0;}return r;} // not used
static vector<u64> from_L(const vector<u128>& c){vector<u64> out;u128 acc=0;for(int i=(int)c.size()-1;i>=0;i--){acc=(acc<<L)|c[i];while(acc>=ONE64){out.push_back((u64)(acc&MASK64));acc>>=64;}}while(acc>0){out.push_back((u64)(acc&MASK64));acc>>=64;}return out;}
static vector<u64> hex_to_limbs64(const string& s){vector<u64> r;for(size_t i=s.size();i>0;){size_t a=(i>=16)?i-16:0;string chunk=s.substr(a,i-a);r.push_back(strtoull(chunk.c_str(),nullptr,16));if(i<=16)break;i-=16;}return r;}
static string limbs64_to_hex(const vector<u64>& out){if(out.empty())return "0";string s;char buf[17];for(int i=(int)out.size()-1;i>=0;i--){if(i==(int)out.size()-1)snprintf(buf,sizeof buf,"%llX",(unsigned long long)out[i]);else snprintf(buf,sizeof buf,"%016llX",(unsigned long long)out[i]);s+=buf;}size_t p=s.find_first_not_of('0');if(p==string::npos)return "0";return s.substr(p);}

// NTT pipeline returning base-2^64 limbs (low->high)
static vector<u64> ntt_pipeline(const string& A,const string& B){
    vector<u64> a64=hex_to_limbs64(A),b64=hex_to_limbs64(B);
    vector<u64> ca=to_L(a64),cb=to_L(b64);
    size_t na=ca.size(),nb=cb.size();
    int N=1;while((size_t)N<na+nb-1)N<<=1;
    ca.resize(N,0);cb.resize(N,0);
    vector<u64> r0(N,0),r1(N,0),r2(N,0);
    vector<u64> ca0=ca,ca1=ca,ca2=ca,cb0=cb,cb1=cb,cb2=cb;
    ntt(ca0,P0,false);ntt(cb0,P0,false);ntt(ca1,P1,false);ntt(cb1,P1,false);ntt(ca2,P2,false);ntt(cb2,P2,false);
    for(int i=0;i<N;i++){r0[i]=mulmod(ca0[i],cb0[i],P0);r1[i]=mulmod(ca1[i],cb1[i],P1);r2[i]=mulmod(ca2[i],cb2[i],P2);}
    ntt(r0,P0,true);ntt(r1,P1,true);ntt(r2,P2,true);
    u64 inv01=powmod(P0,P1-2,P1),inv012=powmod(mulmod(P0,P1,P2),P2-2,P2);
    size_t nc=na+nb-1; vector<u128> c(nc,0);
    for(size_t i=0;i<nc;i++){u64 rr0=r0[i],rr1=r1[i],rr2=r2[i];u64 t1=mulmod(submod(rr1,rr0,P1),inv01,P1);u64 v=addmod(rr0%P2,mulmod(P0,t1,P2),P2);u64 t2=mulmod(submod(rr2,v,P2),inv012,P2);c[i]=(u128)rr0+(u128)P0*t1+(u128)P0*P1*t2;}
    u128 carry=0; for(size_t i=0;i<nc;i++){u128 val=c[i]+carry;c[i]=val&MASKL;carry=val>>L;} while(carry){c.push_back(carry&MASKL);carry>>=L;}
    return from_L(c);
}
// schoolbook base-2^64
static vector<u64> schoolbook(const vector<u64>& a,const vector<u64>& b){
    vector<u128> sb(a.size()+b.size()+1,0);
    for(size_t i=0;i<a.size();i++)for(size_t j=0;j<b.size();j++)sb[i+j]+=(u128)a[i]*b[j];
    vector<u64> out;u128 carry=0;for(auto x:sb){u128 v=x+carry;out.push_back((u64)(v&MASK64));carry=v>>64;}while(carry){out.push_back((u64)(carry&MASK64));carry>>=64;}
    while(out.size()>1&&out.back()==0)out.pop_back();
    return out;
}
int main(int argc,char** argv){
    if(argc<2){fprintf(stderr,"need .in path\n");return 1;}
    ifstream fin(argv[1]); string all((istreambuf_iterator<char>(fin)),istreambuf_iterator<char>());
    istringstream iss(all); long long T; iss>>T;
    string A,B; iss>>A>>B;
    fprintf(stderr,"pair0: A.len=%zu B.len=%zu\n",A.size(),B.size());
    vector<u64> a64=hex_to_limbs64(A),b64=hex_to_limbs64(B);
    vector<u64> ntt_out=ntt_pipeline(A,B);
    vector<u64> sb=schoolbook(a64,b64);
    // compare (both low->high)
    bool ok=(ntt_out.size()==sb.size());
    for(size_t i=0;ok&&i<min(ntt_out.size(),sb.size());i++) if(ntt_out[i]!=sb[i]){ok=false;fprintf(stderr,"  diff@%zu ntt=%llX sb=%llX\n",(unsigned long long)i,(unsigned long long)ntt_out[i],(unsigned long long)sb[i]);}
    if(!ok&&ntt_out.size()!=sb.size()) fprintf(stderr,"  size ntt=%zu sb=%zu\n",ntt_out.size(),sb.size());
    fprintf(stderr,"NTT vs schoolbook: %s\n",ok?"PASS":"FAIL");
    if(!ok){ size_t lim=min(ntt_out.size(),sb.size()); for(size_t i=0;i<lim;i+=max((size_t)1,lim/5)) fprintf(stderr,"  i=%zu ntt=%llX sb=%llX\n",(unsigned long long)i,(unsigned long long)ntt_out[i],(unsigned long long)sb[i]); }
    // full hex compare
    string hn=limbs64_to_hex(ntt_out), hs=limbs64_to_hex(sb);
    fprintf(stderr,"hex match=%s\n",hn==hs?"PASS":"FAIL");
    fprintf(stderr,"ntt_hex=%s\n",hn.c_str());
    fprintf(stderr,"sb_hex =%s\n",hs.c_str());
    return 0;
}
