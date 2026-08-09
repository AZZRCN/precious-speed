// ============================================================================
// so_core — 真·超优化器搜索核心 (替代 gcc modifield/_o4_impl/superopt_v2.py)
//
// 与旧版的根本区别:
//   旧版: 枚举 mnemonic 字符串的笛卡尔积, 只累加 latency, verified=true 写死,
//         从不执行指令 => 搜 fma 和搜 max 结果相同 (空壳)。
//   本版: 每个中间值是一个「值向量」(K 个测试输入下的真实 uint64 结果),
//         指令真正被求值; 命中条件是值向量逐元素等于目标 => 语义真实。
//
// 搜索策略: IDDFS (迭代加深) + 5 类剪枝
//   P1 等价类剪枝  同一值向量已在池中 -> 无信息增益, 剪
//   P2 代价上界    cost_so_far + 剩余最小代价 >= best -> 剪
//   P3 canonical   交换律 op 强制 i<=j; 禁止 const op const
//   P4 平凡剪枝    x-x / x&x / x|x / x^x / x*1 / x+0 / shift>=64 等
//   P5 goal 计数   未命中 goal 数 > 剩余深度 -> 剪
//
// 代价模型: Zen3 (uops, latency) 双目标。主排序 uops —— 因为 DIV 的目标热点
//   carryPropSeg 实测 D1mr≈0 属纯指令吞吐 bound, 减 uop 才直接兑现。
//
// 用法: so_core spec.json
// ============================================================================
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <random>
#include <thread>
#include <atomic>
#include <mutex>

// ---- 跨编译器 bit intrinsics (GCC/Clang __builtin_* 在 MSVC 无定义) ----
// SO 是纯成本模型模拟, 可用本机最合适的编译器 (Windows 首选 MSVC /O2 /GL /arch:AVX2)
// 编译以提升「SO 本身」的运行速度; 搜索的目标微架构仍固定为 LC/AMD Zen3。
#if defined(_MSC_VER)
  #include <intrin.h>
  static inline int so_ctzll(uint64_t x){ unsigned long r; _BitScanForward64(&r, x); return (int)r; }
  static inline int so_clzll(uint64_t x){ unsigned long r; _BitScanReverse64(&r, x); return 63 - (int)r; }
  static inline int so_popcountll(uint64_t x){ return (int)__popcnt64(x); }
  static inline int so_popcount(unsigned x){ return (int)__popcnt(x); }
#else
  static inline int so_ctzll(uint64_t x){ return __builtin_ctzll(x); }
  static inline int so_clzll(uint64_t x){ return __builtin_clzll(x); }
  static inline int so_popcountll(uint64_t x){ return __builtin_popcountll(x); }
  static inline int so_popcount(unsigned x){ return __builtin_popcount(x); }
#endif

// ---------------------------------------------------------------- 值向量
static constexpr int KMAX = 32;
static thread_local int K = 16;        // 实际测试输入个数 (每线程独立)

struct Vec {
    uint64_t v[KMAX];
    bool operator==(const Vec &o) const {
        for (int i = 0; i < K; i++) if (v[i] != o.v[i]) return false;
        return true;
    }
};
struct VecHash {
    size_t operator()(const Vec &a) const {
        uint64_t h = 1469598103934665603ULL;
        for (int i = 0; i < K; i++) { h ^= a.v[i]; h *= 1099511628211ULL; }
        return (size_t)h;
    }
};

// ---------------------------------------------------------------- 操作集
// 端口掩码 (AMD Zen3 整数域): bit0..3 = ALU0..ALU3
#define P_ALL  0xFu   // 任意 ALU
#define P_12   0x6u   // ALU1 / ALU2  (移位、位域)
#define P_1    0x2u   // ALU1 only    (乘法、pdep/pext)
#define P_01   0x3u   // ALU0 / ALU1  (带 scale 的 LEA)
static const int NPORT = 4;

