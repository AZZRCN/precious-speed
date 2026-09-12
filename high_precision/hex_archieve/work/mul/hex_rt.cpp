#include <bits/stdc++.h>
using namespace std;
using u64 = uint64_t; using u128 = unsigned __int128;
static const int L = 27; static const u64 MASKL = (1ULL<<L)-1;
static const u128 ONE64 = (u128)1<<64; static const u64 MASK64 = 0xFFFFFFFFFFFFFFFFULL;
static vector<u64> hex_to_limbs64(const string& s){
    vector<u64> r;
    for (size_t i = s.size(); i > 0; ){
        size_t a = (i >= 16) ? i - 16 : 0;
        string chunk = s.substr(a, i - a);
        r.push_back(strtoull(chunk.c_str(), nullptr, 16));
        if (i <= 16) break; i -= 16;
    }
    return r;
}
static string limbs64_to_hex(const vector<u64>& out){
    if (out.empty()) return "0";
    string s; char buf[17];
    for (int i=(int)out.size()-1;i>=0;i--){
        if (i==(int)out.size()-1) snprintf(buf,sizeof buf,"%llX",(unsigned long long)out[i]);
        else snprintf(buf,sizeof buf,"%016llX",(unsigned long long)out[i]);
        s+=buf;
    }
    size_t p=s.find_first_not_of('0'); if(p==string::npos) return "0"; return s.substr(p);
}
int main(int argc, char** argv){
    ifstream in(argv[1]); string A,B; in>>A>>B;
    string reB = limbs64_to_hex(hex_to_limbs64(B));
    // compare reB to B normalized
    string nB = B; for(auto&c:nB) c=(char)toupper(c); size_t p=nB.find_first_not_of('0'); nB = p==string::npos?string("0"):nB.substr(p);
    fprintf(stderr,"len reB=%zu nB=%zu\n", reB.size(), nB.size());
    size_t n=min(reB.size(),nB.size()); size_t d=string::npos;
    for(size_t i=0;i<n;i++) if(reB[i]!=nB[i]){d=i;break;}
    if(d==string::npos && reB.size()!=nB.size()) d=min(reB.size(),nB.size());
    fprintf(stderr,"diff@%zu\n", d);
    if(d!=string::npos){ long lo=(long)d-30; if(lo<0)lo=0;
        fprintf(stderr,"reB: ...%s\n", reB.substr(lo,50).c_str());
        fprintf(stderr,"nB : ...%s\n", nB.substr(lo,50).c_str());
    }
    return 0;
}
