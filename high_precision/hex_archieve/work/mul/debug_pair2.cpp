// Isolated NTT pipeline debugger for the FIRST pair of a .in file.
// Strips signs. Runs each stage independently to localize the bug:
//   S1: to_L/from_L round-trip on A and B (back to hex, compare to input)
//   S2: per-prime NTT convolution vs naive convolution mod p (ca,cb base-2^27)
//   S3: CRT coefficient reconstruction vs true coefficient (schoolbook u128)
//   S4: full pipeline vs schoolbook base-2^64
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
// normalize hex (strip leading zeros)
static string normhex(const string& s){size_t p=s.find_first_not_of('0');return p==string::npos?"0":s.substr(p);}

static vector<u64> naive_conv(const vector<u64>& a,const vector<u64>& b,u64 p){
    vector<u64> r(a.size()+b.size()-1,0);
    for(size_t i=0;i<a.size();i++)for(size_t j=0;j<b.size();j++) r[i+j]=addmod(r[i+j],mulmod(a[i],b[j],p),p);
    return r;
}
// true coefficient of linear convolution at position k (u128, exact)
static u128 true_coeff(const vector<u64>& a,const vector<u64>& b,size_t k){
    u128 s=0; for(size_t i=0;i<a.size();i++){ size_t j=k-i; if(j<b.size()) s+=(u128)a[i]*b[j]; } return s;
}

