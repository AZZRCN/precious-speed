// bench8.cpp — 多线程 C++ 实测台 (多层级评判: 墙钟 / 指令数 / miss 数 / syscall 数 / bytecmp 闸门)
// 线程池 + 全局任务队列 + 条件变量即时分配; 每 worker 绑独享核 (pthread_setaffinity_np).
//
// ════════════════════════════════════════════════════════════════════════
// 指标说明 (多层级评判体系, 用户 8/21 确立):
//   wall      : 原生墙钟, min-of-N (规避调度抖动). 时序代理, 受 cache/邻居影响.
//   Ir        : callgrind/cachegrind 用户态指令数. **确定性真值**, 在 x86-64 二进制下
//               Intel/AMD 一致 -> 即 AMD EPYC 7B13 实际指令数 (目标评判机).
//   miss      : cachegrind D1/DL/LL 读/写 miss. 解释 wall 与 cycles 差异.
//   syscall   : strace -c 系统调用计数 (total + write). **VM 无 PMU, 内核态指令数不可得**,
//               故用 syscall 计数作代理. OUTfix 回归正是 57 万次 write -> wall 暴涨而 Ir 不变.
//   cycle     : 需真实 PMU; .66 为 VMware 客户机未透传虚拟 PMU -> 本台不取 (墙钟作代理).
// ════════════════════════════════════════════════════════════════════════
//
// 任务类型:
//   0 = CORRECT   : bin 与 base-bin 逐字节比 (正确性闸门)
//   1 = CG        : callgrind 取 bin 默认环境 Ir
//   3 = CG_SWEEP  : 同 bin 注入不同 env (--sweep KEY:V1,V2,..) 各跑 callgrind, 取 Ir
//   4 = MULTILEVEL: 墙钟(min-of-reps) + cachegrind(Ir + D1/DL/LL 读/写 miss) [+ syscall 若 --syscall]
//   5 = SYSCALL   : strace -c 取 syscall 计数 (total + write)
//   6 = FULL      : = MULTILEVEL 但强制含 syscall
//   7 = ML_BASE   : 同 4, 但跑 BASE_BIN, 结果存 g_ml_base (供 rb 回归报告)
//
// 模式:
//   correct | cg | cgsweep | cgfiles | multilevel | syscall | full | rb
//   rb = 对 BIN 与 BASE_BIN 各跑 multilevel, 末段打印"回归/改善报告"(高亮 --target,
//        标出 Ir 退化 > --rb-thresh% 的点, 并断言其他测试点 Ir 未丢红利).
//
// 用法:
//   bench8 --bin bin/ref_cand --base-bin bin/ref_base --mode rb \
//          --cases-dir lcinputs --target length_ratio_integer_03 --rb-thresh 2.0 --workers 14
//   bench8 --bin bin/ref_base --mode syscall --cases-dir lcinputs_ml --workers 14
//   bench8 --bin bin/ref_base --mode multilevel --minbytes 100000000 --syscall \
//          --cases-dir lcinputs --workers 14
//   bench8 --bin bin/ref_base --mode cgsweep --sweep CYCB:48,49,50 --single monster.in
#include <bits/stdc++.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <cstdlib>
#include <fstream>

using namespace std;

static string BIN        = "/tmp/hexcmp/bin/ref_base"; // 被测二进制
static string BASE_BIN   = "";                          // bytecmp / rb 基线 (空=不比)
static string CASES_DIR  = "/tmp/hexcmp/cases";
static string SINGLE     = "";                          // 单文件模式
static string G_ENV_KEY  = "", G_ENV_VAL = "";           // correct 模式给 bin 注入 env
static int    CORES      = 14;
static long long MINBYTES = 0, MAXBYTES = (long long)1e18;
static int    REPS       = 3;        // 墙钟次数 (取 min)
static double RB_THRESH  = 2.0;      // rb 模式 Ir 退化告警阈值 (%)
static string TARGET     = "";       // rb 模式高亮目标点
static bool   DO_SYSCALL = false;    // multilevel 是否附加 syscall 计数
static bool   COLLECT_SYSTIME = false; // callgrind --collect-systime=yes (syscall 时间代理)