enum OpKind {
    OP_ADD, OP_SUB, OP_MUL, OP_MULHI, OP_AND, OP_OR, OP_XOR,
    OP_SHL, OP_SHR, OP_SAR, OP_NOT, OP_NEG, OP_LEA2, OP_LEA4, OP_LEA8,
    OP_CMPLT, OP_MINU, OP_MAXU, OP_ROL, OP_BZHI, OP_ANDN,
    // ---- 以下为本轮补齐 (BMI1 / BMI2 / ABM 全套标量) ----
    OP_ROR, OP_BEXTR, OP_BLSI, OP_BLSR, OP_BLSMSK,
    OP_TZCNT, OP_LZCNT, OP_POPCNT, OP_PDEP, OP_PEXT,
    OP_CMPEQ, OP_CMPGT, OP_SBBMASK,
    OP_COUNT
};
// uops = 未融合域微指令数; lat = 结果延迟(周期); ports = 可用端口掩码
struct OpInfo { const char *name; int arity; int uops; int lat; bool commut; unsigned ports; };
static const OpInfo OPS[OP_COUNT] = {
    {"add",    2, 1, 1, true , P_ALL},
    {"sub",    2, 1, 1, false, P_ALL},
    {"mul",    2, 1, 3, true , P_1  },   // imul r64,r64 (低 64 位)
    {"mulhi",  2, 2, 4, true , P_1  },   // mulx/mul -> 高 64 位
    {"and",    2, 1, 1, true , P_ALL},
    {"or",     2, 1, 1, true , P_ALL},
    {"xor",    2, 1, 1, true , P_ALL},
    {"shl",    2, 1, 1, false, P_12 },   // shlx
    {"shr",    2, 1, 1, false, P_12 },
    {"sar",    2, 1, 1, false, P_12 },
    {"not",    1, 1, 1, false, P_ALL},
    {"neg",    1, 1, 1, false, P_ALL},
    {"lea2",   2, 1, 1, false, P_01 },   // a + b*2  (scaled LEA 限 ALU0/1)
    {"lea4",   2, 1, 1, false, P_01 },
    {"lea8",   2, 1, 1, false, P_01 },
    {"cmplt",  2, 2, 2, false, P_ALL},   // cmp + setb
    {"minu",   2, 2, 2, true , P_ALL},   // cmp + cmov
    {"maxu",   2, 2, 2, true , P_ALL},
    {"rol",    2, 1, 1, false, P_12 },
    {"bzhi",   2, 1, 1, false, P_12 },   // a & ((1<<b)-1)          BMI2
    {"andn",   2, 1, 1, false, P_ALL},   // ~a & b                  BMI1
    {"ror",    2, 1, 1, false, P_12 },
    {"bextr",  2, 1, 1, false, P_12 },   // (a>>(b&255)) & ((1<<(b>>8))-1)  BMI1
    {"blsi",   1, 1, 2, false, P_12 },   // a & -a                  BMI1
    {"blsr",   1, 1, 2, false, P_12 },   // a & (a-1)               BMI1
    {"blsmsk", 1, 1, 2, false, P_12 },   // a ^ (a-1)               BMI1
    {"tzcnt",  1, 1, 2, false, P_12 },   // BMI1
    {"lzcnt",  1, 1, 2, false, P_12 },   // ABM
    {"popcnt", 1, 1, 1, false, P_12 },   // ABM
    {"pdep",   2, 1, 3, false, P_1  },   // BMI2 (Zen3 起才是快速硬件实现)
    {"pext",   2, 1, 3, false, P_1  },   // BMI2
    {"cmpeq",  2, 2, 2, true , P_ALL},   // cmp + sete
    {"cmpgt",  2, 2, 2, false, P_ALL},   // cmp + seta  (无符号 a>b)
    {"sbbmask",2, 2, 2, false, P_ALL},   // cmp + sbb -> a<b ? ~0 : 0
};

static inline uint64_t mulhi64(uint64_t a, uint64_t b) {
    // MSVC 不支持 __int128 (error C4235) —— 这是此前 MSVC 构建一直静默回落到
    // g++ 的真正原因; 用 _umul128 内建等价实现。
#if defined(_MSC_VER)
    uint64_t hi;
    _umul128(a, b, &hi);
    return hi;
#else
    return (uint64_t)(((unsigned __int128)a * b) >> 64);
#endif
}

// 单元素求值; 返回 false 表示该组合非法(应剪枝)
static inline bool eval1(int op, uint64_t a, uint64_t b, uint64_t &out) {
    switch (op) {
    case OP_ADD:   out = a + b; break;
    case OP_SUB:   out = a - b; break;
    case OP_MUL:   out = a * b; break;
    case OP_MULHI: out = mulhi64(a, b); break;
    case OP_AND:   out = a & b; break;
    case OP_OR:    out = a | b; break;
    case OP_XOR:   out = a ^ b; break;
    case OP_SHL:   if (b >= 64) return false; out = a << b; break;
    case OP_SHR:   if (b >= 64) return false; out = a >> b; break;
    case OP_SAR:   if (b >= 64) return false; out = (uint64_t)((int64_t)a >> b); break;
    case OP_NOT:   out = ~a; break;
    case OP_NEG:   out = (uint64_t)(-(int64_t)a); break;
    case OP_LEA2:  out = a + b * 2; break;
    case OP_LEA4:  out = a + b * 4; break;
    case OP_LEA8:  out = a + b * 8; break;
    case OP_CMPLT: out = (a < b) ? 1 : 0; break;
    case OP_MINU:  out = a < b ? a : b; break;
    case OP_MAXU:  out = a > b ? a : b; break;
    case OP_ROL:   { unsigned s = (unsigned)(b & 63); out = s ? ((a << s) | (a >> (64 - s))) : a; } break;
    case OP_BZHI:  { unsigned s = (unsigned)b; out = (s >= 64) ? a : (a & ((1ULL << s) - 1)); } break;
    case OP_ANDN:  out = (~a) & b; break;
    case OP_ROR:   { unsigned s = (unsigned)(b & 63); out = s ? ((a >> s) | (a << (64 - s))) : a; } break;
    case OP_BEXTR: { unsigned st = (unsigned)(b & 0xFF), ln = (unsigned)((b >> 8) & 0xFF);
                     if (st >= 64) { out = 0; break; }
                     uint64_t t = a >> st;
                     out = (ln >= 64) ? t : (t & ((1ULL << ln) - 1)); } break;
    case OP_BLSI:   out = a & (uint64_t)(-(int64_t)a); break;
    case OP_BLSR:   out = a & (a - 1); break;
    case OP_BLSMSK: out = a ^ (a - 1); break;
    case OP_TZCNT:  out = a ? (uint64_t)so_ctzll(a) : 64; break;
    case OP_LZCNT:  out = a ? (uint64_t)so_clzll(a) : 64; break;
    case OP_POPCNT: out = (uint64_t)so_popcountll(a); break;
    case OP_PDEP:  { uint64_t r = 0, m = b, s = a;
                     while (m) { uint64_t lo = m & (uint64_t)(-(int64_t)m);
                                 if (s & 1) r |= lo; s >>= 1; m ^= lo; }
                     out = r; } break;
    case OP_PEXT:  { uint64_t r = 0, m = b, s = a; int k = 0;
                     while (m) { uint64_t lo = m & (uint64_t)(-(int64_t)m);
                                 if (s & lo) r |= (1ULL << k); k++; m ^= lo; }
                     out = r; } break;
    case OP_CMPEQ:   out = (a == b) ? 1 : 0; break;
    case OP_CMPGT:   out = (a > b)  ? 1 : 0; break;
    case OP_SBBMASK: out = (a < b)  ? ~0ULL : 0ULL; break;
    default: return false;
    }
    return true;
}