int main(int argc,char** argv){
    if(argc<2){fprintf(stderr,"need .in path\n");return 1;}
    ifstream fin(argv[1]); string all((istreambuf_iterator<char>(fin)),istreambuf_iterator<char>());
    istringstream iss(all); long long T; iss>>T;
    string A,B; iss>>A>>B;
    // strip signs
    bool negA=!A.empty()&&A[0]=='-'; if(negA)A=A.substr(1);
    bool negB=!B.empty()&&B[0]=='-'; if(negB)B=B.substr(1);
    fprintf(stderr,"pair0: A.len=%zu B.len=%zu negA=%d negB=%d\n",A.size(),B.size(),negA,negB);

    vector<u64> a64=hex_to_limbs64(A), b64=hex_to_limbs64(B);
    vector<u64> ca=to_L(a64), cb=to_L(b64);
    fprintf(stderr,"na(base2^27)=%zu nb=%zu\n",ca.size(),cb.size());

    // S1: to_L/from_L round-trip on A
    {
        vector<u64> back=from_L(vector<u128>(ca.begin(),ca.end()));
        string h=limbs64_to_hex(back);
        fprintf(stderr,"S1 to_L/from_L A: %s\n", normhex(h)==normhex(A)?"PASS":"FAIL");
    }
    {
        vector<u64> back=from_L(vector<u128>(cb.begin(),cb.end()));
        string h=limbs64_to_hex(back);
        fprintf(stderr,"S1 to_L/from_L B: %s\n", normhex(h)==normhex(B)?"PASS":"FAIL");
    }

    // S2: per-prime NTT conv vs naive conv mod p
    int N=1; while((size_t)N<ca.size()+cb.size()-1)N<<=1;
    fprintf(stderr,"N=%d\n",N);
    {
        vector<u64> xa=ca,xb=cb; xa.resize(N,0);xb.resize(N,0);
        vector<u64> ya=xa,yb=xb; ntt(ya,P0,false);ntt(yb,P0,false);
        for(int i=0;i<N;i++)ya[i]=mulmod(ya[i],yb[i],P0); ntt(ya,P0,true);
        vector<u64> nav=naive_conv(xa,xb,P0);
        bool ok=true; for(int i=0;i<(int)ca.size()+cb.size()-1;i++) if(ya[i]!=nav[i]){ok=false;fprintf(stderr,"  p0 diff@%d ntt=%llu nav=%llu\n",i,(unsigned long long)ya[i],(unsigned long long)nav[i]);break;}
        fprintf(stderr,"S2 NTT conv vs naive mod P0: %s\n",ok?"PASS":"FAIL");
    }
    {
        vector<u64> xa=ca,xb=cb; xa.resize(N,0);xb.resize(N,0);
        vector<u64> ya=xa,yb=xb; ntt(ya,P1,false);ntt(yb,P1,false);
        for(int i=0;i<N;i++)ya[i]=mulmod(ya[i],yb[i],P1); ntt(ya,P1,true);
        vector<u64> nav=naive_conv(xa,xb,P1);
        bool ok=true; for(int i=0;i<(int)ca.size()+cb.size()-1;i++) if(ya[i]!=nav[i]){ok=false;fprintf(stderr,"  p1 diff@%d ntt=%llu nav=%llu\n",i,(unsigned long long)ya[i],(unsigned long long)nav[i]);break;}
        fprintf(stderr,"S2 NTT conv vs naive mod P1: %s\n",ok?"PASS":"FAIL");
    }
    {
        vector<u64> xa=ca,xb=cb; xa.resize(N,0);xb.resize(N,0);
        vector<u64> ya=xa,yb=xb; ntt(ya,P2,false);ntt(yb,P2,false);
        for(int i=0;i<N;i++)ya[i]=mulmod(ya[i],yb[i],P2); ntt(ya,P2,true);
        vector<u64> nav=naive_conv(xa,xb,P2);
        bool ok=true; for(int i=0;i<(int)ca.size()+cb.size()-1;i++) if(ya[i]!=nav[i]){ok=false;fprintf(stderr,"  p2 diff@%d ntt=%llu nav=%llu\n",i,(unsigned long long)ya[i],(unsigned long long)nav[i]);break;}
        fprintf(stderr,"S2 NTT conv vs naive mod P2: %s\n",ok?"PASS":"FAIL");
    }

    // S3: CRT reconstruction vs true coeff at a few positions
    {
        vector<u64> xa=ca,xb=cb; xa.resize(N,0);xb.resize(N,0);
        vector<u64> r0=xa,r1=xa,r2=xa,cb0=xb,cb1=xb,cb2=xb;
        ntt(r0,P0,false);ntt(cb0,P0,false);ntt(r1,P1,false);ntt(cb1,P1,false);ntt(r2,P2,false);ntt(cb2,P2,false);
        for(int i=0;i<N;i++){r0[i]=mulmod(r0[i],cb0[i],P0);r1[i]=mulmod(r1[i],cb1[i],P1);r2[i]=mulmod(r2[i],cb2[i],P2);}
        ntt(r0,P0,true);ntt(r1,P1,true);ntt(r2,P2,true);
        u64 inv01=powmod(P0,P1-2,P1),inv012=powmod(mulmod(P0,P1,P2),P2-2,P2);
        bool ok=true;
        vector<size_t> ks = {0, 1, ca.size()-1, (ca.size()+cb.size())/2};
        for(size_t k: ks){
            u64 rr0=r0[k],rr1=r1[k],rr2=r2[k];
            u64 t1=mulmod(submod(rr1,rr0,P1),inv01,P1);
            u64 v=addmod(rr0%P2,mulmod(P0,t1,P2),P2);
            u64 t2=mulmod(submod(rr2,v,P2),inv012,P2);
            u128 c=(u128)rr0+(u128)P0*t1+(u128)P0*P1*t2;
            u128 tc=true_coeff(ca,cb,k);
            // c should equal tc mod p0p1p2; reduce for compare
            u128 M=(u128)P0*P1*P2; u128 cmod=c%M; u128 tcmod=tc%M;
            if(cmod!=tcmod){ok=false;fprintf(stderr,"  CRT k=%zu crt=%llu%llu tc=%llu%llu (hi/lo)\n",(unsigned long long)k,(unsigned long long)(cmod>>64),(unsigned long long)(cmod&MASK64),(unsigned long long)(tcmod>>64),(unsigned long long)(tcmod&MASK64));}
        }
        fprintf(stderr,"S3 CRT vs true coeff: %s\n",ok?"PASS":"FAIL");
    }

    // S4: full pipeline vs schoolbook base-2^64
    {
        // pipeline result
        vector<u64> xa=ca,xb=cb; xa.resize(N,0);xb.resize(N,0);
        vector<u64> r0=xa,r1=xa,r2=xa,cb0=xb,cb1=xb,cb2=xb;
        ntt(r0,P0,false);ntt(cb0,P0,false);ntt(r1,P1,false);ntt(cb1,P1,false);ntt(r2,P2,false);ntt(cb2,P2,false);
        for(int i=0;i<N;i++){r0[i]=mulmod(r0[i],cb0[i],P0);r1[i]=mulmod(r1[i],cb1[i],P1);r2[i]=mulmod(r2[i],cb2[i],P2);}
        ntt(r0,P0,true);ntt(r1,P1,true);ntt(r2,P2,true);
        u64 inv01=powmod(P0,P1-2,P1),inv012=powmod(mulmod(P0,P1,P2),P2-2,P2);
        size_t nc=ca.size()+cb.size()-1; vector<u128> c(nc,0);
        for(size_t i=0;i<nc;i++){u64 rr0=r0[i],rr1=r1[i],rr2=r2[i];u64 t1=mulmod(submod(rr1,rr0,P1),inv01,P1);u64 v=addmod(rr0%P2,mulmod(P0,t1,P2),P2);u64 t2=mulmod(submod(rr2,v,P2),inv012,P2);c[i]=(u128)rr0+(u128)P0*t1+(u128)P0*P1*t2;}
        // FULL check c[i] vs true coeff
        { bool allok=true; for(size_t i=0;i<nc;i++){ u128 tc=true_coeff(ca,cb,i); if(c[i]!=tc){allok=false; fprintf(stderr,"  CRT-FULL mismatch@%zu crt=%llu%llu tc=%llu%llu\n",(unsigned long long)i,(unsigned long long)(c[i]>>64),(unsigned long long)(c[i]&MASK64),(unsigned long long)(tc>>64),(unsigned long long)(tc&MASK64)); if(i>5)break;} } fprintf(stderr,"S3b CRT ALL positions: %s\n",allok?"PASS":"FAIL"); }
        // DUMP low coefficients
        {
            u128 direct=(u128)ca[0]*cb[0];
            fprintf(stderr,"DUMP ca[0]=%llX cb[0]=%llX ca0*cb0=%llX%llX c[0]=%llX%llX\n",
                (unsigned long long)ca[0],(unsigned long long)cb[0],
                (unsigned long long)(direct>>64),(unsigned long long)(direct&MASK64),
                (unsigned long long)(c[0]>>64),(unsigned long long)(c[0]&MASK64));
            u128 lowc=c[0]; // raw coeff pos 0
            fprintf(stderr,"DUMP (A*B) mod 2^27 true=%llX ntt=%llX\n",(unsigned long long)((((u128)ca[0]*cb[0])&MASKL)),(unsigned long long)(lowc&MASKL));
        }
        u128 carry=0; for(size_t i=0;i<nc;i++){u128 val=c[i]+carry;c[i]=val&MASKL;carry=val>>L;} while(carry){c.push_back(carry&MASKL);carry>>=L;}
        {
            fprintf(stderr,"DUMP carry-norm c[0..4] = %llX %llX %llX %llX %llX\n",
                (unsigned long long)(c.size()>0?c[0]:0),(unsigned long long)(c.size()>1?c[1]:0),
                (unsigned long long)(c.size()>2?c[2]:0),(unsigned long long)(c.size()>3?c[3]:0),(unsigned long long)(c.size()>4?c[4]:0));
            u128 acc=0; for(int i=0;i<3 && i<(int)c.size();i++) acc|=(u128)c[i]<<(27*i);
            fprintf(stderr,"DUMP reconstructed low64 from digits = %016llX (true C743D7096102D92F)\n",(unsigned long long)(acc&MASK64));
        }
        vector<u64> ntt_out=from_L(c);
        // schoolbook base-2^64
        vector<u128> sb(a64.size()+b64.size()+1,0);
        for(size_t i=0;i<a64.size();i++)for(size_t j=0;j<b64.size();j++)sb[i+j]+=(u128)a64[i]*b64[j];
        vector<u64> sbout;u128 sc=0;for(auto x:sb){u128 v=x+sc;sbout.push_back((u64)(v&MASK64));sc=v>>64;}while(sc){sbout.push_back((u64)(sc&MASK64));sc>>=64;}
        while(sbout.size()>1&&sbout.back()==0)sbout.pop_back();
        bool ok=(ntt_out.size()==sbout.size());
        for(size_t i=0;ok&&i<min(ntt_out.size(),sbout.size());i++) if(ntt_out[i]!=sbout[i]){ok=false;fprintf(stderr,"  diff@%zu ntt=%016llX sb=%016llX\n",(unsigned long long)i,(unsigned long long)ntt_out[i],(unsigned long long)sbout[i]);}
        fprintf(stderr,"S4 full pipeline vs schoolbook: %s\n",ok?"PASS":"FAIL");
        { FILE* f=fopen("ntt_hex.txt","w"); fprintf(f,"%s",limbs64_to_hex(ntt_out).c_str()); fclose(f); }
        { FILE* f=fopen("sb_hex.txt","w"); fprintf(f,"%s",limbs64_to_hex(sbout).c_str()); fclose(f); }
        { FILE* f=fopen("a_hex.txt","w"); fprintf(f,"%s",A.c_str()); fclose(f); }
        { FILE* f=fopen("b_hex.txt","w"); fprintf(f,"%s",B.c_str()); fclose(f); }
    }
    return 0;
}