static mutex        g_out, g_q, g_cgmap;
static condition_variable g_cv;
static queue<tuple<int,string,string>> g_tasks; // (type, path, name)
static atomic<bool> g_done{false};
static atomic<int>  g_fails{0}, g_total{0};
static atomic<uint64_t> g_busy[64]{};
static bool         g_include_sysc = false; // 运行期: multilevel 是否带 syscall

// 多层级单点结果
struct ML {
    long long wall=0, Ir=0, Dr=0, Dw=0,
              D1mr=0, DLmr=0, LLmr=0, D1mw=0, DLmw=0, LLmw=0,
              sysc_total=0, sysc_write=0;
};
struct SysC { long long total=0, write=0; };

static map<string,ML> g_ml, g_ml_base;             // 多层级 (BIN / BASE_BIN)
static map<string,map<string,long long>> g_cgsweep;
static map<string,long long> g_cg;
static map<string,vector<string>> g_sweep;
static string g_report; // rb 报告累积

static string bname(const string& p){
    size_t s=p.find_last_of('/'); return s==string::npos?p:p.substr(s+1);
}
static long long fsize(const string& p){
    struct stat st; if(stat(p.c_str(),&st)!=0) return 0; return st.st_size;
}
static vector<string> list_in(const string& dir){
    vector<string> r; DIR* d=opendir(dir.c_str()); if(!d) return r;
    struct dirent* e; while((e=readdir(d))){ string n=e->d_name;
        if(n.size()>=3 && n.compare(n.size()-3,3,".in")==0) r.push_back(dir+"/"+n); }
    closedir(d); sort(r.begin(),r.end()); return r;
}
// 跑命令捕获 stdout; 返回退出码
static int capture(const string& cmd, string& out){
    FILE* f=popen(cmd.c_str(),"r"); if(!f) return -1;
    char buf[1<<16]; out.clear();
    while(fgets(buf,sizeof(buf),f)) out+=buf;
    int rc=pclose(f);
    return WIFEXITED(rc)?WEXITSTATUS(rc):-1;
}
static int run_prog(const string& bin,const string& in,string& out){
    return capture(bin+" < "+in, out);
}
// callgrind: 解析 totals: Ir
static long long run_cg(const string& bin,const string& in,const string& tag){
    string outf="/tmp/cg_"+tag+".txt";
    string sy = COLLECT_SYSTIME? " --collect-systime=yes" : "";
    string dummy;
    capture("valgrind --tool=callgrind"+sy+" --callgrind-out-file="+outf+" --quiet "
            +bin+" < "+in+" > /dev/null 2>&1", dummy);
    long long ir=0; ifstream f(outf); if(!f) return 0; string line;
    while(getline(f,line)) if(line.rfind("totals:",0)==0){
        stringstream ss(line); string t; ss>>t; if(ss>>ir) break; }
    return ir;
}
// 原生墙钟 (min of REPS 次, ms). 输出重定向 /dev/null, 避免大输出拷贝干扰计时.
static long long run_wall(const string& bin,const string& in){
    long long best=LLONG_MAX;
    for(int r=0;r<REPS;r++){
        auto t0=chrono::steady_clock::now();
        FILE* f=popen((bin+" < "+in+" > /dev/null 2>&1").c_str(),"r");
        if(f) pclose(f);
        auto t1=chrono::steady_clock::now();
        long long ms=chrono::duration_cast<chrono::milliseconds>(t1-t0).count();
        if(ms<best) best=ms;
    }
    return best;
}
// cachegrind: 解析 events:/totals: 行 -> ML (含 Ir)
static ML run_cgmiss(const string& bin,const string& in,const string& tag){
    string outf="/tmp/cgm_"+tag+".txt";
    string dummy;
    // 用 callgrind (非 cachegrind): 它同样收集 Ir + D1/DL 读写 miss, 且输出 events:/totals: 格式
    // (cachegrind 输出 ==PID== I refs: 样式, 无法用 events:/totals: 解析 -> Ir/miss 全 0).
    // 注: callgrind 的 DLmr 即末级(L2/LL)读 miss, 本台视角记作 LLmr.
    capture("valgrind --tool=callgrind --cache-sim=yes --callgrind-out-file="+outf+" --quiet "
            +bin+" < "+in+" > /dev/null 2>&1", dummy);
    ML m; ifstream f(outf); if(!f) return m;
    string evl="", tot="", line;
    while(getline(f,line)){
        if(line.rfind("events:",0)==0) evl=line;
        if(line.rfind("totals:",0)==0){ tot=line; break; }
    }
    map<string,int> col; { stringstream ss(evl); string t; bool first=true; int idx=0;
        while(ss>>t){ if(first){first=false;continue;} col[t]=idx++; } }
    vector<long long> v; { stringstream ss(tot); string t; bool first=true;
        while(ss>>t){ if(first){first=false;continue;} v.push_back(atoll(t.c_str())); } }
    auto get=[&](const string& n)->long long{ auto it=col.find(n);
        return it==col.end()?0:(v.empty()?0:v[it->second]); };
    m.Ir=get("Ir"); m.Dr=get("Dr"); m.Dw=get("Dw");
    m.D1mr=get("D1mr"); m.DLmr=get("DLmr"); m.LLmr=get("DLmr"); // callgrind DLmr = 末级读 miss -> 记 LLmr
    m.D1mw=get("D1mw"); m.DLmw=get("DLmw"); m.LLmw=get("DLmw");
    return m;
}
// strace -c: 系统调用计数 (total + write). VM 无 PMU, 内核指令数不可得 -> 用计数代理.
static SysC run_syscall(const string& bin,const string& in,const string& tag){
    string outf="/tmp/st_"+tag+".txt";
    string dummy;
    capture("strace -c -o "+outf+" "+bin+" < "+in+" > /dev/null 2>&1", dummy);
    SysC s; ifstream f(outf); if(!f) return s; string line;
    while(getline(f,line)){
        vector<string> tk; stringstream ss(line); string t; while(ss>>t) tk.push_back(t);
        if(tk.empty()) continue;
        // strace -c 每行格式: %time seconds usecs/call calls [errors] syscall
        //   errors 列仅当非 0 时出现 -> 列数不定, 但 calls 恒在第 4 列 (0-based index 3).
        if(tk.back()=="total" && tk.size()>=4){
            s.total = atoll(tk[3].c_str());                // calls 列 (第4列, 0-based 3)
        } else if(tk.back()=="write" && tk.size()>=4){
            s.write = atoll(tk[3].c_str());                // calls 列 (第4列, 0-based 3)
        }
    }
    return s;
}
// 多层级单点: 墙钟 + cachegrind(Ir+miss) [+ syscall 若 inclSysc]
static ML run_ml(const string& bin,const string& in,const string& name,bool inclSysc){
    ML m;
    m.wall = run_wall(bin,in);
    m = run_cgmiss(bin,in,"ml_"+name);
    m.wall = run_wall(bin,in); // cachegrind 跑完再测一次原生墙钟, 避免首跑冷启偏差
    if(inclSysc){
        SysC sc = run_syscall(bin,in,"ml_"+name);
        m.sysc_total = sc.total; m.sysc_write = sc.write;
    }
    return m;
}