// ---------------------------------------------------------------- 状态
struct Step { int op, i, j; };          // pool[i] op pool[j] -> 新槽

// ===== 以下为「每线程」搜索状态: 多线程时每线程独立副本, 各自保持强去重 =====
static thread_local std::vector<Vec>      pool;      // 当前可用值 (inputs+consts+已算出的)
static thread_local std::vector<int>      poolKind;   // 0=var 1=num-const 2=shift-const
static thread_local std::vector<int>      poolReady;  // 该值就绪的周期 (输入/常量=0) —— 关键路径用
// 该值是否(传递地)依赖于「链上输入」。不依赖的子表达式可在链上数据到达前
// 预先算完 —— 它们占端口(计入 res), 但在关键路径上是 0 拍。
// 对 carryPropSeg: v_k 从数组读(可预取)=非链上; c_k 是上一步的进位=链上。
static thread_local std::vector<char>     poolCrit;
static thread_local std::vector<std::string> poolName;
// 断点续搜: 只存值向量的 FNV-1a 64 位哈希。恢复时把哈希灌回即可,
// DFS 再次展开时靠哈希去重续上搜索 —— 无需序列化原始 256 字节向量。
// (64 位哈希碰撞概率在 K=32、规模 <1e6 时可忽略; 即便碰撞也只是过度剪枝,
//  真实候选须经 Python 侧 40 万样本独立重放验证兜底, 不会产生错误候选。)
static thread_local std::unordered_set<size_t> poolSet;

static thread_local std::vector<int>      allowedOps;
static thread_local std::vector<Vec>      goals;
static thread_local std::vector<std::string> goalName;
// 该 goal 是否落在跨迭代的串行依赖链上 (例如进位 c_k -> c_{k+1})。
// 只有关键 goal 参与关键路径统计; 非关键 goal (如写回内存的余数) 只吃吞吐。
// 缺省全部视为关键 —— 保守, 不会低估。
static thread_local std::vector<int>      goalCrit;
static thread_local int   nBase = 0;                 // inputs+consts 数量
static thread_local int   maxDepth = 4;
static thread_local int   bestUops = 1 << 30, bestLat = 1 << 30;

// ---- Zen3 周期模型 ----------------------------------------------------
//  一条序列的执行周期下界 = max( 关键路径, 端口资源下界 )
//    关键路径 : 沿数据依赖 DAG 的最长 latency 路径 (poolReady 增量维护)
//    资源下界 : 对每个端口子集 S, 只能落在 S 内的 uop 总数 / |S|  (Hall 条件)
//               枚举 15 个子集即得精确的分数匹配下界; S=全集时退化为 uops/4
//  costMode: 0=吞吐优先(热点可并行)  1=延迟优先(热点在串行依赖链上)  2=两者取大
static thread_local int maskUops[16];    // 按端口掩码分桶的 uop 计数
static thread_local int costMode = 2;

static int resBound100() {        // 返回 周期*100
    int best = 0;
    for (unsigned S = 1; S < 16; S++) {
        int sum = 0;
        for (unsigned m = 1; m < 16; m++)
            if (maskUops[m] && (m & ~S) == 0) sum += maskUops[m];
        if (!sum) continue;
        int np = so_popcount(S);
        int c = (sum * 100 + np - 1) / np;
        if (c > best) best = c;
    }
    return best;
}
// 综合代价 (×100), 供候选排序与 best 更新
static inline int cycles100(int cp, int res) {
    if (costMode == 0) return res;
    if (costMode == 1) return cp * 100;
    return std::max(cp * 100, res);
}
static thread_local std::vector<Step> bestSeq;
static thread_local std::vector<Step> curSeq;
static thread_local uint64_t nodes = 0, prunedEq = 0, prunedCost = 0, prunedTriv = 0, prunedDead = 0;
static thread_local double  timeLimit = 60.0;
static thread_local std::chrono::steady_clock::time_point tStart;
static thread_local int     minOpUops = 1;
static thread_local bool    verbose = false;
static thread_local int     nFound = 0;
static thread_local bool    findAll = false;

// 多线程分片: 本线程只负责以 rootSlice 中的指令作为「第一步」(depth==0)。
// depth>=1 仍用全部 allowedOps。各线程第一步互不相交 => 顶层表达式不重复,
// 子表达式层(>=1)可跨线程冗余展开, 但每线程内部 P1 等价类去重依旧成立(强去重)。
static thread_local std::vector<int> rootSlice;

// ---- 多进程分片 (取代 in-process std::thread, 进程间独立地址空间 => 零数据竞争) ----
// 每个 so_core 进程认领「第一步 (op,i,j)」的一个子集: 以 depth==0 处一个确定性
// 计数器 rootCtr 取模 shardN, 命中 shardK 才展开。计数点在 P1 去重之后 (base pool
// 相同 => 各进程枚举顺序一致), 保证 shardN 个进程的第一步互斥且并集=全搜索。
// depth>=1 仍展开全部 allowedOps => 每进程内部强去重完整, 结果正确性与单进程一致。
static int       shardK = 0;      // 本进程编号 [0, shardN)
static int       shardN = 1;      // 总进程数
static long long rootCtr = 0;     // depth==0 第一步计数器 (每层 IDDFS 重置)

// ---- 共享(只读)控制 + 跨线程信号 ----
static bool    doResume = false;
static bool    doTestSem = false;
static std::string ckptPath = "so_ckpt.bin";
static bool    ckptEnabled = false;
static std::atomic<bool> g_timedOut{false};   // 任一线程超时即全停
static bool    g_multithread = false;          // 决定是否落盘 ckpt(多线程不写, 避免互相覆盖)
static bool    save_ckpt(int depth);   // 前向声明: 落盘 depth+bestUops+bestLat+poolSet哈希

