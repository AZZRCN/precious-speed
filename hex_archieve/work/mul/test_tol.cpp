// Unit test: isolate to_L / from_L / hex_to_limbs64 on the REAL A,B.
// Verify (a) round-trip, (b) ca[0] == int(A,16) mod 2^27 (true low digit).
#include <bits/stdc++.h>
using namespace std; using u64=uint64_t; using u128=unsigned __int128;
static const int L=27; static const u64 MASKL=(1ULL<<L)-1;
static const u128 ONE64=(u128)1<<64; static const u64 MASK64=0xFFFFFFFFFFFFFFFFULL;
static vector<u64> to_L(const vector<u64>& a){vector<u64> out;u128 acc=0;for(int i=(int)a.size()-1;i>=0;i--){acc=(acc<<64)|a[i];while(acc>MASKL){out.push_back((u64)(acc&MASKL));acc>>=L;}}while(acc>0){out.push_back((u64)(acc&MASKL));acc>>=L;}return out;}
static vector<u64> from_L(const vector<u128>& c){vector<u64> out;u128 acc=0;for(int i=(int)c.size()-1;i>=0;i--){acc=(acc<<L)|c[i];while(acc>=ONE64){out.push_back((u64)(acc&MASK64));acc>>=64;}}while(acc>0){out.push_back((u64)(acc&MASK64));acc>>=64;}return out;}
static vector<u64> hex_to_limbs64(const string& s){vector<u64> r;for(size_t i=s.size();i>0;){size_t a=(i>=16)?i-16:0;string chunk=s.substr(a,i-a);r.push_back(strtoull(chunk.c_str(),nullptr,16));if(i<=16)break;i-=16;}return r;}
static string limbs64_to_hex(const vector<u64>& out){if(out.empty())return "0";string s;char buf[17];for(int i=(int)out.size()-1;i>=0;i--){if(i==(int)out.size()-1)snprintf(buf,sizeof buf,"%llX",(unsigned long long)out[i]);else snprintf(buf,sizeof buf,"%016llX",(unsigned long long)out[i]);s+=buf;}size_t p=s.find_first_not_of('0');if(p==string::npos)return "0";return s.substr(p);}
static string normhex(const string& s){size_t p=s.find_first_not_of('0');return p==string::npos?"0":s.substr(p);}

int main(int argc,char** argv){
    string A; {ifstream f("a_hex.txt"); f>>A;}
    string B; {ifstream f("b_hex.txt"); f>>B;}
    fprintf(stderr,"A.len=%zu B.len=%zu\n",A.size(),B.size());

    vector<u64> a64=hex_to_limbs64(A);
    vector<u64> ca=to_L(a64);
    fprintf(stderr,"a64.size=%zu ca.size=%zu\n",a64.size(),ca.size());
    { FILE* f=fopen("ca_dump.txt","w"); for(size_t i=0;i<ca.size();i++) fprintf(f,"%llX\n",(unsigned long long)ca[i]); fclose(f); }

    // (a) round-trip (numeric: compare from_L(ca) limbs vs a64)
    {
        vector<u64> X=from_L(vector<u128>(ca.begin(),ca.end()));
        bool numok = (X.size()==a64.size());
        if(numok) for(size_t i=0;i<X.size();i++) if(X[i]!=a64[i]){numok=false;fprintf(stderr,"  limb mismatch@%zu X=%016llX a64=%016llX\n",(unsigned long long)i,(unsigned long long)X[i],(unsigned long long)a64[i]);}
        fprintf(stderr,"roundtrip-numeric A: %s (X.size=%zu a64.size=%zu)\n",numok?"PASS":"FAIL",X.size(),a64.size());
        fprintf(stderr,"X[0]=%016llX a64[0]=%016llX\n",(unsigned long long)X[0],(unsigned long long)a64[0]);
        fprintf(stderr,"X[0] mod2^27=%llX  (true A mod2^27=3A8059)\n",(unsigned long long)(X[0]&MASKL));
    }
    string rt=limbs64_to_hex(from_L(vector<u128>(ca.begin(),ca.end())));
    fprintf(stderr,"roundtrip-string A: %s\n", normhex(rt)==normhex(A)?"PASS":"FAIL");

    // (b) ca[0] vs true mod 2^27  (compute true via Python-provided value: read from a separate file a_mod.txt if present)
    // compute true low 27 bits of A using a64: A mod 2^27 = a64[0] mod 2^27 (since 2^27 < 2^64)
    u64 true_low27 = a64[0] & MASKL;  // this is A mod 2^27 ONLY IF a64[0] is correct low limb of A
    fprintf(stderr,"ca[0]=%llX  a64[0]&MASKL(true low27 if hex_to_limbs64 ok)=%llX\n",
        (unsigned long long)ca[0],(unsigned long long)true_low27);

    // Also: reconstruct A from a64 directly in base 2^64 and compare to limbs64_to_hex(a64)
    string ah=limbs64_to_hex(a64);
    fprintf(stderr,"hex_to_limbs64->hex A: %s (compare to A)\n", normhex(ah)==normhex(A)?"PASS":"FAIL");

    // dump last 16 chars of A and of a64 reconstruction
    fprintf(stderr,"A  last16=%s\n", A.size()>=16?A.substr(A.size()-16).c_str():"(short)");
    fprintf(stderr,"a64 last  =%s\n", ah.size()>=16?ah.substr(ah.size()-16).c_str():"(short)");

    // B side
    vector<u64> b64=hex_to_limbs64(B);
    vector<u64> cb=to_L(b64);
    u64 true_low27b=b64[0]&MASKL;
    fprintf(stderr,"cb[0]=%llX  b64[0]&MASKL=%llX  hex_to_limbs64->hex B: %s\n",
        (unsigned long long)cb[0],(unsigned long long)true_low27b, normhex(limbs64_to_hex(b64))==normhex(B)?"PASS":"FAIL");
    return 0;
}