static void worker(int id){
    cpu_set_t cs; CPU_ZERO(&cs); CPU_SET(id%CORES,&cs);
    pthread_setaffinity_np(pthread_self(),sizeof(cs),&cs);
    uint64_t t0=0; bool busy=false;
    auto nowns=[]()->uint64_t{ return chrono::duration_cast<chrono::nanoseconds>(
        chrono::steady_clock::now().time_since_epoch()).count(); };
    while(true){
        int type; string path,name;
        { unique_lock<mutex> lk(g_q);
          g_cv.wait(lk,[&]{ return !g_tasks.empty() || g_done.load(); });
          if(g_tasks.empty() && g_done.load()) break;
          tie(type,path,name)=g_tasks.front(); g_tasks.pop(); }
        if(!busy){ t0=nowns(); busy=true; }

        if(type==0){
            string ob,oz;
            if(!G_ENV_KEY.empty()) setenv(G_ENV_KEY.c_str(), G_ENV_VAL.c_str(), 1);
            int rb=run_prog(BIN,path,ob);
            if(!G_ENV_KEY.empty()) unsetenv(G_ENV_KEY.c_str());
            bool same=true;
            if(!BASE_BIN.empty()){
                int rz=run_prog(BASE_BIN,path,oz);
                same=(rb==0&&rz==0&&ob==oz);
            } else { same=(rb==0); }
            if(!same){ lock_guard<mutex> lk(g_out);
                cout<<"DIFF "<<name<<" rb="<<rb<<" lb="<<ob.size()<<" lz="<<oz.size()<<"\n"<<flush; }
            if(!same) g_fails++;
        } else if(type==1){
            long long ir=run_cg(BIN,path,"base_"+name);
            { lock_guard<mutex> lk(g_cgmap); g_cg[name]=ir; }
        } else if(type==4){
            ML m=run_ml(BIN,path,name,g_include_sysc);
            { lock_guard<mutex> lk(g_cgmap); g_ml[name]=m; }
        } else if(type==7){
            ML m=run_ml(BASE_BIN,path,name,g_include_sysc);
            { lock_guard<mutex> lk(g_cgmap); g_ml_base[name]=m; }
        } else if(type==5){
            SysC sc=run_syscall(BIN,path,name);
            ML m; m.sysc_total=sc.total; m.sysc_write=sc.write;
            { lock_guard<mutex> lk(g_cgmap); g_ml[name]=m; }
        } else if(type==3){
            extern map<string,vector<string>> g_sweep;
            for(auto& kv : g_sweep){
                string key=kv.first;
                for(auto& val : kv.second){
                    setenv(key.c_str(), val.c_str(), 1);
                    string tag = key+"_"+val+"_"+name;
                    long long ir=run_cg(BIN,path,tag);
                    unsetenv(key.c_str());
                    { lock_guard<mutex> lk(g_cgmap); g_cgsweep[key][val]+=ir; }
                }
            }
        }
        g_total++;
        uint64_t t1=nowns(); if(id<64) g_busy[id].fetch_add(t1-t0); busy=false;
        { lock_guard<mutex> lk(g_out); cout<<"[w"<<id<<"] done "<<name<<" (left="<<g_tasks.size()<<")"<<"\n"<<flush; }
    }
}