// ------- 候选收集 (两阶段策略: 小 K 快筛 -> 收集 -> 事后大 K + 随机重放验证) -------
//   搜索期只用少量测试向量, 命中即视为「有潜力」并收集, 不立刻当成答案。
//   小 K 会带来两类误差, 全部由事后验证兜底:
//     a) 假阳性: 恰好在这几个点上相等 -> 事后验证淘汰
//     b) 假合并: P1 等价类在小 K 下把不同函数当成同一个 -> 靠多候选 + 多种子对冲
struct Cand {
    int uops, lat, insns;
    int cp;        // 关键路径 (周期)
    int res100;    // 端口资源下界 (周期*100)
    int cyc100;    // 综合代价 (周期*100)
    std::vector<std::string> exprs;      // 每个 goal 一条表达式
};
static std::vector<Cand> cands;
static int  maxCands = 200;
// 允许收集 uops <= bestUops + slack 的候选。
// 必须 >0 : 最小 uops 的解未必是周期最优 —— 多 1~2 个 uop 换更短关键路径,
// 在串行依赖链热点上通常更快。事后按 cycles 重排即可。
static int  slack    = 2;

static bool candSeen(const Cand &c) {
    for (auto &o : cands) if (o.exprs == c.exprs) return true;
    return false;
}

// goal 是否已全部在 pool 中; 返回未命中数
static int goalsMissing() {
    int miss = 0;
    for (auto &g : goals) if (!poolSet.count(VecHash()(g))) miss++;
    return miss;
}

// x86-64 的一条 `mulq r64` 同时写 rdx:rax —— 即同一对操作数的 mulhi 与 mul
// 共享同一条指令。若 curSeq 中已存在配对的另一半, 本步增量 uops 记为 0。
// P6 死代码前瞻剪枝 (最强的一条):
//   已产出但「后续从未被引用、且本身不是任何 goal」的中间值 = 悬空值。
//   每多走一步, 悬空数最多净减 1 (消耗 ≤2 个, 又产出 1 个新的)。
//   终态要求悬空数为 0, 故 unused > remaining 时该分支必然产生死代码 -> 剪。
//
// !!! 2026-08-07 修正 (原实现 off-by-miss, 静默漏解) !!!
//   上面「每步最多净减 1」只对**产出非 goal**的步成立。若该步产出的正是一个
//   goal, 则它被 unusedCount() 排除 => 该步净减 2 (消耗 2 个, 产出不计)。
//   剩余 remaining 步中至多有 miss 步是产 goal 的, 故最大总降幅 =
//        (remaining - miss)*1 + miss*2 = remaining + miss
//   正确判据应为  unused > remaining + miss  才剪。
//   原式 unused > remaining 会把「深子树 + 末步合并两个悬空值」整类程序剪掉:
//     反例 digits2  packed = sub( shl(y,8), mul(2559, shr(mul(y,103),10)) )
//     走到第 4 步时 unused={shl(y,8), mul(...)}=2, remaining=1 -> 2>1 被误剪,
//     而第 5 步 sub 恰好同时吃掉这两个悬空值并产出 goal, 是合法 5 条解。
//     (该式已 Python 全域 100/100 验证正确)
//   影响面: 所有「无解」结论均需以修正后的搜索重新判定 (含 digits4)。
static inline int unusedCount() {
    const int d = (int)curSeq.size();
    int u = 0;
    for (int t = 0; t < d; t++) {
        const int idx = nBase + t;
        bool used = false;
        for (int L = t + 1; L < d; L++)
            if (curSeq[L].i == idx || curSeq[L].j == idx) { used = true; break; }
        if (used) continue;
        bool isGoal = false;
        for (auto &g : goals) if (pool[idx] == g) { isGoal = true; break; }
        if (!isGoal) u++;
    }
    return u;
}

// 返回本步的 uops 增量; -1 表示不共享(按 OPS 表原值计)。
// 一条 mulq 总代价 2 uops, 已计入 mate 的部分要扣掉。
static inline int mulPairShareDelta(int op, int i, int j) {
    int mate = (op == OP_MUL) ? OP_MULHI : (op == OP_MULHI ? OP_MUL : -1);
    if (mate < 0) return -1;
    for (auto &s : curSeq)
        if (s.op == mate && ((s.i == i && s.j == j) || (s.i == j && s.j == i)))
            return 2 - OPS[mate].uops;      // MULHI 已发 -> +0 ; MUL 已发 -> +1
    return -1;
}

static void report(int uops, int lat) {
    if ((int)cands.size() >= maxCands) return;
    Cand c; c.uops = uops; c.lat = lat; c.insns = (int)curSeq.size();
    // 每个 goal 对应池中哪个表达式 —— Python 侧据此重放验证
    int cp = 0;
    for (size_t gi = 0; gi < goals.size(); gi++) {
        for (size_t pi = 0; pi < pool.size(); pi++) {
            if (pool[pi] == goals[gi]) {
                c.exprs.push_back(poolName[pi]);
                // 关键路径只由「参与跨迭代依赖」的 goal 决定
                if (gi >= goalCrit.size() || goalCrit[gi])
                    cp = std::max(cp, poolReady[pi]);
                break;
            }
        }
    }
    if (c.exprs.size() != goals.size()) return;
    if (candSeen(c)) return;
    c.cp = cp;
    c.res100 = resBound100();
    c.cyc100 = cycles100(cp, c.res100);
    cands.push_back(c);
    nFound++;
    if (verbose) { printf("[cand#%d] uops=%d cp=%d res=%.2f\n",
                          nFound, uops, cp, c.res100 / 100.0); fflush(stdout); }
}

