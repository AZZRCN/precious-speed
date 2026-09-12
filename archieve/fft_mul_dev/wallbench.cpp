// wallbench.cpp — 纯墙钟多轮测速器 (原生跑分, 每点 N 轮, 报 min/median/mean)
// 与 bench8 同思路: 线程池 + 全局队列 + 条件变量即时分配; worker 绑独享核 (pthread_setaffinity_np)
// 只测原生墙钟, 不夹 callgrind -> 干净时序. 输出重定向 /dev/null 避免 I/O 拷贝干扰计时.
#include <bits/stdc++.h>
#include <pthread.h>
#include <sys/stat.h>
#include <dirent.h>
using namespace std;

static string BIN="", CASES_DIR="";
static int    CORES=14, REPS=5;
static long long MINBYTES=0, MAXBYTES=(long long)1e18;

static mutex g_q, g_out;
static condition_variable g_cv;
static queue<pair<string,string>> g_tasks; // (path, name)
static atomic<bool> g_done{false};

// 每点一轮的墙钟(ms) -> 结果
struct R { string name; vector<long long> rounds; };
static map<string,R> g_res;
static mutex g_res_m;
static atomic<uint64_t> g_busy[64]{};
static atomic<int> g_done_cnt{0};

static string bname(const string& p){ size_t s=p.find_last_of('/'); return s==string::npos?p:p.substr(s+1); }
static long long fsize(const string& p){ struct stat st; if(stat(p.c_str(),&st)!=0) return 0; return st.st_size; }
static vector<string> list_in(const string& dir){
    vector<string> r; DIR* d=opendir(dir.c_str()); if(!d) return r;
    struct dirent* e; while((e=readdir(d))){ string n=e->d_name;
        if(n.size()>=3 && n.compare(n.size()-3,3,".in")==0) r.push_back(dir+"/"+n); }
    closedir(d); sort(r.begin(),r.end()); return r;
}
// 原生墙钟单轮 (ms)
static long long run_wall(const string& bin,const string& in){
    auto t0=chrono::steady_clock::now();
    FILE* f=popen((bin+" < "+in+" > /dev/null 2>&1").c_str(),"r");
    if(f) pclose(f);
    auto t1=chrono::steady_clock::now();
    return chrono::duration_cast<chrono::milliseconds>(t1-t0).count();
}
static double median(const vector<long long>& v){
    if(v.empty()) return 0; vector<long long> s=v; sort(s.begin(),s.end());
    size_t n=s.size();
    return n%2 ? s[n/2] : 0.5*(s[n/2-1]+s[n/2]);
}
static void worker(int id){
    cpu_set_t cs; CPU_ZERO(&cs); CPU_SET(id%CORES,&cs);
    pthread_setaffinity_np(pthread_self(),sizeof(cs),&cs);
    uint64_t t0=0; bool busy=false;
    auto nowns=[]()->uint64_t{ return chrono::duration_cast<chrono::nanoseconds>(
        chrono::steady_clock::now().time_since_epoch()).count(); };
    while(true){
        pair<string,string> task;
        { unique_lock<mutex> lk(g_q);
          g_cv.wait(lk,[&]{ return !g_tasks.empty() || g_done.load(); });
          if(g_tasks.empty() && g_done.load()) break;
          task=g_tasks.front(); g_tasks.pop(); }
        if(!busy){ t0=nowns(); busy=true; }
        vector<long long> rounds; rounds.reserve(REPS);
        for(int r=0;r<REPS;r++) rounds.push_back(run_wall(BIN, task.first));
        long long mn = *min_element(rounds.begin(),rounds.end());
        double md = median(rounds);
        double mean = accumulate(rounds.begin(),rounds.end(),0LL)/(double)rounds.size();
        { lock_guard<mutex> lk(g_res_m); g_res[task.second].rounds=move(rounds); }
        g_done_cnt++;
        uint64_t t1=nowns(); if(id<64) g_busy[id].fetch_add(t1-t0); busy=false;
        { lock_guard<mutex> lk(g_out);
          cout<<"[w"<<id<<"] "<<task.second<<" rounds="<<REPS
              <<" min="<<mn
              <<" med="<<fixed<<setprecision(1)<<md
              <<" mean="<<setprecision(1)<<mean
              <<" (left="<<g_tasks.size()<<")"<<"\n"<<flush; }
    }
}
int main(int argc,char** argv){
    string mode="";
    for(int i=1;i<argc;i++){
        string a=argv[i];
        if(a=="--bin"&&i+1<argc) BIN=argv[++i];
        else if(a=="--cases-dir"&&i+1<argc) CASES_DIR=argv[++i];
        else if(a=="--reps"&&i+1<argc) REPS=max(1,atoi(argv[++i]));
        else if(a=="--workers"&&i+1<argc) CORES=atoi(argv[++i]);
        else if(a=="--minbytes"&&i+1<argc) MINBYTES=atoll(argv[++i]);
        else if(a=="--maxbytes"&&i+1<argc) MAXBYTES=atoll(argv[++i]);
    }
    int workers=max(1,min(CORES,14));
    vector<string> cases=list_in(CASES_DIR);
    vector<string> f;
    for(auto&c:cases){ long long sz=fsize(c); if(sz>=MINBYTES&&sz<=MAXBYTES) f.push_back(c); }
    cases=f;
    { lock_guard<mutex> lk(g_q); for(auto&c:cases) g_tasks.push({c,bname(c)}); }
    cout<<"WALLBENCH bin="<<BIN<<" cases="<<cases.size()<<" reps="<<REPS
        <<" workers="<<workers<<" minB="<<MINBYTES<<" maxB="<<MAXBYTES<<"\n"<<flush;
    auto w0=chrono::steady_clock::now();
    vector<thread> thr; for(int i=0;i<workers;i++) thr.emplace_back(worker,i);
    { lock_guard<mutex> lk(g_q); g_done=true; } g_cv.notify_all();
    for(auto&t:thr) t.join();
    auto w1=chrono::steady_clock::now();
    uint64_t wns=chrono::duration_cast<chrono::nanoseconds>(w1-w0).count();
    uint64_t bt=0; for(int i=0;i<workers;i++) bt+=g_busy[i];
    double util=100.0*bt/(workers*wns);

    vector<pair<string,R>> rows(g_res.begin(),g_res.end());
    sort(rows.begin(),rows.end(),[](const auto&a,const auto&b){return a.first<b.first;});
    cout<<"\n════ 墙钟多轮汇总 (ms; min/med/mean) ════\n";
    cout<<setw(28)<<left<<"CASE"<<right<<setw(10)<<"min"<<setw(10)<<"med"<<setw(10)<<"mean"<<"\n";
    for(auto&kv:rows){
        const auto& v=kv.second.rounds;
        long long mn=*min_element(v.begin(),v.end());
        double md=median(v), mean=(accumulate(v.begin(),v.end(),0LL)/(double)v.size());
        cout<<setw(28)<<left<<kv.first<<right<<setw(10)<<mn<<setw(10)<<fixed<<setprecision(1)<<md<<setw(10)<<setprecision(1)<<mean<<"\n";
    }
    cout<<"UTIL workers="<<workers<<" wall_s="<<fixed<<setprecision(1)<<(wns/1e9)
        <<" busy_sum_s="<<(bt/1e9)<<" util="<<util<<"%\n"<<flush;
    return 0;
}