int main(int argc,char** argv){
    string mode="both";
    int workers=14;
    string sweeparg="";
    for(int i=1;i<argc;i++){
        string a=argv[i];
        if(a=="--mode"&&i+1<argc) mode=argv[++i];
        else if(a=="--bin"&&i+1<argc) BIN=argv[++i];
        else if(a=="--base-bin"&&i+1<argc) BASE_BIN=argv[++i];
        else if(a=="--cases-dir"&&i+1<argc) CASES_DIR=argv[++i];
        else if(a=="--single"&&i+1<argc) SINGLE=argv[++i];
        else if(a=="--sweep"&&i+1<argc) sweeparg=argv[++i];
        else if(a=="--env"&&i+1<argc){ string e=argv[++i]; size_t eq=e.find('=');
            if(eq!=string::npos){ G_ENV_KEY=e.substr(0,eq); G_ENV_VAL=e.substr(eq+1); } }
        else if(a=="--workers"&&i+1<argc) workers=atoi(argv[++i]);
        else if(a=="--minbytes"&&i+1<argc) MINBYTES=atoll(argv[++i]);
        else if(a=="--maxbytes"&&i+1<argc) MAXBYTES=atoll(argv[++i]);
        else if(a=="--reps"&&i+1<argc) REPS=max(1,atoi(argv[++i]));
        else if(a=="--rb-thresh"&&i+1<argc) RB_THRESH=atof(argv[++i]);
        else if(a=="--target"&&i+1<argc) TARGET=argv[++i];
        else if(a=="--syscall") DO_SYSCALL=true;
        else if(a=="--collect-systime") COLLECT_SYSTIME=true;
    }
    workers=max(1,min(workers,CORES));
    g_include_sysc = DO_SYSCALL || (mode=="full");

    if(!sweeparg.empty()){
        size_t c=sweeparg.find(':');
        if(c!=string::npos){
            string key=sweeparg.substr(0,c);
            string vals=sweeparg.substr(c+1);
            stringstream ss(vals); string v;
            while(getline(ss,v,',')) if(!v.empty()) g_sweep[key].push_back(v);
        }
    }

    vector<string> cases;
    if(!SINGLE.empty()) cases.push_back(SINGLE);
    else {
        cases = list_in(CASES_DIR);
        vector<string> f;
        for(auto&c:cases){ long long sz=fsize(c);
            if(sz>=MINBYTES && sz<=MAXBYTES) f.push_back(c); }
        cases=f;
    }

    bool doCorrect=(mode=="correct"||mode=="both");
    bool doCg     =(mode=="cg"||mode=="both");
    bool doSweep  =(mode=="cgsweep");
    bool doCgFiles=(mode=="cgfiles");
    bool doMl     =(mode=="multilevel"||mode=="rb"||mode=="full");
    bool doSysOnly=(mode=="syscall");
    bool doRb     =(mode=="rb");

    { lock_guard<mutex> lk(g_q);
      for(auto&c:cases){
          if(doCg || doCgFiles)             g_tasks.push({1,c,bname(c)});
          if(doSweep && !g_sweep.empty())   g_tasks.push({3,c,bname(c)});
          if(doCorrect && !BASE_BIN.empty()) g_tasks.push({0,c,bname(c)});
          if(doMl)                          g_tasks.push({4,c,bname(c)});
          if(doMl && doRb && !BASE_BIN.empty()) g_tasks.push({7,c,"base_"+bname(c)});
          if(doSysOnly)                     g_tasks.push({5,c,bname(c)});
      } }
    int ntasks=g_tasks.size();
    cout<<"BIN="<<BIN<<" BASE="<<(BASE_BIN.empty()?"(none)":BASE_BIN)
        <<" CASES="<<cases.size()<<" TASKS="<<ntasks<<" workers="<<workers
        <<" mode="<<mode<<" sweep="<<sweeparg
        <<" minB="<<MINBYTES<<" maxB="<<MAXBYTES<<" reps="<<REPS
        <<" sysc="<<(g_include_sysc?"on":"off")<<"\n"<<flush;

    auto wall0=chrono::steady_clock::now();
    vector<thread> thr;
    for(int i=0;i<workers;i++) thr.emplace_back(worker,i);
    { lock_guard<mutex> lk(g_q); g_done=true; } g_cv.notify_all();
    for(auto&t:thr) t.join();
    auto wall1=chrono::steady_clock::now();

    uint64_t wall_ns=chrono::duration_cast<chrono::nanoseconds>(wall1-wall0).count();
    uint64_t busy_tot=0; for(int i=0;i<workers;i++) busy_tot+=g_busy[i];
    double util=100.0*busy_tot/(workers*wall_ns);

    if(doCorrect) cout<<"CORRECT tot="<<cases.size()<<" fails="<<g_fails.load()<<"\n"<<flush;
    if(doSweep && !g_sweep.empty()){
        for(auto& kv : g_sweep){
            string key=kv.first; const auto& vals=kv.second;
            long long base = g_cgsweep[key][vals.front()];
            cout<<"SWEEP "<<key<<":";
            for(auto& v: vals){
                long long ir=g_cgsweep[key][v];
                double d = base? 100.0*(base-ir)/base : 0;
                cout<<" "<<v<<"="<<ir<<" ("<<fixed<<setprecision(2)<<d<<"%)";
            }
            cout<<"\n"<<flush;
        }
    }
    if(doCg && !g_cg.empty()){
        long long tb=0; for(auto&kv:g_cg) tb+=kv.second;
        cout<<"CG TOTAL base="<<tb<<"\n"<<flush;
    }
    if(doCgFiles && !g_cg.empty()){
        vector<pair<string,long long>> rows(g_cg.begin(), g_cg.end());
        sort(rows.begin(), rows.end());
        cout<<"CG FILES ("<<rows.size()<<"):\n"<<flush;
        for(auto&kv:rows) cout<<"  "<<kv.first<<" Ir="<<kv.second<<"\n"<<flush;
    }

    auto fmtM=[&](long long x)->double{ return x/1e6; };
    if((doMl||doSysOnly) && !g_ml.empty()){
        vector<pair<string,ML>> rows(g_ml.begin(), g_ml.end());
        sort(rows.begin(), rows.end(), [](const auto&a,const auto&b){return a.first<b.first;});
        cout<<"MULTILEVEL (wall_ms / Ir(M) / D1mr / DLmr / LLmr / D1mw / DLmw / LLmw";
        if(g_include_sysc||doSysOnly) cout<<" / sysc_total / sysc_write";
        cout<<"):\n"<<flush;
        for(auto&kv:rows){
            const ML& m=kv.second;
            cout<<"  "<<kv.first
                <<" wall="<<m.wall
                <<" Ir="<<fmtM(m.Ir)
                <<" D1mr="<<m.D1mr<<" DLmr="<<m.DLmr<<" LLmr="<<m.LLmr
                <<" D1mw="<<m.D1mw<<" DLmw="<<m.DLmw<<" LLmw="<<m.LLmw;
            if(g_include_sysc||doSysOnly) cout<<" sysc="<<m.sysc_total<<" syscW="<<m.sysc_write;
            cout<<"\n"<<flush;
        }
    }

    // ── rb 回归/改善报告 ───────────────────────────────────────────────
    if(doRb && !g_ml_base.empty() && !g_ml.empty()){
        vector<pair<string,string>> rows; // (name, name_for_map)
        for(auto&kv:g_ml) rows.push_back({kv.first,kv.first});
        sort(rows.begin(),rows.end());
        string rep;
        rep += "\n════ RB 回归/改善报告 (BASE=" + BASE_BIN + ") ════\n";
        rep += "TARGET=" + (TARGET.empty()?"(none)":TARGET) + "  Ir退化阈值=" + to_string(RB_THRESH) + "%\n";
        rep += "CASE                 | Ir%     | wall%   | LLmr%   | sysc%   | flag\n";
        int ir_regress=0; double tgt_ir=0,tgt_wall=0,tgt_llmr=0,tgt_sysc=0; int tgt_wins=0;
        for(auto&pr:rows){
            string nm=pr.first;
            auto it=g_ml.find(nm), itb=g_ml_base.find("base_"+nm);
            if(it==g_ml.end()||itb==g_ml_base.end()) continue;
            const ML &a=it->second, &b=itb->second;
            double dir = b.Ir? 100.0*(a.Ir-b.Ir)/b.Ir : 0;   // 正=退化(变多)
            double dwall= b.wall? 100.0*(a.wall-b.wall)/b.wall : 0;
            double dllmr= b.LLmr? 100.0*(a.LLmr-b.LLmr)/b.LLmr : 0;
            double dsysc= b.sysc_total? 100.0*(a.sysc_total-b.sysc_total)/b.sysc_total : 0;
            string flag = (dir>RB_THRESH)? "⚠IR_REGRESS" : "ok";
            if(dir>RB_THRESH) ir_regress++;
            if(nm==TARGET || nm==TARGET+".in"){
                tgt_ir=dir; tgt_wall=dwall; tgt_llmr=dllmr; tgt_sysc=dsysc;
                if(dir< -0.01) tgt_wins++;
                if(dwall< -0.01) tgt_wins++;
                if(dllmr< -0.01) tgt_wins++;
                if(dsysc< -0.01) tgt_wins++;
            }
            char buf[512];
            snprintf(buf,sizeof(buf),"%-20s | %+7.2f | %+7.2f | %+7.2f | %+7.2f | %s\n",
                     nm.c_str(), dir, dwall, dllmr, dsysc, flag.c_str());
            rep += buf;
        }
        rep += "────────────────────────────────────────────────────────\n";
        rep += "Ir 退化点 (> "+to_string(RB_THRESH)+"%): "+to_string(ir_regress)+" 个\n";
        if(!TARGET.empty()){
            rep += "TARGET("+TARGET+") 改善: Ir="+to_string(tgt_ir)+"% wall="+to_string(tgt_wall)
                  +"% LLmr="+to_string(tgt_llmr)+"% sysc="+to_string(tgt_sysc)
                  +"%  -> 赢 "+to_string(tgt_wins)+" 个指标(负=改善)\n";
        }
        rep += "结论: 其他测试点 Ir " + string(ir_regress?"有退化 ❌":"无退化 ✅(红利保留)") + "\n";
        cout<<rep<<flush;
        g_report = rep;
    }

    cout<<"UTIL workers="<<workers<<" wall_s="<<fixed<<setprecision(1)<<(wall_ns/1e9)
        <<" busy_sum_s="<<(busy_tot/1e9)<<" util="<<util<<"%\n"<<flush;
    return 0;
}