// 所有搜索结束后统一输出候选, 按 (综合周期, 关键路径, uops, 指令数) 排序
static void dumpCands() {
    std::stable_sort(cands.begin(), cands.end(), [](const Cand &a, const Cand &b) {
        if (a.cyc100 != b.cyc100) return a.cyc100 < b.cyc100;
        if (a.cp     != b.cp    ) return a.cp     < b.cp;
        if (a.uops   != b.uops  ) return a.uops   < b.uops;
        return a.insns < b.insns;
    });
    for (auto &c : cands) {
        printf("FOUND cyc=%.2f cp=%d res=%.2f uops=%d insns=%d\n",
               c.cyc100 / 100.0, c.cp, c.res100 / 100.0, c.uops, c.insns);
        for (size_t gi = 0; gi < c.exprs.size(); gi++)
            printf("  GOAL %s = %s\n", goalName[gi].c_str(), c.exprs[gi].c_str());
    }
    fflush(stdout);
}

static void dfs(int depth, int uops, int lat) {
    if (g_timedOut.load(std::memory_order_relaxed)) return;
    if ((++nodes & 0xFFFF) == 0) {
        double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - tStart).count();
        if (el > timeLimit) {
            g_timedOut.store(true, std::memory_order_relaxed);
            if (ckptEnabled && !g_multithread) save_ckpt(maxDepth);
            return;
        }
    }
    if (depth == 0) rootCtr = 0;   // 每层 IDDFS 顶层重置分片计数器 (跨层一致)

    int miss = goalsMissing();
    if (miss == 0) {
        if (uops < bestUops || (uops == bestUops && lat < bestLat)) {
            bestUops = uops; bestLat = lat; bestSeq = curSeq;
        }
        report(uops, lat);           // 收集为候选(含同代价的其它解), 不当场判定
        return;                      // 已命中, 再加指令只会更贵
    }
    if (depth >= maxDepth) return;
    const int remaining = maxDepth - depth;
    // P5: 未命中 goal 数 > 剩余深度
    if (miss > remaining) return;
    // P2: 代价上界 —— 收集模式下放宽到 bestUops+slack, 以便留住同代价的备选解
    if (uops + miss * minOpUops > bestUops + slack) { prunedCost++; return; }
    // P6: 死代码前瞻 (修正: +miss —— 产 goal 的步净减 2 而非 1, 详见 unusedCount 上方注释)
    if (unusedCount() > remaining + miss) { prunedDead++; return; }

    const int n = (int)pool.size();
    // 多线程: depth==0 只用本线程分片 rootSlice; depth>=1 用全部 allowedOps
    for (int op : (depth == 0 ? rootSlice : allowedOps)) {
        const OpInfo &oi = OPS[op];
        int nuBase = uops + oi.uops;
        // 乐观下界用 0 增量(可能与已有 mulq 共享), 具体到操作数对时再定
        int nuOpt = (op == OP_MUL || op == OP_MULHI) ? uops : nuBase;
        if (nuOpt + (miss - 1) * minOpUops > bestUops + slack) { prunedCost++; continue; }
        int nu = nuBase;

        if (oi.arity == 1) {
            for (int i = 0; i < n; i++) {
                if (poolKind[i]) continue;                  // P3: 不对常量做一元运算
                Vec nv; bool ok = true;
                for (int t = 0; t < K; t++)
                    if (!eval1(op, pool[i].v[t], 0, nv.v[t])) { ok = false; break; }
                if (!ok) { prunedTriv++; continue; }
                if (poolSet.count(VecHash()(nv))) { prunedEq++; continue; }   // P1
                // 多进程分片: depth==0 按第一步取模认领 (计数点在 P1 之后, 各进程一致)
                if (depth == 0 && shardN > 1 && (rootCtr++ % shardN) != shardK) continue;
                pool.push_back(nv); poolKind.push_back(0);
                poolCrit.push_back(poolCrit[i]);
                // 不在链上的子表达式可预先算好 -> 关键路径记 0
                poolReady.push_back(poolCrit[i] ? poolReady[i] + oi.lat : 0);
                maskUops[oi.ports] += oi.uops;                     // 端口占用
                poolName.push_back(std::string(oi.name) + "(" + poolName[i] + ")");
                poolSet.insert(VecHash()(nv));
                curSeq.push_back({op, i, -1});
                dfs(depth + 1, nu, lat + oi.lat);
                curSeq.pop_back();
                poolSet.erase(VecHash()(nv)); poolName.pop_back();
                maskUops[oi.ports] -= oi.uops;
                poolReady.pop_back(); poolCrit.pop_back();
                poolKind.pop_back(); pool.pop_back();
            }
        } else {
            for (int i = 0; i < n; i++) {
                int jstart = oi.commut ? i : 0;
                for (int j = jstart; j < n; j++) {
                    // P3: 常量 op 常量 -> 编译期折叠, 无意义
                    if (poolKind[i] && poolKind[j]) continue;
                    // P4: 平凡组合
                    if (i == j) {
                        if (op == OP_SUB || op == OP_XOR || op == OP_ANDN) { prunedTriv++; continue; } // ->0
                        if (op == OP_AND || op == OP_OR || op == OP_MINU || op == OP_MAXU) { prunedTriv++; continue; } // ->x
                    }
                    // P7 常量分类: 移位量与数值常量互不串用 —— 控制爆炸的关键
                    const bool isShiftOp = (op == OP_SHL || op == OP_SHR || op == OP_SAR ||
                                            op == OP_ROL || op == OP_BZHI);
                    if (isShiftOp) { if (poolKind[j] != 2) continue; }
                    else           { if (poolKind[i] == 2 || poolKind[j] == 2) continue; }

                    // 与已发射的 mulq 共享 rdx:rax -> 只补差额
                    int shd = mulPairShareDelta(op, i, j);
                    int nuHere = (shd >= 0) ? (uops + shd) : nuBase;
                    if (nuHere + (miss - 1) * minOpUops > bestUops + slack) { prunedCost++; continue; }

                    Vec nv; bool ok = true;
                    for (int t = 0; t < K; t++)
                        if (!eval1(op, pool[i].v[t], pool[j].v[t], nv.v[t])) { ok = false; break; }
                    if (!ok) { prunedTriv++; continue; }
                    if (poolSet.count(VecHash()(nv))) { prunedEq++; continue; }   // P1
                    // 多进程分片: depth==0 按第一步取模认领 (计数点在 P1 之后, 各进程一致)
                    if (depth == 0 && shardN > 1 && (rootCtr++ % shardN) != shardK) continue;

                    // 关键路径: 两个操作数都就绪后才能发射; 若整个子表达式都不
                    // 依赖链上数据, 它可以预先算完 -> 关键路径记 0
                    const char crt = (char)(poolCrit[i] || poolCrit[j]);
                    const int rdy = crt ? std::max(poolReady[i], poolReady[j]) + oi.lat : 0;
                    const int duo = nuHere - uops;   // 本步实际 uops 增量(含 mulq 共享)

                    pool.push_back(nv); poolKind.push_back(0);
                    poolCrit.push_back(crt);
                    poolReady.push_back(rdy);
                    maskUops[oi.ports] += duo;
                    poolName.push_back(std::string(oi.name) + "(" + poolName[i] + "," + poolName[j] + ")");
                    poolSet.insert(VecHash()(nv));
                    curSeq.push_back({op, i, j});
                    dfs(depth + 1, nuHere, lat + oi.lat);
                    curSeq.pop_back();
                    poolSet.erase(VecHash()(nv)); poolName.pop_back();
                    maskUops[oi.ports] -= duo;
                    poolReady.pop_back(); poolCrit.pop_back();
                    poolKind.pop_back(); pool.pop_back();
                }
            }
        }
    }
}

// ---------------------------------------------------------------- 极简 JSON
// 只支持本工具生成的固定结构, 不做通用解析。
struct Json {
    std::string s; size_t p = 0;
    void skip() { while (p < s.size() && (isspace((unsigned char)s[p]))) p++; }
    bool findKey(const char *key, size_t from = 0) {
        std::string k = std::string("\"") + key + "\"";
        size_t q = s.find(k, from);
        if (q == std::string::npos) return false;
        p = s.find(':', q); p++; skip(); return true;
    }
    std::vector<uint64_t> readU64Array() {
        std::vector<uint64_t> r; skip();
        if (s[p] != '[') return r; p++;
        while (true) {
            skip();
            if (s[p] == ']') { p++; break; }
            if (s[p] == ',') { p++; continue; }
            size_t e; r.push_back(std::stoull(s.substr(p), &e, 10)); p += e;
        }
        return r;
    }
    std::vector<std::string> readStrArray() {
        std::vector<std::string> r; skip();
        if (s[p] != '[') return r; p++;
        while (true) {
            skip();
            if (s[p] == ']') { p++; break; }
            if (s[p] == ',') { p++; continue; }
            if (s[p] == '"') { size_t e = s.find('"', p + 1); r.push_back(s.substr(p + 1, e - p - 1)); p = e + 1; }
            else break;
        }
        return r;
    }
    long long readInt() { skip(); size_t e; long long v = std::stoll(s.substr(p), &e); p += e; return v; }
    double readDouble() { skip(); size_t e; double v = std::stod(s.substr(p), &e); p += e; return v; }
};

// ---------------------------------------------------------------- 断点续搜
// poolSet 已改为「值向量哈希集」, 故 checkpoint 只需落盘魔法数 + 维度 + 已完成
// 层次 + best + 哈希集合。恢复时重建初始 pool 并把哈希灌回 poolSet, DFS 再次
// 展开即靠哈希去重续上, 不会二次派生已见的值向量 (强去重保持)。
static bool save_ckpt(int depth) {
    if (!ckptEnabled) return false;
    std::ofstream f(ckptPath, std::ios::binary);
    if (!f) return false;
    const uint32_t magic = 0x534F434BU;       // "SOCK"
    f.write((const char*)&magic, 4);
    f.write((const char*)&K, 4);
    f.write((const char*)&nBase, 4);
    f.write((const char*)&depth, 4);
    f.write((const char*)&bestUops, 4);
    f.write((const char*)&bestLat, 4);
    uint64_t nh = poolSet.size();
    f.write((const char*)&nh, 8);
    for (size_t h : poolSet) f.write((const char*)&h, sizeof h);
    return (bool)f;
}

// 成功恢复返回 true; depth 传出上次完成的层次
static bool load_ckpt(int &depth) {
    std::ifstream f(ckptPath, std::ios::binary);
    if (!f) return false;
    uint32_t magic; f.read((char*)&magic, 4);
    if (magic != 0x534F434BU) return false;
    int k, nb;
    f.read((char*)&k, 4); f.read((char*)&nb, 4);
    if (k != K || nb != nBase) return false;   // spec 不匹配 -> 放弃续搜
    int bu, bl;
    f.read((char*)&depth, 4); f.read((char*)&bu, 4); f.read((char*)&bl, 4);
    bestUops = bu; bestLat = bl;
    uint64_t nh; f.read((char*)&nh, 8);
    for (uint64_t i = 0; i < nh; i++) {
        size_t h; f.read((char*)&h, sizeof h);
        if (f) poolSet.insert(h);
    }
    return true;
}

// ---------------------------------------------------------------- SEM 一致性自测
// 打印每条指令在结构化 + 随机输入下的 eval1 输出; so.py 用 Python SEM 重算对照,
// 堵住「SEM 与 eval1 同时对某条指令写错成一样」的漏网风险 (验证阶段也用 SEM 作
// 真值, 故两者必须逐位一致)。
static void run_sem_test() {
    std::mt19937_64 rng(0xC0FFEEULL);
    printf("BEGIN_SEMTEST\n");
    uint64_t ea[] = {0, 1, 2, 63, 64, 0xFFFFFFFFFFFFFFFFULL,
                     0x8000000000000000ULL, 123456789012345ULL, 0xAAAAAAAAAAAAAAAAULL};
    uint64_t eb[] = {0, 1, 2, 3, 63, 64, 65, 0xFFFFFFFFFFFFFFFFULL, 999999999ULL};
    for (int op = 0; op < OP_COUNT; op++) {
        for (uint64_t a : ea) for (uint64_t b : eb) {
            uint64_t out;
            if (eval1(op, a, b, out))
                printf("SEMCHECK %s %llu %llu %llu\n", OPS[op].name,
                       (unsigned long long)a, (unsigned long long)b, (unsigned long long)out);
        }
        for (int t = 0; t < 25; t++) {
            uint64_t a = rng(), b = rng(), out;
            if (eval1(op, a, b, out))
                printf("SEMCHECK %s %llu %llu %llu\n", OPS[op].name,
                       (unsigned long long)a, (unsigned long long)b, (unsigned long long)out);
        }
    }
    printf("END_SEMTEST\n");
}

// ---------------------------------------------------------------- 多线程驱动支撑
struct WorkerResult {
    std::vector<Cand> cands;
    int bestUops = 1 << 30, bestLat = 1 << 30;
    std::vector<Step> bestSeq;
    uint64_t nodes = 0, prunedEq = 0, prunedCost = 0, prunedTriv = 0, prunedDead = 0;
};

// 从 spec 文件重建「本线程」搜索状态。多线程时每个 worker 各自调用一次。
static int setup_thread_locals(const std::string &specPath) {
    FILE *f = fopen(specPath.c_str(), "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", specPath.c_str()); return -1; }
    std::string src; { char buf[65536]; size_t n; while ((n = fread(buf, 1, sizeof buf, f)) > 0) src.append(buf, n); }
    fclose(f);
    Json J; J.s = src;

    if (J.findKey("K")) K = (int)J.readInt();
    if (K > KMAX) K = KMAX;
    if (J.findKey("max_insn")) maxDepth = (int)J.readInt();
    if (J.findKey("time_limit")) timeLimit = J.readDouble();
    if (J.findKey("verbose")) verbose = J.readInt() != 0;
    if (J.findKey("find_all")) findAll = J.readInt() != 0;
    if (J.findKey("max_cands")) maxCands = (int)J.readInt();
    if (J.findKey("slack")) slack = (int)J.readInt();
    if (J.findKey("cost_mode")) costMode = (int)J.readInt();

    {
        std::vector<std::string> names;
        if (J.findKey("ops")) names = J.readStrArray();
        allowedOps.clear();
        if (names.empty()) { for (int i = 0; i < OP_COUNT; i++) allowedOps.push_back(i); }
        else for (auto &nm : names) {
            bool hit = false;
            for (int i = 0; i < OP_COUNT; i++) if (nm == OPS[i].name) { allowedOps.push_back(i); hit = true; break; }
            if (!hit) fprintf(stderr, "warn: unknown op '%s'\n", nm.c_str());
        }
    }
    minOpUops = 99; for (int op : allowedOps) minOpUops = std::min(minOpUops, OPS[op].uops);

    pool.clear(); poolKind.clear(); poolName.clear(); poolReady.clear(); poolCrit.clear(); poolSet.clear();

    std::vector<std::string> inNames;
    if (J.findKey("input_names")) inNames = J.readStrArray();
    for (size_t vi = 0; vi < inNames.size(); vi++) {
        char key[64]; snprintf(key, sizeof key, "input_%zu", vi);
        if (!J.findKey(key)) { fprintf(stderr, "missing %s\n", key); return -1; }
        auto a = J.readU64Array();
        Vec v{}; for (int t = 0; t < K; t++) v.v[t] = (t < (int)a.size() ? a[t] : 0);
        pool.push_back(v); poolKind.push_back(0); poolName.push_back(inNames[vi]);
    }
    std::vector<uint64_t> consts, shifts;
    if (J.findKey("consts")) consts = J.readU64Array();
    if (J.findKey("shifts")) shifts = J.readU64Array();
    for (uint64_t c : consts) {
        Vec v{}; for (int t = 0; t < K; t++) v.v[t] = c;
        pool.push_back(v); poolKind.push_back(1);
        char nb[32]; snprintf(nb, sizeof nb, "%llu", (unsigned long long)c);
        poolName.push_back(nb);
    }
    for (uint64_t c : shifts) {
        Vec v{}; for (int t = 0; t < K; t++) v.v[t] = c;
        pool.push_back(v); poolKind.push_back(2);
        char nb[32]; snprintf(nb, sizeof nb, "%llu", (unsigned long long)c);
        poolName.push_back(nb);
    }
    nBase = (int)pool.size();
    poolReady.assign(nBase, 0);
    poolCrit.assign(nBase, 0);
    {
        std::vector<uint64_t> ic;
        if (J.findKey("input_crit")) ic = J.readU64Array();
        for (size_t vi = 0; vi < inNames.size(); vi++)
            poolCrit[vi] = (char)(vi < ic.size() ? (ic[vi] != 0) : 1);
    }
    for (int m = 0; m < 16; m++) maskUops[m] = 0;
    for (auto &v : pool) poolSet.insert(VecHash()(v));

    // goals (必须在搜索前读取, 否则 goalsMissing() 恒为 0 会误判 depth0 即命中)
    if (J.findKey("goal_names")) goalName = J.readStrArray();
    if (J.findKey("goal_crit")) {
        auto a = J.readU64Array();
        for (uint64_t x : a) goalCrit.push_back((int)x);
    }
    for (size_t gi = 0; gi < goalName.size(); gi++) {
        char key[64]; snprintf(key, sizeof key, "goal_%zu", gi);
        if (!J.findKey(key)) { fprintf(stderr, "missing %s\n", key); return -1; }
        auto a = J.readU64Array();
        Vec v{}; for (int t = 0; t < K; t++) v.v[t] = (t < (int)a.size() ? a[t] : 0);
        goals.push_back(v);
    }

    int resumeDepth = 1;
    if (doResume && load_ckpt(resumeDepth)) {
        if (resumeDepth > maxDepth) resumeDepth = maxDepth;
        if (resumeDepth < 1) resumeDepth = 1;
        printf("[resume] from depth=%d best_uops=%s poolSet=%d\n", resumeDepth,
               bestUops == (1 << 30) ? "-" : std::to_string(bestUops).c_str(),
               (int)poolSet.size());
        fflush(stdout);
    }
    rootSlice = allowedOps;   // 单线程/缺省: 第一步可用全部 op; 分片时由调用方覆盖
    return resumeDepth;
}

// 单线程完整 IDDFS 驱动; 结果写回 out。printStats 仅主线程为 true。
static void run_idfs(int resumeDepth, int userMax, bool printStats, WorkerResult &out) {
    tStart = std::chrono::steady_clock::now();
    for (int d = resumeDepth; d <= userMax && !g_timedOut.load(std::memory_order_relaxed); d++) {
        maxDepth = d;
        dfs(0, 0, 0);
        if (ckptEnabled && !g_multithread) save_ckpt(d);
        if (printStats) {
            double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - tStart).count();
            printf("  depth<=%d done  nodes=%llu  t=%.2fs  best_uops=%s  cands=%d\n", d,
                   (unsigned long long)nodes, el,
                   bestUops == (1 << 30) ? "-" : std::to_string(bestUops).c_str(),
                   (int)cands.size());
            fflush(stdout);
        }
        if (bestUops != (1 << 30) && !findAll) break;
    }
    out.cands = cands; out.bestUops = bestUops; out.bestLat = bestLat; out.bestSeq = bestSeq;
    out.nodes = nodes; out.prunedEq = prunedEq; out.prunedCost = prunedCost;
    out.prunedTriv = prunedTriv; out.prunedDead = prunedDead;
}

int main(int argc, char **argv) {
    // 参数: 第一个非 -- 开头的是 spec.json; 其余为标志
    std::string specPath;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--resume") { doResume = true; ckptEnabled = true; }
        else if (a == "--ckpt") { if (i + 1 < argc) { ckptPath = argv[++i]; ckptEnabled = true; } }
        else if (a.rfind("--ckpt=", 0) == 0) { ckptPath = a.substr(7); ckptEnabled = true; }
        else if (a == "--test-sem") { doTestSem = true; }
        // 多进程分片: --shard k n  (本进程认领 k, 总 n 个) —— 取代崩溃的 in-process 线程
        else if (a == "--shard") { if (i + 2 < argc) { shardK = atoi(argv[++i]); shardN = atoi(argv[++i]); } }
        else if (a[0] != '-' && specPath.empty()) specPath = a;
    }
    if (shardN < 1) shardN = 1;
    if (shardK < 0 || shardK >= shardN) shardK = 0;
    if (doTestSem) { run_sem_test(); return 0; }   // 不需要 spec
    if (specPath.empty()) {
        fprintf(stderr, "usage: so_core [--resume] [--ckpt PATH] [--shard k n] [--test-sem] spec.json\n");
        return 1;
    }

    int rd = setup_thread_locals(specPath);
    if (rd < 0) return 1;
    g_multithread = false;          // 单进程单线程; 并行由多进程 (--shard) 提供 => 零数据竞争
    rootSlice = allowedOps;         // 第一步候选全集; 具体认领由 rootCtr%shardN 在 dfs 内决定

    int nIn = 0, nC = 0, nS = 0;
    for (int i = 0; i < nBase; i++) { if (poolKind[i] == 0) nIn++; else if (poolKind[i] == 1) nC++; else nS++; }
    static const char *CM[] = {"throughput", "latency", "max(both)"};
    printf("== so_core ==  K=%d  base=%d (%d in + %d const + %d shift)  goals=%d  ops=%d  max_insn=%d\n"
           "   cost=%s  slack=%d uops  ports=Zen3(4 ALU)  shard=%d/%d\n",
           K, nBase, nIn, nC, nS, (int)goals.size(),
           (int)allowedOps.size(), maxDepth,
           CM[costMode < 0 || costMode > 2 ? 2 : costMode], slack, shardK, shardN);
    fflush(stdout);

    auto masterStart = std::chrono::steady_clock::now();
    WorkerResult out;
    run_idfs(rd, maxDepth, true, out);

    double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - masterStart).count();
    printf("\n-- stats --  nodes=%llu  eq=%llu cost=%llu triv=%llu dead=%llu  time=%.2fs%s\n",
           (unsigned long long)nodes, (unsigned long long)prunedEq,
           (unsigned long long)prunedCost, (unsigned long long)prunedTriv,
           (unsigned long long)prunedDead, el,
           g_timedOut.load() ? "  (TIMEOUT)" : "");
    if (cands.empty()) { printf("RESULT: none\n"); return 2; }
    // 统一输出全部候选 —— Python 侧逐个做大 K + 随机重放验证
    printf("-- candidates: %d --\n", (int)cands.size());
    dumpCands();
    printf("RESULT: uops=%d lat=%d insns=%d cands=%d\n",
           bestUops, bestLat, (int)bestSeq.size(), (int)cands.size());
    return 0;
}
