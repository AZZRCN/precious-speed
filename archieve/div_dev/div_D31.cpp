// AZZRCN
// https://github.com/AZZRCN
//
// Division of Big Integers
// D31 = D27 + Knuth D3 qhat refinement + Granlund-Montgomery reciprocal
// LC: https://judge.yosupo.jp/problem/division_of_big_integers
#define HINT_OP_DIV

// LC 评测机 (judge.yosupo.jp/help 官方确认): GCP c2d-highcpu-8
//   AMD EPYC 7B13 = Zen3 "Milan", 限 1 核, 1 GiB 内存
//   AVX2+BMI2 可用; 无 AVX-512 (Zen3 不支持)
// #pragma GCC target 可用 (与无效的 #pragma GCC optimize 不同, target 控制指令集)
// NOTE: fma 已移除 — 实测 fma 导致 FFT 浮点精度变化, 触发罕见除法余数错误 (broad#134)
#pragma GCC target("avx2,bmi,bmi2,popcnt,lzcnt")

// cyclic (2NXN mod B^m-1) 路径: 运行时 use_cyclic 切换
// 当 cyclic_m >= in+len2 时循环卷积退化为线性 (conv_len_max=in+len2 <= cyclic_m),
//   fftMulModBm1Pre 精度比 fftMulPre 差, 导致 r-based 修正过度调整 qhat (burnikel RE).
// 修复: use_cyclic=(cyclic_m < in+len2), 退化时自动切换到 fftMulPre + 双向修正.
// 保留有意义的 cyclic (cyclic_m < in+len2, 如 1M/500k 的 22% FFT 提升).
//#define DISABLE_2NXN_CYCLIC

#ifndef HINT_MINI_HPP
#define HINT_MINI_HPP

#include <iostream>
#include <vector>
#include <string>
#include <complex>
#include <cstring>
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <climits>
#include <algorithm>
#include <immintrin.h>  // AVX2/SSE2 SIMD (emmintrin.h 子集, 含 AVX2 intrinsics)


// ==================== D27: Zen3 大页 Arena 分配器 (LC 特调) ====================
// LC 判题机 (judge.yosupo.jp/help): GCP c2d-highcpu-8 / AMD EPYC 7B13 (Zen3 Milan)
//   限 1 核, 1 GiB.  L1d 32KB/8way, L2 512KB/8way, L3 32MB/16way(victim)
//   L1 DTLB 64 项 -> 4KB 页仅覆盖 256KB;  L2 TLB 2048 项 -> 仅覆盖 8MB
//   DIV 峰值工作集 33MB => 连 L2 TLB 都装不下, 且 Zen3 硬件预取器不跨 4KB 页边界
// 对策: 所有堆分配收拢到一块 2MB 对齐 + MADV_HUGEPAGE 的 arena
//   实测缺页 amb_02 6490 / lri_00 4718 -> 预期 ~20
//   TLB 覆盖 8MB -> 128MB (L1 DTLB 64 x 2MB), 预取器可在 2MB 内连续工作
// 结构: bump 分配 + 2^k size-class freelist (vector 反复 grow 时复用, 防 arena 撑爆)
//   owns() 区分 arena/malloc 来源; 全套 new/delete 变体覆盖, 防止跨分配器混用
#if defined(__linux__) && !defined(HP_ARENA_OFF)
#include <sys/mman.h>
#include <new>
#include <cstdlib>
#include <cstdint>
#ifdef HP_STATS
#include <cstdio>
#endif

namespace hparena
{
    static constexpr size_t HP = size_t(2) << 20;             // 2MB 大页
    static constexpr size_t ARENA_SIZE = size_t(512) << 20;   // 虚拟保留 (MAP_NORESERVE, 不占 RSS)
    static constexpr unsigned KMIN = 6;                       // 最小块 64B
    static constexpr unsigned KMAX = 30;                      // 最大块 1GB

    static char *g_raw = nullptr;   // mmap 原始返回
    static char *g_base = nullptr;  // 2MB 对齐起点
    static char *g_cur = nullptr;
    static char *g_end = nullptr;
    static bool g_init = false;
    static void *g_free[KMAX + 2] = {};
#ifdef HP_STATS
    static size_t g_nalloc = 0, g_nhit = 0, g_nfall = 0;
#endif

    static void init()
    {
        g_init = true;
        void *m = mmap(nullptr, ARENA_SIZE + HP, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
        if (m == MAP_FAILED)
            return;
        g_raw = static_cast<char *>(m);
        uintptr_t a = (reinterpret_cast<uintptr_t>(m) + (HP - 1)) & ~static_cast<uintptr_t>(HP - 1);
        g_base = g_cur = reinterpret_cast<char *>(a);
        g_end = g_raw + ARENA_SIZE + HP;
        // 关键: 请求透明大页. GCP 默认 THP=madvise, 故必须显式 madvise 才生效.
        madvise(g_base, static_cast<size_t>(g_end - g_base) & ~(HP - 1), MADV_HUGEPAGE);
    }

    static inline bool owns(void *p) noexcept
    {
        return g_base && static_cast<char *>(p) >= g_base && static_cast<char *>(p) < g_end;
    }

    // 布局: [raw .. ret-8: 空洞][ret-8: uint32 offset][ret-4: uint32 k][ret .. ret+n]
    static void *take(size_t n, size_t align) noexcept
    {
        if (__builtin_expect(!g_init, 0))
            init();
        if (__builtin_expect(!g_base, 0))
            return nullptr;
        if (align < 64)
            align = 64;
        size_t need = n + align + 64;
        unsigned k = 64u - static_cast<unsigned>(__builtin_clzll(need | 63));
        if ((size_t(1) << k) < need)
            ++k;
        if (k < KMIN)
            k = KMIN;
        if (k > KMAX)
            return nullptr;

        char *raw;
        if (g_free[k])
        {
            raw = static_cast<char *>(g_free[k]);
            g_free[k] = *reinterpret_cast<void **>(raw);
#ifdef HP_STATS
            ++g_nhit;
#endif
        }
        else
        {
            size_t blk = size_t(1) << k;
            char *p = reinterpret_cast<char *>(
                (reinterpret_cast<uintptr_t>(g_cur) + 63) & ~static_cast<uintptr_t>(63));
            if (__builtin_expect(p + blk > g_end, 0))
                return nullptr;
            g_cur = p + blk;
            raw = p;
        }
#ifdef HP_STATS
        ++g_nalloc;
#endif
        char *ret = reinterpret_cast<char *>(
            (reinterpret_cast<uintptr_t>(raw) + align + 8 + (align - 1)) & ~static_cast<uintptr_t>(align - 1));
        reinterpret_cast<uint32_t *>(ret)[-1] = k;
        reinterpret_cast<uint32_t *>(ret)[-2] = static_cast<uint32_t>(ret - raw);
        return ret;
    }

    static inline void give(void *p) noexcept
    {
        uint32_t k = reinterpret_cast<uint32_t *>(p)[-1];
        uint32_t off = reinterpret_cast<uint32_t *>(p)[-2];
        if (k < KMIN || k > KMAX)
            return;  // 防御: header 损坏则泄漏而非崩溃
        void *raw = static_cast<char *>(p) - off;
        *reinterpret_cast<void **>(raw) = g_free[k];
        g_free[k] = raw;
    }

    static inline void *fallback(size_t n) noexcept
    {
#ifdef HP_STATS
        ++g_nfall;
#endif
        return std::malloc(n ? n : 1);
    }

#ifdef HP_STATS
    struct Stats
    {
        ~Stats()
        {
            std::fprintf(stderr,
                         "[hparena] alloc=%zu  freelist_hit=%zu  malloc_fallback=%zu  bump_peak=%.2f MB\n",
                         g_nalloc, g_nhit, g_nfall,
                         g_base ? double(g_cur - g_base) / 1048576.0 : 0.0);
        }
    };
    static Stats g_stats;
#endif
} // namespace hparena

void *operator new(size_t n)
{
    void *p = hparena::take(n, 64);
    if (__builtin_expect(p != nullptr, 1))
        return p;
    p = hparena::fallback(n);
    if (!p)
        throw std::bad_alloc();
    return p;
}
void *operator new[](size_t n) { return ::operator new(n); }
void *operator new(size_t n, const std::nothrow_t &) noexcept
{
    void *p = hparena::take(n, 64);
    return p ? p : hparena::fallback(n);
}
void *operator new[](size_t n, const std::nothrow_t &) noexcept
{
    return ::operator new(n, std::nothrow);
}
void *operator new(size_t n, std::align_val_t a)
{
    void *p = hparena::take(n, static_cast<size_t>(a));
    if (__builtin_expect(p != nullptr, 1))
        return p;
    p = std::aligned_alloc(static_cast<size_t>(a),
                           (n + static_cast<size_t>(a) - 1) & ~(static_cast<size_t>(a) - 1));
    if (!p)
        throw std::bad_alloc();
    return p;
}
void *operator new[](size_t n, std::align_val_t a) { return ::operator new(n, a); }

void operator delete(void *p) noexcept
{
    if (!p)
        return;
    if (__builtin_expect(hparena::owns(p), 1))
        hparena::give(p);
    else
        std::free(p);
}
void operator delete[](void *p) noexcept { ::operator delete(p); }
void operator delete(void *p, size_t) noexcept { ::operator delete(p); }
void operator delete[](void *p, size_t) noexcept { ::operator delete(p); }
void operator delete(void *p, std::align_val_t) noexcept { ::operator delete(p); }
void operator delete[](void *p, std::align_val_t) noexcept { ::operator delete(p); }
void operator delete(void *p, size_t, std::align_val_t) noexcept { ::operator delete(p); }
void operator delete[](void *p, size_t, std::align_val_t) noexcept { ::operator delete(p); }
void operator delete(void *p, const std::nothrow_t &) noexcept { ::operator delete(p); }
void operator delete[](void *p, const std::nothrow_t &) noexcept { ::operator delete(p); }
#endif // __linux__ && !HP_ARENA_OFF
// ================== end D27 arena ==================

#include <chrono>
#include <cstdio>  // for debug fprintf to stderr (LC RE shows stderr)
#ifdef FFTHIST
#include <map>
#include <cmath>
#include <cstdlib>
#endif
#ifdef CEILHIST
// 诊断用: 统计 "卷积需求长度 need -> 实际 FFT 档位 used" 的浪费分布。
// 只在真实执行点打点(决策/代价模型走 _q 静默版), 权重取 N*log2(N)。
#include <map>
#include <vector>
#include <string>
#include <cmath>
#include <cstdlib>
#include <tuple>
#include <algorithm>
inline std::map<std::tuple<int, size_t, size_t>, size_t> &ceilHist()
{
    static std::map<std::tuple<int, size_t, size_t>, size_t> h;
    static bool reg = (std::atexit([]
    {
        static const char *TAG[3] = {"lin", "mn", "cycm"};
        double used_w = 0, need_w = 0;
        std::vector<std::pair<double, std::string>> rows;
        for (auto &kv : ceilHist())
        {
            size_t need = std::get<1>(kv.first), used = std::get<2>(kv.first);
            if (need < 2) continue;
            double wu = double(used) * std::log2(double(used)) * double(kv.second);
            double wn = double(need) * std::log2(double(need)) * double(kv.second);
            used_w += wu; need_w += wn;
            char buf[256];
            snprintf(buf, sizeof buf,
                     "  %-4s need=%-9zu used=%-9zu x%-6zu waste=%5.1f%%  dNlogN=%.4g",
                     TAG[std::get<0>(kv.first)], need, used, kv.second,
                     100.0 * (double(used) / double(need) - 1.0), wu - wn);
            rows.emplace_back(wu - wn, buf);
        }
        std::sort(rows.begin(), rows.end(),
                  [](const auto &a, const auto &b) { return a.first > b.first; });
        fprintf(stderr, "==== fft_ceil need->used (top 20 by wasted N*logN) ====\n");
        for (size_t i = 0; i < rows.size() && i < 20; i++)
            fprintf(stderr, "%s\n", rows[i].second.c_str());
        fprintf(stderr, "  TOTAL used=%.6g ideal=%.6g overhead=%.2f%%\n",
                used_w, need_w, need_w > 0 ? 100.0 * (used_w / need_w - 1.0) : 0.0);
        // 机器可读全量导出, 供离线档位模拟
        if (const char *fn = getenv("CEILHIST_CSV"))
        {
            if (FILE *f = fopen(fn, "w"))
            {
                fprintf(f, "tag,need,used,cnt\n");
                for (auto &kv : ceilHist())
                    fprintf(f, "%d,%zu,%zu,%zu\n", std::get<0>(kv.first),
                            std::get<1>(kv.first), std::get<2>(kv.first), kv.second);
                fclose(f);
            }
        }
    }), true);
    (void)reg;
    return h;
}
#define CEILHIST_TICK(tag, need, used) (ceilHist()[std::make_tuple((tag), (need), (used))]++)
#else
#define CEILHIST_TICK(tag, need, used) ((void)0)
#endif
#ifdef TOPPROF
// 顶层四段计时: 解析(十进制->limb) / 除法 / 输出格式化(limb->十进制) / 读写 IO
#include <chrono>
struct TopProfT
{
    double rd = 0, parse = 0, div = 0, print = 0, wr = 0;
    ~TopProfT()
    {
        double t = rd + parse + div + print + wr;
        fprintf(stderr,
                "[topprof] read=%.2f parse=%.2f div=%.2f fmt=%.2f write=%.2f  total=%.2f ms\n"
                "[topprof]   %%  read=%.1f parse=%.1f div=%.1f fmt=%.1f write=%.1f\n",
                rd, parse, div, print, wr, t,
                t > 0 ? 100 * rd / t : 0, t > 0 ? 100 * parse / t : 0,
                t > 0 ? 100 * div / t : 0, t > 0 ? 100 * print / t : 0,
                t > 0 ? 100 * wr / t : 0);
    }
};
inline TopProfT &topprof() { static TopProfT t; return t; }
#define TP_DECL(v) auto _tp_##v = std::chrono::high_resolution_clock::now()
#define TP_ADD(v) (topprof().v += std::chrono::duration<double, std::milli>(  \
                       std::chrono::high_resolution_clock::now() - _tp_##v)   \
                       .count())
#else
#define TP_DECL(v) ((void)0)
#define TP_ADD(v) ((void)0)
#endif
#ifdef INVPROF
// 诊断用: absInvNewtonGMP 逐层自耗时 (self = 本层总耗时 - 子层耗时)。
// 大用例走 GMP 路径, 原 PROFILE_DIV 完全没插桩 -> 倒数是黑盒。
// 这里按 (k, cyclic, mn) 聚合, 输出每层 self ms / 次数 / 占比, 定位倒数内部的真实热点。
#include <map>
#include <vector>
#include <tuple>
#include <cstdlib>
#include <algorithm>
struct InvProfKey
{
    size_t k, s, rn, mn;
    int cyc;
    bool operator<(const InvProfKey &o) const
    {
        return std::tie(k, s, rn, mn, cyc) < std::tie(o.k, o.s, o.rn, o.mn, o.cyc);
    }
};
inline std::map<InvProfKey, std::pair<double, size_t>> &invProfRecs()
{
    static std::map<InvProfKey, std::pair<double, size_t>> m;
    static bool reg = (std::atexit([]
    {
        auto &mm = invProfRecs();
        double tot = 0;
        for (auto &kv : mm) tot += kv.second.first;
        std::fprintf(stderr, "\n[invprof] total self = %.3f ms over %zu distinct levels\n",
                     tot, mm.size());
        std::fprintf(stderr, "[invprof] %10s %10s %10s %10s %5s %12s %8s %7s\n",
                     "k", "s", "rn", "mn", "cyc", "self_ms", "n", "pct");
        std::vector<std::pair<double, InvProfKey>> rows;
        for (auto &kv : mm) rows.push_back({kv.second.first, kv.first});
        std::sort(rows.begin(), rows.end(),
                  [](const auto &a, const auto &b) { return a.first > b.first; });
        size_t shown = 0;
        for (auto &r : rows)
        {
            if (shown++ >= 24) break;
            auto &v = mm[r.second];
            std::fprintf(stderr, "[invprof] %10zu %10zu %10zu %10zu %5d %12.3f %8zu %6.1f%%\n",
                         r.second.k, r.second.s, r.second.rn, r.second.mn, r.second.cyc,
                         v.first, v.second, tot > 0 ? v.first / tot * 100 : 0);
        }
    }), true);
    (void)reg;
    return m;
}
inline double &invProfChild()
{
    static thread_local double c = 0;
    return c;
}
struct InvProfScope
{
    InvProfKey key{};
    std::chrono::high_resolution_clock::time_point t0;
    double saved_child;
    explicit InvProfScope(size_t kk)
        : t0(std::chrono::high_resolution_clock::now())
    {
        key.k = kk;
        key.cyc = -1;
        saved_child = invProfChild();
        invProfChild() = 0.0;
    }
    ~InvProfScope()
    {
        double tot = std::chrono::duration<double, std::milli>(
                         std::chrono::high_resolution_clock::now() - t0)
                         .count();
        double self = tot - invProfChild();
        invProfChild() = saved_child + tot;
        auto &e = invProfRecs()[key];
        e.first += self;
        e.second += 1;
    }
};
#define INVPROF_SCOPE(kk) InvProfScope _ips(kk)
#define INVPROF_SET(ss, rr, mm, cc) \
    do { _ips.key.s = (ss); _ips.key.rn = (rr); _ips.key.mn = (mm); _ips.key.cyc = (cc); } while (0)
// 细粒度相位: 只对大 k 打印, 定位 cyclic 路径内部哪一步爆炸
#define INVPH_DECL(v) auto _iph_##v = std::chrono::high_resolution_clock::now()
#define INVPH_END(v, kk, tag)                                                       \
    do {                                                                            \
        if ((kk) >= 40000)                                                          \
            std::fprintf(stderr, "[invph] k=%zu %-10s %8.3f ms\n", (size_t)(kk), tag, \
                         std::chrono::duration<double, std::milli>(                 \
                             std::chrono::high_resolution_clock::now() - _iph_##v)  \
                             .count());                                             \
    } while (0)
#else
#define INVPROF_SCOPE(kk) ((void)0)
#define INVPROF_SET(ss, rr, mm, cc) ((void)0)
#define INVPH_DECL(v) ((void)0)
#define INVPH_END(v, kk, tag) ((void)0)
#endif
#ifdef PROFILE_DIV
static FILE *_prof_fp = nullptr;
static inline void _prof_init() { if (!_prof_fp) _prof_fp = std::fopen("prof_detail.log", "w"); }
static inline void _prof_flush() { if (_prof_fp) { std::fflush(_prof_fp); } }
#define PROF_PRINT(...) do { _prof_init(); if (_prof_fp) std::fprintf(_prof_fp, __VA_ARGS__); } while(0)
#else
#define PROF_PRINT(...) ((void)0)
#endif

namespace hint
{
    using Float32 = float;
    using Float64 = double;
    using Complex32 = std::complex<Float32>;
    using Complex64 = std::complex<Float64>;

    constexpr Float64 HINT_PI = 3.141592653589793238462643;
    constexpr Float64 HINT_2PI = HINT_PI * 2;
    constexpr Float64 COS_PI_8 = 0.707106781186547524400844;

    constexpr size_t FFT_MAX_LEN = size_t(1) << 23;

    // Builtin helper macros — use long-named GCC builtins for optimization
    // HINT_ASSUME: hint compiler that cond is always true (via __builtin_unreachable),
    //   enables dead-code elimination, bounds-check removal, power-of-2 strength reduction
    // HINT_PREFETCH: __builtin_prefetch wrapper for explicit cache control
    // HINT_PROB_LIKELY / HINT_PROB_UNLIKELY: __builtin_expect_with_probability for
    //   precise branch prediction with known probability (GCC 9+)
#ifndef HINT_ASSUME
#define HINT_ASSUME(cond) do { if (!(cond)) __builtin_unreachable(); } while(0)
#endif
#ifndef HINT_PREFETCH
#define HINT_PREFETCH(addr, rw, locality) __builtin_prefetch((addr), (rw), (locality))
#endif
#ifndef HINT_PROB_LIKELY
#define HINT_PROB_LIKELY(x, p)   __builtin_expect_with_probability(!!(x), 1, (p))
#define HINT_PROB_UNLIKELY(x, p) __builtin_expect_with_probability(!!(x), 0, (p))
#endif
// ---- D17: FFTW-style codelets ------------------------------------------
// split-radix recursion T(n)=1+T(n/2)+2T(n/4) with base float_len<=8 produces
// ~n/6 recursive nodes; a single 32768-point transform costs ~5461 calls, each
// paying ~135 Ir of call/prologue overhead for only 6 butterflies of real work.
// Compile-time unrolling of every subtree with LEN<=FFT_FIXED_MAX collapses the
// call count by ~FFT_FIXED_MAX/8 and turns the leaves into straight-line SIMD.
#ifndef FFT_FIXED_MAX
#define FFT_FIXED_MAX 32
#endif
// Forcing the codelet tree inline is a win, but forcing the 4/8-point leaves or
// the runtime->compile-time dispatcher inline costs more in register pressure
// than it saves in calls (measured on Zen3 cache model), so they stay heuristic.
#define HINT_AI __attribute__((always_inline)) inline
#ifndef FFT_AI_SMALL
#define FFT_AI_SMALL 0
#endif
#ifndef FFT_AI_DISPATCH
#define FFT_AI_DISPATCH 0
#endif
#if FFT_AI_SMALL
#define HINT_AI_SMALL HINT_AI
#else
#define HINT_AI_SMALL
#endif
#if FFT_AI_DISPATCH
#define HINT_AI_DISPATCH HINT_AI
#else
#define HINT_AI_DISPATCH
#endif
#ifndef HINT_LIKELY
#define HINT_LIKELY(x)   __builtin_expect(!!(x), 1)
#define HINT_UNLIKELY(x) __builtin_expect(!!(x), 0)
#endif

    // 32-byte aligned allocator for AVX2 FFT buffers
    // posix_memalign guarantees 32-byte alignment, enabling _mm256_load_pd/_mm256_store_pd
    // and __builtin_assume_aligned hints to the compiler
    template <typename T>
    struct AlignedAlloc32
    {
        using value_type = T;
        using pointer = T *;
        using const_pointer = const T *;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        AlignedAlloc32() = default;
        template <typename U>
        AlignedAlloc32(const AlignedAlloc32<U> &) {}
        T *allocate(size_t n)
        {
            void *p = nullptr;
#ifdef _WIN32
            p = _aligned_malloc(n * sizeof(T), 32);
            if (!p) throw std::bad_alloc();
#else
#if defined(__linux__) && !defined(HP_ARENA_OFF)
            // D27: 走大页 arena (2MB 页 + freelist 复用, 消除 mmap/munmap syscall)
            p = ::hparena::take(n * sizeof(T), 32);
            if (p)
                return static_cast<T *>(p);
#endif
            if (posix_memalign(&p, 32, n * sizeof(T)) != 0)
                throw std::bad_alloc();
#endif
            return static_cast<T *>(p);
        }
        void deallocate(T *p, size_t)
        {
#ifdef _WIN32
            _aligned_free(p);
#else
#if defined(__linux__) && !defined(HP_ARENA_OFF)
            if (::hparena::owns(p))
            {
                ::hparena::give(p);
                return;
            }
#endif
            free(p);
#endif
        }
        template <typename U>
        struct rebind { using other = AlignedAlloc32<U>; };
        bool operator==(const AlignedAlloc32 &) const { return true; }
        bool operator!=(const AlignedAlloc32 &) const { return false; }
    };
    template <typename T>
    using AlignedVec32 = std::vector<T, AlignedAlloc32<T>>;


    template <typename T>
    constexpr T int_ceil2(T n)
    {
        if (n <= 1)
            return 1;
        // __builtin_clzll: O(1) vs O(log bits) loop, one BSR instruction on x86
        if constexpr (sizeof(T) <= 4)
            return T(2) << (31 - __builtin_clz(uint32_t(n - 1)));
        else
            return T(2) << (63 - __builtin_clzll(uint64_t(n - 1)));
    }

    template <typename IntTy>
    constexpr bool is_2pow(IntTy n)
    {
        return n != 0 && (n & (n - 1)) == 0;
    }

    // ---- 非 2 幂 FFT 档位 (radix-3 顶层) -------------------------------------
    // FFT 长度只允许 2^k 时, "刚过一个 2 幂" 的卷积白付近 1.5 倍代价(最坏浪费 100%).
    // 增加 3*2^k 档位后台阶细化为 {.., 2^k, 1.5*2^k, 2^{k+1}, ..}, 最坏浪费降到 50%.
    // FFT3_MIN: radix-3 路径要求每块 M = float_len/6 >= 32, 即 float_len >= 192.
    constexpr size_t FFT3_MIN = 192;
    // 返回 >= n 的最小可用 FFT 长度 (2^k 或 3*2^k)
    constexpr size_t fft_ceil(size_t n)
    {
        size_t p = int_ceil2(n);
#ifdef NO_FFT3
        return p; // A/B 开关: 退回纯 2 幂档位
#else
        size_t h = p / 4 * 3; // = 3*2^(k-2), 落在 (p/2, p)
        if (h >= FFT3_MIN && h >= n)
            return h;
        return p;
#endif
    }
    // float_len 是否走 radix-3 路径 (合法长度只有 2^k 与 3*2^k 两类)
    constexpr bool is_fft3(size_t float_len)
    {
        return (float_len % 3) == 0;
    }

    // ---- bisect 开关: 按"语义类别"独立回退到纯 2 幂档位 -----------------------
    // 三类长度的风险面完全不同, 必须能分别关掉才能定位回归:
    //   lin  : 纯线性卷积长度 —— 只要 >= conv_len 即正确, 风险最低
    //   mn   : absInvNewtonGMP 的 cyclic 模数 B^mn-1 —— 改变 GMP 修正的整数约束
    //   cycm : absDivMu 的 cyclic 模数 —— 改变 unwrap 近似的 wrap 次数与精度
    constexpr size_t fft_ceil_lin_q(size_t n)
    {
#ifdef BISECT_LIN_2POW
        return int_ceil2(n);
#else
        return fft_ceil(n);
#endif
    }
    constexpr size_t fft_ceil_mn_q(size_t n)
    {
#ifdef BISECT_MN_2POW
        return int_ceil2(n);
#else
        return fft_ceil(n);
#endif
    }
    constexpr size_t fft_ceil_cycm_q(size_t n)
    {
#ifdef BISECT_CYCM_2POW
        return int_ceil2(n);
#else
        return fft_ceil(n);
#endif
    }
    // 真实执行点用的版本 (CEILHIST 下会打点; 正式构建与 _q 完全等价, 零开销)
#ifdef CEILHIST
    inline size_t fft_ceil_lin(size_t n)
    { size_t r = fft_ceil_lin_q(n); CEILHIST_TICK(0, n, r); return r; }
    inline size_t fft_ceil_mn(size_t n)
    { size_t r = fft_ceil_mn_q(n); CEILHIST_TICK(1, n, r); return r; }
    inline size_t fft_ceil_cycm(size_t n)
    { size_t r = fft_ceil_cycm_q(n); CEILHIST_TICK(2, n, r); return r; }
#else
    constexpr size_t fft_ceil_lin(size_t n) { return fft_ceil_lin_q(n); }
    constexpr size_t fft_ceil_mn(size_t n) { return fft_ceil_mn_q(n); }
    constexpr size_t fft_ceil_cycm(size_t n) { return fft_ceil_cycm_q(n); }
#endif

    // ---- mu_div_qr 块划分代价模型 -------------------------------------------
    // D13 引入 (只用于 qn_mu>len2 分支), D15 修正闸门口径, D16 抽出并推广到全分支。
    //
    // cost(inc) = A*W(Fi) + W(Fi) + W(Fc) + nblk*(2W(Fi) + 2W(Fc)),  W(N)=N*log2 N
    //   A≈6.8  : Newton 倒数相对"顶层一次变换"的倍数 (INVPROF 逐层实测 6.8~7.0)
    //   Fi     : fft_ceil(2*inc+1) —— 同时驱动 (a) 整个倒数 (b) 每块第一次乘法
    //   Fc     : cyclic 模数 (开得了 cyclic 时约 Fl/2), 否则退回线性 Fl
    //   关键杠杆: inc 稍大就把 Fi 顶上一档, 而 Fi 有双重作用 => 多切一块常常更划算。
    //
    // ★ 闸门口径必须与 absDivMu 执行处 (use_cyclic) 逐字一致, 否则模型会把
    //   "实际能吃循环卷积"的 inc 误判成线性、成本虚高, 从而死守次优块数。
    inline double muBlockCost(size_t qn_mu, size_t len2, size_t inc)
    {
        if (inc > len2 || inc < 64) return 1e300;
        // 档位长度恒为 2^k 或 3*2^k, 直接算 log2 避免引入 <cmath>
        auto Wf = [](size_t n) -> double {
            if (n < 2) return 0.0;
            size_t p = n, e = 0;
            while (p > 1 && (p & 1) == 0) { p >>= 1; e++; }
            return (double)n * ((double)e + (p == 3 ? 1.5849625007211562 : 0.0));
        };
        const size_t nblk = (qn_mu + inc - 1) / inc;
        const size_t Fi = fft_ceil_lin_q(2 * inc + 1);
        const size_t Fl = fft_ceil_lin_q(len2 + inc);
        const size_t Fc_cand = std::max(fft_ceil_cycm_q(len2 + 1),
                                        fft_ceil_cycm_q((len2 + inc) / 2 + 1));
#ifdef GATE_POW2
        const size_t gate = std::max(int_ceil2(len2 + 1),
                                     int_ceil2((len2 + inc) / 2 + 1));
#else
        const size_t gate = Fc_cand;
#endif
        const bool cyc = (gate < inc + len2);  // 调用点已保证 inc>=64 => allow_cyclic
        const size_t Fc = cyc ? Fc_cand : Fl;
        const double wi = Wf(Fi), wc = Wf(Fc);
        return 6.8 * wi + wi + wc + (double)nblk * (2 * wi + 2 * wc);
    }

#ifdef DIV_INVDUMP
// 对拍探针: dump absInvNewtonGMP 每层的输出 inv, 用于 cyclic 开/关两次运行 diff.
#define DIV_INVDUMP_EMIT(tag, kk, invsp)                                                    \
    do {                                                                                    \
        size_t k_ = (size_t)(kk);                                                           \
        if (k_ >= 4096) {                                                                   \
            unsigned long long h_ = 1469598103934665603ULL;                                 \
            for (size_t j_ = 0; j_ <= k_; j_++) {                                           \
                h_ ^= (unsigned long long)(invsp).ptr[j_];                                  \
                h_ *= 1099511628211ULL;                                                     \
            }                                                                               \
            fprintf(stderr,                                                                 \
                    "[invdump] %s k=%zu hash=%016llx int=%u hi=%u,%u,%u lo=%u,%u,%u\n",     \
                    tag, k_, h_, (unsigned)(invsp).ptr[k_],                                 \
                    (unsigned)(invsp).ptr[k_ - 1], (unsigned)(invsp).ptr[k_ - 2],           \
                    (unsigned)(invsp).ptr[k_ - 3], (unsigned)(invsp).ptr[2],                \
                    (unsigned)(invsp).ptr[1], (unsigned)(invsp).ptr[0]);                    \
        }                                                                                   \
    } while (0)
#endif

    template <typename T>
    constexpr int hint_log2(T n)
    {
        if (n <= 0)
            return -1;
        // __builtin_clz: O(1) vs O(log bits) binary search, one LZCNT/BSR instruction
        if constexpr (sizeof(T) <= 4)
            return 31 - __builtin_clz(uint32_t(n));
        else
            return 63 - __builtin_clzll(uint64_t(n));
    }
    constexpr int hint_ctz(uint32_t x)
    {
        // __builtin_ctz: O(1) TZCNT/BSF instruction (BMI1 enabled via pragma target)
        if (x == 0)
            return 32;
        return __builtin_ctz(x);
    }

    constexpr int hint_ctz(uint64_t x)
    {
        // __builtin_ctzll: O(1) TZCNT/BSF instruction (BMI1 enabled via pragma target)
        if (x == 0)
            return 64;
        return __builtin_ctzll(x);
    }


    constexpr int hint_clz(uint32_t x)
    {
        // __builtin_clz: O(1) LZCNT/BSR instruction (LZCNT enabled via pragma target)
        if (x == 0)
            return 32;
        return __builtin_clz(x);
    }

    constexpr int hint_clz(uint64_t x)
    {
        // __builtin_clzll: O(1) LZCNT/BSR instruction (LZCNT enabled via pragma target)
        if (x == 0)
            return 64;
        return __builtin_clzll(x);
    }

    
    template <typename IntTy>
    constexpr int hint_bit_length(IntTy x)
    {
        if (0 == x)
        {
            return 0;
        }
        return sizeof(IntTy) * CHAR_BIT - hint_clz(x);
    }

    
    template <typename T, typename T1>
    constexpr T qpow(T m, T1 n)
    {
        T result = 1;
        while (true)
        {
            if (n & 1)
            {
                result *= m;
            }
            if (0 == n)
            {
                break;
            }
            m *= m;
            n >>= 1;
        }
        return result;
    }

    constexpr int hint_popcnt(uint32_t n)
    {
        constexpr uint32_t mask55 = 0x55555555;
        constexpr uint32_t mask33 = 0x33333333;
        constexpr uint32_t mask0f = 0x0f0f0f0f;
        constexpr uint32_t maskff = 0x00ff00ff;
        n = (n & mask55) + ((n >> 1) & mask55);
        n = (n & mask33) + ((n >> 2) & mask33);
        n = (n & mask0f) + ((n >> 4) & mask0f);
        n = (n & maskff) + ((n >> 8) & maskff);
        return uint16_t(n) + (n >> 16);
    }
    constexpr int hint_popcnt(uint64_t n)
    {
        constexpr uint64_t mask5555 = 0x5555555555555555;
        constexpr uint64_t mask3333 = 0x3333333333333333;
        constexpr uint64_t mask0f0f = 0x0f0f0f0f0f0f0f0f;
        constexpr uint64_t mask00ff = 0x00ff00ff00ff00ff;
        constexpr uint64_t maskffff = 0x0000ffff0000ffff;
        n = (n & mask5555) + ((n >> 1) & mask5555);
        n = (n & mask3333) + ((n >> 2) & mask3333);
        n = (n & mask0f0f) + ((n >> 4) & mask0f0f);
        n = (n & mask00ff) + ((n >> 8) & mask00ff);
        n = (n & maskffff) + ((n >> 16) & maskffff);
        return uint32_t(n) + (n >> 32);
    }

    constexpr uint32_t bitrev32(uint32_t n)
    {
        constexpr uint32_t mask55 = 0x55555555;
        constexpr uint32_t mask33 = 0x33333333;
        constexpr uint32_t mask0f = 0x0f0f0f0f;
        constexpr uint32_t maskff = 0x00ff00ff;
        n = ((n & mask55) << 1) | ((n >> 1) & mask55);
        n = ((n & mask33) << 2) | ((n >> 2) & mask33);
        n = ((n & mask0f) << 4) | ((n >> 4) & mask0f);
        n = ((n & maskff) << 8) | ((n >> 8) & maskff);
        return (n << 16) | (n >> 16);
    }
    constexpr uint32_t bitrev(uint32_t n, int len)
    {
        assert(len <= 32);
        return bitrev32(n) >> (32 - len);
    }


    template <typename Float>
    struct Float2
    {
        Float x0, x1;
        using F2 = Float2;
        Float2() = default;
        constexpr Float2(Float x0, Float x1) : x0(x0), x1(x1) {}

        constexpr F2 &operator+=(const F2 &rhs)
        {
            x0 += rhs.x0;
            x1 += rhs.x1;
            return *this;
        }
        constexpr F2 &operator-=(const F2 &rhs)
        {
            x0 -= rhs.x0;
            x1 -= rhs.x1;
            return *this;
        }
        constexpr F2 &operator*=(const F2 &rhs)
        {
            x0 *= rhs.x0;
            x1 *= rhs.x1;
            return *this;
        }
        friend constexpr F2 operator+(const F2 &lhs, const F2 &rhs)
        {
            return F2(lhs.x0 + rhs.x0, lhs.x1 + rhs.x1);
        }
        friend constexpr F2 operator-(const F2 &lhs, const F2 &rhs)
        {
            return F2(lhs.x0 - rhs.x0, lhs.x1 - rhs.x1);
        }
        friend constexpr F2 operator*(const F2 &lhs, const F2 &rhs)
        {
            return F2(lhs.x0 * rhs.x0, lhs.x1 * rhs.x1);
        }
        friend constexpr F2 operator*(const F2 &lhs, const Float &rhs)
        {
            return F2(lhs.x0 * rhs, lhs.x1 * rhs);
        }
        constexpr F2 reverse() const
        {
            return F2(x1, x0);
        }
        constexpr void set1(Float x)
        {
            x0 = x1 = x;
        }
        static constexpr F2 from1(Float x)
        {
            return F2(x, x);
        }
        static constexpr F2 fromMem(const Float *p)
        {
            return F2(p[0], p[1]);
        }
        void store(Float *p) const
        {
            p[0] = x0;
            p[1] = x1;
        }
    };

    template <typename Float>
    struct Complex2
    {
        using F2 = Float2<Float>;
        using C2 = Complex2;
        F2 real, imag;
        Complex2() = default;
        constexpr Complex2(F2 r, F2 i) : real(r), imag(i) {}
        constexpr Complex2(Float r, Float i) : real(F2::from1(r)), imag(F2::from1(i)) {}
        constexpr Complex2(Float x0, Float x1, Float x2, Float x3) : real(x0, x1), imag(x2, x3) {}

        constexpr C2 &operator+=(const C2 &rhs)
        {
            real += rhs.real;
            imag += rhs.imag;
            return *this;
        }
        constexpr C2 &operator-=(const C2 &rhs)
        {
            real -= rhs.real;
            imag -= rhs.imag;
            return *this;
        }
        constexpr C2 &operator*=(const C2 &rhs)
        {
            F2 r = real * rhs.real - imag * rhs.imag;
            F2 i = real * rhs.imag + imag * rhs.real;
            return *this = C2(r, i);
        }
        friend constexpr C2 operator+(const C2 &lhs, const C2 &rhs)
        {
            return C2(lhs.real + rhs.real, lhs.imag + rhs.imag);
        }
        friend constexpr C2 operator-(const C2 &lhs, const C2 &rhs)
        {
            return C2(lhs.real - rhs.real, lhs.imag - rhs.imag);
        }
        friend constexpr C2 operator*(const C2 &lhs, const F2 &rhs)
        {
            return C2(lhs.real * rhs, lhs.imag * rhs);
        }
        friend constexpr C2 operator*(const C2 &lhs, const Float &rhs)
        {
            return C2(lhs.real * rhs, lhs.imag * rhs);
        }
        constexpr C2 mul(const C2 &other) const
        {
            const F2 ii = imag * other.imag;
            const F2 ri = real * other.imag;
            const F2 r = real * other.real - ii;
            const F2 i = imag * other.real + ri;
            return C2(r, i);
        }
        constexpr C2 mulConj(const C2 &other) const
        {
            const F2 ii = imag * other.imag;
            const F2 ri = real * other.imag;
            const F2 r = real * other.real + ii;
            const F2 i = imag * other.real - ri;
            return C2(r, i);
        }
        constexpr C2 reverse() const
        {
            return C2(real.reverse(), imag.reverse());
        }
        constexpr void permute()
        {
            std::swap(real.x1, imag.x0);
        }
        void load(const Float *p)
        {
            real = F2::fromMem(p);
            imag = F2::fromMem(p + 2);
        }
        void store(Float *p) const
        {
            real.store(p);
            imag.store(p + 2);
        }
        void print() const
        {
            std::cout << '(' << real.x0 << ',' << imag.x0 << ") "
                      << '(' << real.x1 << ',' << imag.x1 << ")\n";
        }
    };

    
    namespace transform
    {

        template <typename T>
        inline void transform2(T &sum, T &diff)
        {
            T temp0 = sum, temp1 = diff;
            sum = temp0 + temp1;
            diff = temp0 - temp1;
        }

        template <typename T>
        inline void transform2(const T a, const T b, T &sum, T &diff)
        {
            sum = a + b;
            diff = a - b;
        }
        namespace fft
        {
            constexpr size_t FFT_MAX_LEN = size_t(1) << 23;

            template <typename Float>
            inline std::complex<Float> getOmega(size_t n, size_t index, Float factor = 1)
            {
                Float theta = -HINT_2PI * index / n;
                return std::polar<Float>(1, theta * factor);
            }
            template <typename Float>
            inline void difSplit(Float &r0, Float &i0, Float &r1, Float &i1, Float &r2, Float &i2, Float &r3, Float &i3)
            {
                transform2(r0, r2);
                transform2(i0, i2);
                transform2(r1, r3);
                transform2(i1, i3);

                transform2(r2, i3);
                transform2(i2, r3, r3, i2);
                std::swap(i3, r3);
            }
            template <typename Float>
            inline void iditSplit(Float &r0, Float &i0, Float &r1, Float &i1, Float &r2, Float &i2, Float &r3, Float &i3)
            {
                transform2(r2, r3);
                transform2(i2, i3);

                transform2(r0, r2);
                transform2(i0, i2);
                transform2(r1, i3, i3, r1);
                transform2(i1, r3);
                std::swap(i3, r3);
            }
            // ================= D21: 4-complex SoA (C4) butterfly layout =================
            // C2 布局 {r0,r1,i0,i1} 里 real/imag 各占半个 ymm, 于是
            //   (a) 复数乘的 real*o.real 只有 128-bit 有效宽度
            //   (b) difSplit 尾部 transform2(r2,i3) / swap(i3,r3) 跨 real/imag 分区
            // 两者都逼 GCC 发 vpermpd/vblendpd。C4 把 4 个复数的 re/im 各放一整个 ymm,
            // 上述两处全部变成同 lane 全宽运算, shuffle 归零。
#ifndef FFT_C4_MIN
#define FFT_C4_MIN 32
#endif
            struct C4d
            {
                __m256d re, im;
            };
            HINT_AI C4d c4load(const double *p)
            {
                return C4d{_mm256_load_pd(p), _mm256_load_pd(p + 4)};
            }
            HINT_AI void c4store(double *p, const C4d &c)
            {
                _mm256_store_pd(p, c.re);
                _mm256_store_pd(p + 4, c.im);
            }
            // 用 __m256d 运算符而非 fma intrinsic: 顶层 pragma target 未开 fma,
            // 交给 GCC 自行融合 (实测仍产出 vfmadd/vfnmadd)。
            HINT_AI C4d c4mul(const C4d &a, const C4d &b)
            {
                return C4d{a.re * b.re - a.im * b.im, a.im * b.re + a.re * b.im};
            }
            HINT_AI C4d c4mulConj(const C4d &a, const C4d &b)
            {
                return C4d{a.re * b.re + a.im * b.im, a.im * b.re - a.re * b.im};
            }
            // difSplit 的 C4 版。原语义 (A=c0-c2, B=c1-c3):
            //   c0 += c2 ; c1 += c3
            //   c2 = (A.re + B.im, A.im - B.re) ; c3 = (A.re - B.im, A.im + B.re)
            HINT_AI void difSplitC4(C4d &c0, C4d &c1, C4d &c2, C4d &c3)
            {
                const __m256d ar = c0.re - c2.re, ai = c0.im - c2.im;
                const __m256d br = c1.re - c3.re, bi = c1.im - c3.im;
                c0.re = c0.re + c2.re;
                c0.im = c0.im + c2.im;
                c1.re = c1.re + c3.re;
                c1.im = c1.im + c3.im;
                c2.re = ar + bi;
                c2.im = ai - br;
                c3.re = ar - bi;
                c3.im = ai + br;
            }
            // iditSplit 的 C4 版。原语义 (S=c2+c3, D=c2-c3):
            //   c0' = c0 + S ; c2' = c0 - S
            //   c1' = (c1.re - D.im, c1.im + D.re) ; c3' = (c1.re + D.im, c1.im - D.re)
            HINT_AI void iditSplitC4(C4d &c0, C4d &c1, C4d &c2, C4d &c3)
            {
                const __m256d sr = c2.re + c3.re, si = c2.im + c3.im;
                const __m256d dr = c2.re - c3.re, di = c2.im - c3.im;
                c2.re = c0.re - sr;
                c2.im = c0.im - si;
                c0.re = c0.re + sr;
                c0.im = c0.im + si;
                c3.re = c1.re + di;
                c3.im = c1.im - dr;
                c1.re = c1.re - di;
                c1.im = c1.im + dr;
            }
            // ---- 边界格式的融合 load/store (D22) ----
            // C2/RRII 内存 -> C4 寄存器: 2 条 perm2f128, 复用主循环本来就有的 load
            HINT_AI C4d c4loadC2(const double *p)
            {
                const __m256d a = _mm256_load_pd(p), b = _mm256_load_pd(p + 4);
                return C4d{_mm256_permute2f128_pd(a, b, 0x20), _mm256_permute2f128_pd(a, b, 0x31)};
            }
            HINT_AI void c4storeC2(double *p, const C4d &c)
            {
                _mm256_store_pd(p, _mm256_permute2f128_pd(c.re, c.im, 0x20));
                _mm256_store_pd(p + 4, _mm256_permute2f128_pd(c.re, c.im, 0x31));
            }
            // RIRI 内存 <-> C4 寄存器: 4 条 shuffle
            HINT_AI C4d c4loadRIRI(const double *p)
            {
                const __m256d a = _mm256_load_pd(p), b = _mm256_load_pd(p + 4);
                return C4d{_mm256_permute4x64_pd(_mm256_unpacklo_pd(a, b), 0xD8),
                           _mm256_permute4x64_pd(_mm256_unpackhi_pd(a, b), 0xD8)};
            }
            HINT_AI void c4storeRIRI(double *p, const C4d &c)
            {
                const __m256d lo = _mm256_unpacklo_pd(c.re, c.im);
                const __m256d hi = _mm256_unpackhi_pd(c.re, c.im);
                _mm256_store_pd(p, _mm256_permute2f128_pd(lo, hi, 0x20));
                _mm256_store_pd(p + 4, _mm256_permute2f128_pd(lo, hi, 0x31));
            }
            // MODE: 0 = C4, 1 = C2/RRII, 2 = RIRI
            template <int MODE>
            HINT_AI C4d c4loadM(const double *p)
            {
                if constexpr (MODE == 0)
                    return c4load(p);
                else if constexpr (MODE == 1)
                    return c4loadC2(p);
                else
                    return c4loadRIRI(p);
            }
            template <int MODE>
            HINT_AI void c4storeM(double *p, const C4d &c)
            {
                if constexpr (MODE == 0)
                    c4store(p, c);
                else if constexpr (MODE == 1)
                    c4storeC2(p, c);
                else
                    c4storeRIRI(p, c);
            }
            // RRII|RRII <-> RRRR|IIII 。perm2f128(A,B,0x20)/(A,B,0x31) 这一对是**对合**,
            // 所以 pack 和 unpack 是同一个函数。n 必须是 8 的倍数。
            inline void packC4(double *p, size_t n)
            {
                for (size_t i = 0; i + 8 <= n; i += 8)
                {
                    const __m256d a = _mm256_load_pd(p + i);
                    const __m256d b = _mm256_load_pd(p + i + 4);
                    _mm256_store_pd(p + i, _mm256_permute2f128_pd(a, b, 0x20));
                    _mm256_store_pd(p + i + 4, _mm256_permute2f128_pd(a, b, 0x31));
                }
            }
            // RIRI -> RRRR|IIII (只在 dif<true> 最外层用一次)
            inline void packC4RIRI(double *p, size_t n)
            {
                for (size_t i = 0; i + 8 <= n; i += 8)
                {
                    const __m256d a = _mm256_load_pd(p + i);
                    const __m256d b = _mm256_load_pd(p + i + 4);
                    _mm256_store_pd(p + i, _mm256_permute4x64_pd(_mm256_unpacklo_pd(a, b), 0xD8));
                    _mm256_store_pd(p + i + 4, _mm256_permute4x64_pd(_mm256_unpackhi_pd(a, b), 0xD8));
                }
            }
            // RRRR|IIII -> RIRI (只在 idit<true> 最外层用一次)
            inline void unpackC4RIRI(double *p, size_t n)
            {
                for (size_t i = 0; i + 8 <= n; i += 8)
                {
                    const __m256d r = _mm256_load_pd(p + i);
                    const __m256d m = _mm256_load_pd(p + i + 4);
                    const __m256d lo = _mm256_unpacklo_pd(r, m);
                    const __m256d hi = _mm256_unpackhi_pd(r, m);
                    _mm256_store_pd(p + i, _mm256_permute2f128_pd(lo, hi, 0x20));
                    _mm256_store_pd(p + i + 4, _mm256_permute2f128_pd(lo, hi, 0x31));
                }
            }

            template <typename Float, int DIV>
            struct FFTTable
            {
                using C2 = Complex2<Float>;
                FFTTable(int factor_in) : factor(factor_in), table(8)
                {
                    size_t len = table.size(), rank = len * DIV / 4;
                    auto it = getBegin(rank);
                    Float theta = -HINT_2PI * factor / rank;
                    table[4] = 1, table[6] = 0;
                    table[5] = std::cos(theta), table[7] = std::sin(theta);
                }

                // D21: >= C4_OFF 的段按 C4 布局存放 (packC4 自逆, 故递推前先还原)。
                static constexpr size_t C4_OFF = size_t(FFT_C4_MIN) * 2 / DIV;
                void packSegs()
                {
                    if constexpr (std::is_same_v<Float, double>)
                    {
                        if (table.size() > C4_OFF)
                        {
                            packC4(&table[C4_OFF], table.size() - C4_OFF);
                        }
                    }
                }
                void expand(size_t fft_len)
                {
                    size_t cur_len = table.size() * DIV / 4;
                    if (fft_len <= cur_len)
                    {
                        return;
                    }
                    packSegs();
                    size_t new_len = fft_len * 4 / DIV;
                    table.resize(new_len);
                    for (size_t rank = cur_len * 2; rank <= fft_len; rank *= 2)
                    {
                        auto it = getBegin(rank), last_it = getBegin(rank / 2);
                        Float theta = -HINT_2PI * factor / rank;
                        C2 unit(std::cos(theta), std::sin(theta));
                        size_t len = rank * 2 / DIV;
                        for (auto end = it + len; it < end; it += 8, last_it += 4)
                        {
                            C2 omega0, omega1;
                            omega0.load(last_it);
                            omega1 = omega0.mul(unit);
                            std::swap(omega0.real.x1, omega1.real.x0);
                            std::swap(omega0.imag.x1, omega1.imag.x0);
                            omega0.store(it);
                            omega1.store(it + 4);
                        }
                    }
                    packSegs();
                }
                constexpr const Float *getBegin(size_t rank) const
                {
                    return &table[rank * 2 / DIV];
                }
                constexpr Float *getBegin(size_t rank)
                {
                    return &table[rank * 2 / DIV];
                }
                AlignedVec32<Float> table;  // 32-byte aligned for AVX2 aligned load/store
                int factor;
            };

            template <typename Float>
            class FFT
            {
                using Table = FFTTable<Float, 4>;
                using F2 = Float2<Float>;
                using C2 = Complex2<Float>;

            public:
                FFT() : table1(1), table3(3) {}
                void expand(size_t float_len)
                {
                    table1.expand(float_len / 2);
                    table3.expand(float_len / 2);
                }
                template <bool RIRI_IN>
                void dif(Float inout[], size_t float_len)
                {
                    HINT_ASSUME(is_2pow(float_len));
                    if constexpr (std::is_same_v<Float, double>)
                    {
                        if (float_len > FFT_C4_MIN)
                        {
                            // D22: 边界格式转换融合进主循环的 load, 不再单独走一趟内存
                            difC4<RIRI_IN ? 2 : 1>(inout, float_len);
                            return;
                        }
                    }
                    if (float_len <= FFT_FIXED_MAX)
                    {
                        difDispatch<RIRI_IN>(inout, float_len);
                        return;
                    }
                    // D17: expand() removed from the recursion. Every external entry
                    // (rdif/ridit/rdot) expands for the *largest* length first and the
                    // tables grow monotonically, so children are always covered.
                    const size_t fft_len = float_len / 2, c2_len = fft_len / 2;
                    const size_t stride1 = c2_len / 4, stride2 = stride1 * 2, stride3 = stride1 * 3;
                    // FFT buffers + twiddle tables: AlignedVec32 (posix_memalign 32), hint for AVX2
                    // sizeof(C2) = 32, so it[0], it[stride1], it[stride2], it[stride3] all 32-byte aligned
                    // (stride1/2/3 are integers, base ptr aligned → all C2 elements aligned)
                    auto tp1 = reinterpret_cast<const C2 *>(__builtin_assume_aligned(table1.getBegin(fft_len), 32));
                    auto tp3 = reinterpret_cast<const C2 *>(__builtin_assume_aligned(table3.getBegin(fft_len), 32));
                    auto it = reinterpret_cast<C2 *>(__builtin_assume_aligned(inout, 32));
                    for (auto end = it + stride1; it < end; it++, tp1++, tp3++)
                    {
                        // __builtin_prefetch: twiddle factors (tp1/tp3) are sequential, L1 likely
                        HINT_PREFETCH(tp1 + 4, 0, 1);
                        HINT_PREFETCH(tp3 + 4, 0, 1);
                        C2 c0 = it[0], c1 = it[stride1], c2 = it[stride2], c3 = it[stride3];
                        if (RIRI_IN)
                        {
                            c0.permute(), c1.permute(), c2.permute(), c3.permute();
                        }
                        difSplit(c0.real, c0.imag, c1.real, c1.imag, c2.real, c2.imag, c3.real, c3.imag);
                        it[0] = c0, it[stride1] = c1, it[stride2] = c2.mul(tp1[0]), it[stride3] = c3.mul(tp3[0]);
                    }
                    size_t stride = float_len / 4;
                    // Recursive calls: inout + stride*k are 32-byte aligned (stride*8*k % 32 == 0 for float_len >= 16)
                    dif<false>(reinterpret_cast<Float *>(__builtin_assume_aligned(inout, 32)), stride * 2);
                    dif<false>(reinterpret_cast<Float *>(__builtin_assume_aligned(inout + stride * 2, 32)), stride);
                    dif<false>(reinterpret_cast<Float *>(__builtin_assume_aligned(inout + stride * 3, 32)), stride);
                }
                template <bool RIRI_OUT>
                void idit(Float inout[], size_t float_len)
                {
                    HINT_ASSUME(is_2pow(float_len));
                    if constexpr (std::is_same_v<Float, double>)
                    {
                        if (float_len > FFT_C4_MIN)
                        {
                            // D22: idit 主循环在递归之后, 最外层那趟正好是最后一步 -> store 时转换
                            iditC4<RIRI_OUT ? 2 : 1>(inout, float_len);
                            return;
                        }
                    }
                    if (float_len <= FFT_FIXED_MAX)
                    {
                        iditDispatch<RIRI_OUT>(inout, float_len);
                        return;
                    }
                    // D17: expand() removed from the recursion (see dif()).
                    size_t stride = float_len / 4;
                    idit<false>(reinterpret_cast<Float *>(__builtin_assume_aligned(inout, 32)), stride * 2);
                    idit<false>(reinterpret_cast<Float *>(__builtin_assume_aligned(inout + stride * 2, 32)), stride);
                    idit<false>(reinterpret_cast<Float *>(__builtin_assume_aligned(inout + stride * 3, 32)), stride);
                    const size_t fft_len = float_len / 2, c2_len = fft_len / 2;
                    const size_t stride1 = c2_len / 4, stride2 = stride1 * 2, stride3 = stride1 * 3;
                    // FFT buffers + twiddle tables: AlignedVec32 (posix_memalign 32), hint for AVX2
                    auto tp1 = reinterpret_cast<const C2 *>(__builtin_assume_aligned(table1.getBegin(fft_len), 32));
                    auto tp3 = reinterpret_cast<const C2 *>(__builtin_assume_aligned(table3.getBegin(fft_len), 32));
                    auto it = reinterpret_cast<C2 *>(__builtin_assume_aligned(inout, 32));
                    for (auto end = it + stride1; it < end; it++, tp1++, tp3++)
                    {
                        // __builtin_prefetch: twiddle factors (sequential, L1 likely)
                        HINT_PREFETCH(tp1 + 4, 0, 1);
                        HINT_PREFETCH(tp3 + 4, 0, 1);
                        C2 c0 = it[0], c1 = it[stride1], c2 = it[stride2].mulConj(tp1[0]), c3 = it[stride3].mulConj(tp3[0]);
                        iditSplit(c0.real, c0.imag, c1.real, c1.imag, c2.real, c2.imag, c3.real, c3.imag);
                        if (RIRI_OUT)
                        {
                            c0.permute(), c1.permute(), c2.permute(), c3.permute();
                        }
                        it[0] = c0, it[stride1] = c1, it[stride2] = c2, it[stride3] = c3;
                    }
                }

                // ---- D21: C4 布局的 split-radix 主体 ----
                // 索引与 C2 版完全一致: s1 = float_len/4 恰好等于 C2 版的 stride,
                // twiddle 段长 fft_len/2 也精确匹配 (每迭代吃 4 个 twiddle = 8 double)。
                template <int IN_MODE>
                void difC4(Float inout[], size_t float_len)
                {
                    if constexpr (std::is_same_v<Float, double>)
                    {
                        if (float_len <= FFT_C4_MIN)
                        {
                            // 只有递归内部 (IN_MODE==0) 会落到叶子: 最外层进来时 float_len > FFT_C4_MIN
                            packC4(inout, float_len); // C4 -> RRII, 回到原路径
                            dif<false>(inout, float_len);
                            return;
                        }
                        const size_t fft_len = float_len / 2;
                        const size_t s1 = float_len / 4, s2 = s1 * 2, s3 = s1 * 3;
                        auto tp1 = reinterpret_cast<const double *>(
                            __builtin_assume_aligned(table1.getBegin(fft_len), 32));
                        auto tp3 = reinterpret_cast<const double *>(
                            __builtin_assume_aligned(table3.getBegin(fft_len), 32));
                        auto it = reinterpret_cast<double *>(__builtin_assume_aligned(inout, 32));
                        for (size_t k = fft_len / 16; k > 0; k--, it += 8, tp1 += 8, tp3 += 8)
                        {
                            HINT_PREFETCH(tp1 + 16, 0, 1);
                            HINT_PREFETCH(tp3 + 16, 0, 1);
                            C4d c0 = c4loadM<IN_MODE>(it), c1 = c4loadM<IN_MODE>(it + s1);
                            C4d c2 = c4loadM<IN_MODE>(it + s2), c3 = c4loadM<IN_MODE>(it + s3);
                            difSplitC4(c0, c1, c2, c3);
                            c4store(it, c0);
                            c4store(it + s1, c1);
                            c4store(it + s2, c4mul(c2, c4load(tp1)));
                            c4store(it + s3, c4mul(c3, c4load(tp3)));
                        }
                        const size_t stride = float_len / 4;
                        difC4<0>(inout, stride * 2);
                        difC4<0>(inout + stride * 2, stride);
                        difC4<0>(inout + stride * 3, stride);
                    }
                }
                template <int OUT_MODE>
                void iditC4(Float inout[], size_t float_len)
                {
                    if constexpr (std::is_same_v<Float, double>)
                    {
                        if (float_len <= FFT_C4_MIN)
                        {
                            idit<false>(inout, float_len);
                            packC4(inout, float_len); // RRII -> C4, 交回上层
                            return;
                        }
                        const size_t stride = float_len / 4;
                        iditC4<0>(inout, stride * 2);
                        iditC4<0>(inout + stride * 2, stride);
                        iditC4<0>(inout + stride * 3, stride);
                        const size_t fft_len = float_len / 2;
                        const size_t s1 = float_len / 4, s2 = s1 * 2, s3 = s1 * 3;
                        auto tp1 = reinterpret_cast<const double *>(
                            __builtin_assume_aligned(table1.getBegin(fft_len), 32));
                        auto tp3 = reinterpret_cast<const double *>(
                            __builtin_assume_aligned(table3.getBegin(fft_len), 32));
                        auto it = reinterpret_cast<double *>(__builtin_assume_aligned(inout, 32));
                        for (size_t k = fft_len / 16; k > 0; k--, it += 8, tp1 += 8, tp3 += 8)
                        {
                            HINT_PREFETCH(tp1 + 16, 0, 1);
                            HINT_PREFETCH(tp3 + 16, 0, 1);
                            C4d c0 = c4load(it), c1 = c4load(it + s1);
                            C4d c2 = c4mulConj(c4load(it + s2), c4load(tp1));
                            C4d c3 = c4mulConj(c4load(it + s3), c4load(tp3));
                            iditSplitC4(c0, c1, c2, c3);
                            c4storeM<OUT_MODE>(it, c0);
                            c4storeM<OUT_MODE>(it + s1, c1);
                            c4storeM<OUT_MODE>(it + s2, c2);
                            c4storeM<OUT_MODE>(it + s3, c3);
                        }
                    }
                }

                template <bool RIRI_IN>
                HINT_AI_SMALL void difSmall(Float inout[], size_t float_len)
                {
                    if (float_len <= 2)
                    {
                        return;
                    }
                    auto itc = reinterpret_cast<C2 *>(inout);
                    auto itf = reinterpret_cast<F2 *>(inout);
                    if (float_len == 4) 
                    {
                        if (RIRI_IN)
                        {
                            std::swap(inout[1], inout[2]);
                        }
                        transform2(inout[0], inout[1]);
                        transform2(inout[2], inout[3]);
                    }
                    else 
                    {
                        if (RIRI_IN)
                        {
                            std::swap(inout[1], inout[2]);
                            std::swap(inout[5], inout[6]);
                        }
                        Float r0 = inout[0], r1 = inout[1], i0 = inout[2], i1 = inout[3];
                        Float r2 = inout[4], r3 = inout[5], i2 = inout[6], i3 = inout[7];
                        difSplit(r0, i0, r1, i1, r2, i2, r3, i3);
                        transform2(r0, r1);
                        transform2(i0, i1);
                        inout[0] = r0, inout[1] = r1, inout[2] = i0, inout[3] = i1;
                        inout[4] = r2, inout[5] = r3, inout[6] = i2, inout[7] = i3;
                    }
                }
                template <bool RIRI_OUT>
                HINT_AI_SMALL void iditSmall(Float inout[], size_t float_len)
                {
                    if (float_len <= 2)
                    {
                        return;
                    }
                    auto itc = reinterpret_cast<C2 *>(inout);
                    if (float_len == 4) 
                    {
                        transform2(inout[0], inout[1]);
                        transform2(inout[2], inout[3]);
                        if (RIRI_OUT)
                        {
                            std::swap(inout[1], inout[2]);
                        }
                    }
                    else 
                    {
                        Float r0 = inout[0], r1 = inout[1], i0 = inout[2], i1 = inout[3];
                        Float r2 = inout[4], r3 = inout[5], i2 = inout[6], i3 = inout[7];
                        transform2(r0, r1);
                        transform2(i0, i1);
                        iditSplit(r0, i0, r1, i1, r2, i2, r3, i3);
                        inout[0] = r0, inout[1] = r1, inout[2] = i0, inout[3] = i1;
                        inout[4] = r2, inout[5] = r3, inout[6] = i2, inout[7] = i3;
                        if (RIRI_OUT)
                        {
                            std::swap(inout[1], inout[2]);
                            std::swap(inout[5], inout[6]);
                        }
                    }
                }

                // ---- D17: compile-time unrolled split-radix codelets ----------------
                // difFixed<LEN>/iditFixed<LEN> mirror dif/idit exactly, but LEN is a
                // compile-time constant, so an entire subtree collapses into
                // straight-line code: zero calls, zero loop bookkeeping, constant
                // twiddle offsets, and fixed trip counts the vectorizer fully unrolls.
                template <size_t LEN, bool RIRI_IN>
                HINT_AI void difFixed(Float inout[])
                {
                    static_assert(LEN >= 2 && (LEN & (LEN - 1)) == 0, "LEN must be a power of two");
                    if constexpr (LEN <= 8)
                    {
                        difSmall<RIRI_IN>(inout, LEN);
                    }
                    else
                    {
                        constexpr size_t fft_len = LEN / 2, c2_len = fft_len / 2;
                        constexpr size_t stride1 = c2_len / 4, stride2 = stride1 * 2, stride3 = stride1 * 3;
                        auto tp1 = reinterpret_cast<const C2 *>(__builtin_assume_aligned(table1.getBegin(fft_len), 32));
                        auto tp3 = reinterpret_cast<const C2 *>(__builtin_assume_aligned(table3.getBegin(fft_len), 32));
                        auto it = reinterpret_cast<C2 *>(__builtin_assume_aligned(inout, 32));
                        for (size_t k = 0; k < stride1; k++)
                        {
                            C2 c0 = it[k], c1 = it[k + stride1], c2 = it[k + stride2], c3 = it[k + stride3];
                            if (RIRI_IN)
                            {
                                c0.permute(), c1.permute(), c2.permute(), c3.permute();
                            }
                            difSplit(c0.real, c0.imag, c1.real, c1.imag, c2.real, c2.imag, c3.real, c3.imag);
                            it[k] = c0, it[k + stride1] = c1;
                            it[k + stride2] = c2.mul(tp1[k]), it[k + stride3] = c3.mul(tp3[k]);
                        }
                        constexpr size_t stride = LEN / 4;
                        difFixed<stride * 2, false>(inout);
                        difFixed<stride, false>(inout + stride * 2);
                        difFixed<stride, false>(inout + stride * 3);
                    }
                }
                template <size_t LEN, bool RIRI_OUT>
                HINT_AI void iditFixed(Float inout[])
                {
                    static_assert(LEN >= 2 && (LEN & (LEN - 1)) == 0, "LEN must be a power of two");
                    if constexpr (LEN <= 8)
                    {
                        iditSmall<RIRI_OUT>(inout, LEN);
                    }
                    else
                    {
                        constexpr size_t stride = LEN / 4;
                        iditFixed<stride * 2, false>(inout);
                        iditFixed<stride, false>(inout + stride * 2);
                        iditFixed<stride, false>(inout + stride * 3);
                        constexpr size_t fft_len = LEN / 2, c2_len = fft_len / 2;
                        constexpr size_t stride1 = c2_len / 4, stride2 = stride1 * 2, stride3 = stride1 * 3;
                        auto tp1 = reinterpret_cast<const C2 *>(__builtin_assume_aligned(table1.getBegin(fft_len), 32));
                        auto tp3 = reinterpret_cast<const C2 *>(__builtin_assume_aligned(table3.getBegin(fft_len), 32));
                        auto it = reinterpret_cast<C2 *>(__builtin_assume_aligned(inout, 32));
                        for (size_t k = 0; k < stride1; k++)
                        {
                            C2 c0 = it[k], c1 = it[k + stride1];
                            C2 c2 = it[k + stride2].mulConj(tp1[k]), c3 = it[k + stride3].mulConj(tp3[k]);
                            iditSplit(c0.real, c0.imag, c1.real, c1.imag, c2.real, c2.imag, c3.real, c3.imag);
                            if (RIRI_OUT)
                            {
                                c0.permute(), c1.permute(), c2.permute(), c3.permute();
                            }
                            it[k] = c0, it[k + stride1] = c1, it[k + stride2] = c2, it[k + stride3] = c3;
                        }
                    }
                }
                // Runtime length -> compile-time codelet. Called once per subtree whose
                // size has fallen to FFT_FIXED_MAX, replacing ~FFT_FIXED_MAX/8 calls.
                template <bool RIRI_IN>
                HINT_AI_DISPATCH void difDispatch(Float inout[], size_t float_len)
                {
                    switch (float_len)
                    {
#if FFT_FIXED_MAX >= 128
                    case 128: difFixed<128, RIRI_IN>(inout); return;
#endif
#if FFT_FIXED_MAX >= 64
                    case 64: difFixed<64, RIRI_IN>(inout); return;
#endif
#if FFT_FIXED_MAX >= 32
                    case 32: difFixed<32, RIRI_IN>(inout); return;
#endif
#if FFT_FIXED_MAX >= 16
                    case 16: difFixed<16, RIRI_IN>(inout); return;
#endif
                    default: difSmall<RIRI_IN>(inout, float_len); return;
                    }
                }
                template <bool RIRI_OUT>
                HINT_AI_DISPATCH void iditDispatch(Float inout[], size_t float_len)
                {
                    switch (float_len)
                    {
#if FFT_FIXED_MAX >= 128
                    case 128: iditFixed<128, RIRI_OUT>(inout); return;
#endif
#if FFT_FIXED_MAX >= 64
                    case 64: iditFixed<64, RIRI_OUT>(inout); return;
#endif
#if FFT_FIXED_MAX >= 32
                    case 32: iditFixed<32, RIRI_OUT>(inout); return;
#endif
#if FFT_FIXED_MAX >= 16
                    case 16: iditFixed<16, RIRI_OUT>(inout); return;
#endif
                    default: iditSmall<RIRI_OUT>(inout, float_len); return;
                    }
                }

            private:
                Table table1, table3;
            };

            template <typename Float>
            class BinRevTableC2HP
            {
            public:
                using C1 = std::complex<Float>;
                using C2 = Complex2<Float>;
                static constexpr int MAX_LOG_LEN = 32, LOG_BLOCK = 1, BLOCK = 1 << LOG_BLOCK;
                static constexpr size_t MAX_LEN = size_t(1) << MAX_LOG_LEN;

                BinRevTableC2HP(int log_max_iter_in, int log_fft_len_in)
                    : index(0), pop(0), log_max_iter(log_max_iter_in), log_fft_len(log_fft_len_in)
                {
                    assert(log_max_iter <= log_fft_len);
                    assert(log_fft_len <= MAX_LOG_LEN);
                    const Float factor = Float(1) / (size_t(1) << (log_fft_len - log_max_iter));
                    for (int i = 0; i < MAX_LOG_LEN; i++)
                    {
                        units[i] = getOmega(size_t(1) << (i + 1), 1, factor);
                    }
                    auto fp = reinterpret_cast<Float *>(table);
                    fp[0] = 1, fp[BLOCK] = 0;
                    for (int i = 1; i < BLOCK; i++)
                    {
                        C1 omega = getOmega(BLOCK, bitrev(i, LOG_BLOCK), factor);
                        fp[i] = omega.real(), fp[i + BLOCK] = omega.imag();
                    }
         }
         
                void reset(size_t i = 0)
                {
                    if (i == 0)
                    {
                        pop = 0, index = i;
                        return;
                    }
                    assert((i & (i - 1)) == 0);
                    assert(i % BLOCK == 0);
                    pop = 1, index = i / BLOCK;
                    int zero = hint_ctz(index);
                    auto fp = reinterpret_cast<Float *>(&units[zero + 1]);
                    table[1].real.set1(fp[0]);
                    table[1].imag.set1(fp[1]);
                    table[1] = table[1].mul(table[0]);
                }
                // D24: 返回 table[pop] 的原始 RRII 向量 (它本来就在内存里, 直接 vmovupd,
                // 不产生任何 spill)。推进逻辑与 iterate() 完全一致。
                __m256d iterateV()
                {
                    static_assert(std::is_same_v<Float, double>, "iterateV: double only");
                    const __m256d res = _mm256_loadu_pd(reinterpret_cast<const double *>(&table[pop]));
                    C2 unitx;
                    index++;
                    int zero = hint_ctz(index);
                    auto fp = reinterpret_cast<Float *>(&units[zero + 1]);
                    unitx.real.set1(fp[0]);
                    unitx.imag.set1(fp[1]);
                    pop -= zero;
                    table[pop + 1] = table[pop].mul(unitx);
                    pop++;
                    return res;
                }
                C2 iterate()
                {
                    C2 res = table[pop], unitx;
                    index++;
                    int zero = hint_ctz(index);
                    auto fp = reinterpret_cast<Float *>(&units[zero + 1]);
                    unitx.real.set1(fp[0]);
                    unitx.imag.set1(fp[1]);
                    pop -= zero;
                    table[pop + 1] = table[pop].mul(unitx);
                    pop++;
                    return res;
                }

            private:
                C1 units[MAX_LOG_LEN]{};
                C2 table[MAX_LOG_LEN]{};
                size_t index;
                int pop;
                int log_max_iter, log_fft_len;
            };

            template <size_t RI_DIFF = 1, typename Float>
            inline void dot_rfft(Float *inout0, Float *inout1, const Float *in0, const Float *in1,
                                 const std::complex<Float> &omega0, const Float factor = 1)
            {
                using Complex = std::complex<Float>;
                auto mul1 = [](Complex c0, Complex c1)
                {
                    return Complex(c0.imag() * c1.real() + c0.real() * c1.imag(),
                                   c0.imag() * c1.imag() - c0.real() * c1.real());
                };
                auto mul2 = [](Complex c0, Complex c1)
                {
                    return Complex(c0.real() * c1.imag() - c0.imag() * c1.real(),
                                   c0.real() * c1.real() + c0.imag() * c1.imag());
                };
                auto compute2 = [&omega0](Complex in0, Complex in1, Complex &out0, Complex &out1, auto Func)
                {
                    in1 = std::conj(in1);
                    transform2(in0, in1);
                    in1 = Func(in1, omega0);
                    out0 = in0 + in1;
                    out1 = std::conj(in0 - in1);
                };
                Complex c0, c1;
                {
                    Complex x0, x1, x2, x3;
                    c0.real(inout0[0]), c0.imag(inout0[RI_DIFF]), c1.real(inout1[0]), c1.imag(inout1[RI_DIFF]);
                    compute2(c0, c1, x0, x1, mul1);
                    c0.real(in0[0]), c0.imag(in0[RI_DIFF]), c1.real(in1[0]), c1.imag(in1[RI_DIFF]);
                    compute2(c0, c1, x2, x3, mul1);
                    x0 *= x2 * factor;
                    x1 *= x3 * factor;
                    compute2(x0, x1, c0, c1, mul2);
                }
                inout0[0] = c0.real(), inout0[RI_DIFF] = c0.imag();
                inout1[0] = c1.real(), inout1[RI_DIFF] = c1.imag();
            }
            template <typename Float>
            inline void dot_rfftX2(Float *inout0, Float *inout1, const Float *in0, const Float *in1, const Complex2<Float> &omega0, const Float2<Float> &inv)
            {
                using C2 = Complex2<Float>;
                auto mul1 = [](C2 c0, C2 c1)
                {
                    return C2(c0.imag * c1.real + c0.real * c1.imag,
                              c0.imag * c1.imag - c0.real * c1.real);
                };
                auto mul2 = [](C2 c0, C2 c1)
                {
                    return C2(c0.real * c1.imag - c0.imag * c1.real,
                              c0.real * c1.real + c0.imag * c1.imag);
                };
                auto compute2 = [&omega0](C2 c0, C2 c1, C2 &out0, C2 &out1, auto Func)
                {
                    C2 t0(c0.real + c1.real, c0.imag - c1.imag), t1(c0.real - c1.real, c0.imag + c1.imag);
                    t1 = Func(t1, omega0);
                    out0 = t0 + t1;
                    out1.real = t0.real - t1.real;
                    out1.imag = t1.imag - t0.imag;
                };
                C2 c0, c1;
                {
                    C2 x0, x1, x2, x3;
                    c0.load(inout0), c1.load(inout1);
                    compute2(c0, c1.reverse(), x0, x1, mul1);

                    c0.load(in0), c1.load(in1);
                    compute2(c0, c1.reverse(), x2, x3, mul1);
                    c0 = x0.mul(x2) * inv;
                    c1 = x1.mul(x3) * inv;
                    compute2(c0, c1, c0, c1, mul2);
                }
                c0.store(inout0), c1.reverse().store(inout1);
            }

            // ================= D24: dot_rfftX2 的 C4 (256-bit) 版 =================
            // 4 个复数整体逆序 = 每个分量一条 vpermpd(0x1B)。
            HINT_AI C4d c4rev(const C4d &c)
            {
                return C4d{_mm256_permute4x64_pd(c.re, 0x1B), _mm256_permute4x64_pd(c.im, 0x1B)};
            }
            // C2/RRII 内存 -> 复数逆序的 C4 (镜像端专用)
            HINT_AI C4d c4loadC2Rev(const double *p)
            {
                return c4rev(c4loadC2(p));
            }
            // 两个 RRII 旋转因子向量 -> 一个 C4d, 只要 2 条 vperm2f128
            HINT_AI C4d c4twiddle(const __m256d &w0, const __m256d &w1)
            {
                return C4d{_mm256_permute2f128_pd(w0, w1, 0x20), _mm256_permute2f128_pd(w0, w1, 0x31)};
            }
            // 表达式结构逐条对齐 dot_rfftX2, 只是宽度 128 -> 256, 逐元素语义不变。
            HINT_AI void dot_rfftX4(double *inout0, double *inout1, const double *in0, const double *in1,
                                    const C4d &om, const __m256d &inv)
            {
                auto mul1 = [](const C4d &c0, const C4d &c1)
                {
                    return C4d{c0.im * c1.re + c0.re * c1.im, c0.im * c1.im - c0.re * c1.re};
                };
                auto mul2 = [](const C4d &c0, const C4d &c1)
                {
                    return C4d{c0.re * c1.im - c0.im * c1.re, c0.re * c1.re + c0.im * c1.im};
                };
                auto compute2 = [&om](const C4d &c0, const C4d &c1, C4d &out0, C4d &out1, auto Func)
                {
                    C4d t0{c0.re + c1.re, c0.im - c1.im}, t1{c0.re - c1.re, c0.im + c1.im};
                    t1 = Func(t1, om);
                    out0 = C4d{t0.re + t1.re, t0.im + t1.im};
                    out1 = C4d{t0.re - t1.re, t1.im - t0.im};
                };
                C4d c0, c1;
                {
                    C4d x0, x1, x2, x3;
                    c0 = c4loadC2(inout0), c1 = c4loadC2Rev(inout1);
                    compute2(c0, c1, x0, x1, mul1);

                    c0 = c4loadC2(in0), c1 = c4loadC2Rev(in1);
                    compute2(c0, c1, x2, x3, mul1);

                    c0 = c4mul(x0, x2);
                    c0.re = c0.re * inv, c0.im = c0.im * inv;
                    c1 = c4mul(x1, x3);
                    c1.re = c1.re * inv, c1.im = c1.im * inv;
                    compute2(c0, c1, c0, c1, mul2);
                }
                c4storeC2(inout0, c0);
                c4storeC2(inout1, c4rev(c1));
            }
            
            template <size_t RI_DIFF = 1, typename Float>
            inline void real_dot_binrev(Float in_out[], const Float in[], size_t float_len, Float inv = -1)
            {
                constexpr size_t MAX_LEN = 32;
                constexpr int LOG_LEN = hint_log2(MAX_LEN);
                static_assert(is_2pow(RI_DIFF));
                static_assert(RI_DIFF <= 8);
                assert(is_2pow(float_len));
                assert(float_len <= MAX_LEN);
                if (float_len < 2)
                {
                    return;
                }
                assert(float_len >= RI_DIFF * 2);
                auto idx_trans = [](size_t idx)
                {
                    return (idx / RI_DIFF) * RI_DIFF * 2 + idx % RI_DIFF;
                };
                auto get_omega = [](size_t idx, size_t rank)
                {
                    return std::polar<Float>(1, -HINT_PI * Float(idx) / rank);
                };
                using Complex = std::complex<Float>;
                static const Complex table[]{
                    get_omega(bitrev(4, LOG_LEN), MAX_LEN),
                    get_omega(bitrev(5, LOG_LEN), MAX_LEN),
                    get_omega(bitrev(8, LOG_LEN), MAX_LEN),
                    get_omega(bitrev(9, LOG_LEN), MAX_LEN),
                    get_omega(bitrev(10, LOG_LEN), MAX_LEN),
                    get_omega(bitrev(11, LOG_LEN), MAX_LEN),
                };
                inv = inv < 0 ? Float(2) / float_len : inv * Float(2);
                auto r0 = in_out[0], i0 = in_out[RI_DIFF], r1 = in[0], i1 = in[RI_DIFF];
                transform2(r0, i0);
                transform2(r1, i1);
                r0 *= r1, i0 *= i1;
                transform2(r0, i0);
                in_out[0] = r0 * 0.5 * inv, in_out[RI_DIFF] = i0 * 0.5 * inv;
                if (float_len >= 4)
                {
                    Complex temp(in_out[idx_trans(1)], in_out[idx_trans(1) + RI_DIFF]);
                    temp *= Complex(in[idx_trans(1)], in[idx_trans(1) + RI_DIFF]) * inv;
                    in_out[idx_trans(1)] = temp.real(), in_out[idx_trans(1) + RI_DIFF] = temp.imag();
                }
                if (float_len >= 8)
                {
                    inv *= Float(0.125);
                    dot_rfft<RI_DIFF>(&in_out[idx_trans(2)], &in_out[idx_trans(3)],
                                      &in[idx_trans(2)], &in[idx_trans(3)], Complex(COS_PI_8, -COS_PI_8), inv);
                }

                if (float_len >= 16)
                {
                    dot_rfft<RI_DIFF>(&in_out[idx_trans(4)], &in_out[idx_trans(7)],
                                      &in[idx_trans(4)], &in[idx_trans(7)], table[0], inv);
                    dot_rfft<RI_DIFF>(&in_out[idx_trans(5)], &in_out[idx_trans(6)],
                                      &in[idx_trans(5)], &in[idx_trans(6)], table[1], inv);
                }
                if (float_len >= 32)
                {
                    dot_rfft<RI_DIFF>(&in_out[idx_trans(8)], &in_out[idx_trans(15)],
                                      &in[idx_trans(8)], &in[idx_trans(15)], table[2], inv);
                    dot_rfft<RI_DIFF>(&in_out[idx_trans(9)], &in_out[idx_trans(14)],
                                      &in[idx_trans(9)], &in[idx_trans(14)], table[3], inv);
                    dot_rfft<RI_DIFF>(&in_out[idx_trans(10)], &in_out[idx_trans(13)],
                                      &in[idx_trans(10)], &in[idx_trans(13)], table[4], inv);
                    dot_rfft<RI_DIFF>(&in_out[idx_trans(11)], &in_out[idx_trans(12)],
                                      &in[idx_trans(11)], &in[idx_trans(12)], table[5], inv);
                }
            }

            template <typename Float>
            inline void real_dot_binrev2(Float in_out[], const Float in[], size_t float_len)
            {
                using F2 = Float2<Float>;
                Float inv = 1.0 / float_len;
                real_dot_binrev<2>(in_out, in, 16, inv);
                inv = 0.25 / float_len;
                const F2 invx = F2::from1(inv);
                static thread_local BinRevTableC2HP<Float> table(31, 32);
                for (size_t begin = 16; begin < float_len; begin *= 2)
                {
                    table.reset(begin / 2);
                    if constexpr (std::is_same_v<Float, double>)
                    {
                        if (begin >= 32) // begin/8 次 C2 迭代 -> begin/16 次 C4 迭代
                        {
                            const __m256d invv = _mm256_set1_pd(inv);
                            double *q0 = in_out + begin, *q1 = q0 + begin - 8;
                            const double *q2 = in + begin, *q3 = q2 + begin - 8;
                            for (size_t u = begin / 16; u > 0; u--, q0 += 8, q1 -= 8, q2 += 8, q3 -= 8)
                            {
                                const __m256d w0 = table.iterateV();
                                const __m256d w1 = table.iterateV();
                                dot_rfftX4(q0, q1, q2, q3, c4twiddle(w0, w1), invv);
                            }
                            continue;
                        }
                    }
                    auto it0 = in_out + begin, it1 = it0 + begin - 4;
                    auto it2 = in + begin, it3 = it2 + begin - 4;
                    for (; it0 < it1; it0 += 4, it1 -= 4, it2 += 4, it3 -= 4)
                    {
                        dot_rfftX2(it0, it1, it2, it3, table.iterate(), invx);
                    }
                }
            }

            // 共享 FFT 对象: 所有函数复用同一份旋转因子表，避免首次调用时重复计算
            template <typename Float>
            inline FFT<Float> &getSharedFFT()
            {
                static FFT<Float> fft;
                return fft;
            }

            // ================= radix-3 顶层: 支持 float_len = 3*2^k =================
            // 布局: FL = 6M floats = 3M complex, 拆 3 块, 每块 M complex (M = 2^t >= 32).
            //   块 j 放频率 f = 3k'+j 的分量, 块内是长度 M 子变换的位反转序.
            // 共轭配对 (real-FFT 解包所需的 Z[f] <-> Z[L-f]):
            //   块0: f=3k' 的伙伴是 3(M-k') -> 仍在块0, 位置镜像与 2 幂情形完全同构,
            //        且 twiddle w_{2L}^{3k'} = w_{2M}^{k'} 与长度 M 的标准实数解包一致
            //        => 块0 可原样复用现成的 real_dot_binrev / BinRevTableC2HP 机制.
            //   块1[p] <-> 块2[M-1-p] (M-1-p 是 p 的按位取反, 位反转后仍是取反 => 全局反转),
            //        twiddle w_{2L}^{3k'+1} = w_{2M}^{k'} * w_{6M}
            //        => 与块0 同一个位反转 twiddle 序列, 只差一个常数旋转 w_{FL}.
            constexpr Float64 SQRT3_DIV2 = 0.866025403784438646763723170752936;

            template <typename Float, int FACTOR>
            struct FFTTable3
            {
                using C2 = Complex2<Float>;
                // e[n] = w_{3M}^{FACTOR*n}, n = 0..M-1;  C2 索引 c 存 (e[2c], e[2c+1])
                // 尺寸 M 的表占 table[2M, 4M), 各尺寸首尾相接 (同 FFTTable 的做法)
                FFTTable3() : cur_m(2), table(8)
                {
                    Float theta = Float(-HINT_2PI) * FACTOR / Float(6); // 3*M, M=2
                    table[4] = 1, table[5] = std::cos(theta);
                    table[6] = 0, table[7] = std::sin(theta);
                }
                void expand(size_t m_need)
                {
                    if (m_need <= cur_m)
                    {
                        return;
                    }
                    table.resize(m_need * 4);
                    for (size_t m = cur_m * 2; m <= m_need; m *= 2)
                    {
                        Float *it = &table[m * 2];
                        const Float *last = &table[m]; // 尺寸 m/2 的表
                        Float theta = Float(-HINT_2PI) * FACTOR / Float(3 * m);
                        C2 unit(Float(std::cos(theta)), Float(std::sin(theta)));
                        for (size_t c = 0; c < m / 4; c++)
                        {
                            C2 o0, o1;
                            o0.load(last + 4 * c);
                            o1 = o0.mul(unit);
                            std::swap(o0.real.x1, o1.real.x0);
                            std::swap(o0.imag.x1, o1.imag.x0);
                            o0.store(it + 8 * c);
                            o1.store(it + 8 * c + 4);
                        }
                    }
                    cur_m = m_need;
                }
                const Float *getBegin(size_t m) const { return &table[m * 2]; }
                size_t cur_m;
                AlignedVec32<Float> table;
            };

            template <typename Float>
            inline FFTTable3<Float, 1> &getTable3a()
            {
                static FFTTable3<Float, 1> t;
                return t;
            }
            template <typename Float>
            inline FFTTable3<Float, 2> &getTable3b()
            {
                static FFTTable3<Float, 2> t;
                return t;
            }

            // p in [0,8) 的 twiddle: w_16^{bitrev_8(p)} (与 Lc 无关), 按 C2 成对给出
            template <typename Float>
            inline const Complex2<Float> *smallOmega8()
            {
                using C2 = Complex2<Float>;
                auto w = [](int e)
                { return std::polar<Float>(1, Float(-HINT_2PI) * e / 16); };
                // bitrev_8: 0,4,2,6,1,5,3,7
                static const C2 tab[4] = {
                    C2(w(0).real(), w(4).real(), w(0).imag(), w(4).imag()),
                    C2(w(2).real(), w(6).real(), w(2).imag(), w(6).imag()),
                    C2(w(1).real(), w(5).real(), w(1).imag(), w(5).imag()),
                    C2(w(3).real(), w(7).real(), w(3).imag(), w(7).imag())};
                return tab;
            }

            // radix-3 DIF 级 (顶层): 3 点 DFT + 旋转因子, 之后三块各自走 2 幂 dif
            template <bool RIRI_IN, typename Float>
            inline void dif3Stage(Float *inout, size_t m)
            {
                using C2 = Complex2<Float>;
                const Float H = Float(0.5), S = Float(SQRT3_DIV2);
                auto &t1 = getTable3a<Float>();
                auto &t2 = getTable3b<Float>();
                t1.expand(m), t2.expand(m);
                auto p1 = reinterpret_cast<const C2 *>(t1.getBegin(m));
                auto p2 = reinterpret_cast<const C2 *>(t2.getBegin(m));
                Float *b0 = inout, *b1 = inout + m * 2, *b2 = inout + m * 4;
                for (size_t c = m / 2; c > 0; c--, b0 += 4, b1 += 4, b2 += 4, p1++, p2++)
                {
                    C2 a0, a1, a2;
                    a0.load(b0), a1.load(b1), a2.load(b2);
                    if (RIRI_IN)
                    {
                        a0.permute(), a1.permute(), a2.permute();
                    }
                    // w3 = e^{-2pi i/3} = -1/2 - i*sqrt3/2
                    C2 s = a1 + a2, d = a1 - a2;
                    C2 r0 = a0 + s;
                    C2 h(a0.real - s.real * H, a0.imag - s.imag * H);
                    C2 g(d.imag * (-S), d.real * S); // i*(sqrt3/2)*d
                    r0.store(b0);
                    (h - g).mul(p1[0]).store(b1);
                    (h + g).mul(p2[0]).store(b2);
                }
            }
            // radix-3 IDIT 级 (顶层): 先解旋转因子, 再乘 conj(W3) (不含 1/3, 与 idit 同约定)
            template <bool RIRI_OUT, typename Float>
            inline void idit3Stage(Float *inout, size_t m)
            {
                using C2 = Complex2<Float>;
                const Float H = Float(0.5), S = Float(SQRT3_DIV2);
                auto &t1 = getTable3a<Float>();
                auto &t2 = getTable3b<Float>();
                t1.expand(m), t2.expand(m);
                auto p1 = reinterpret_cast<const C2 *>(t1.getBegin(m));
                auto p2 = reinterpret_cast<const C2 *>(t2.getBegin(m));
                Float *b0 = inout, *b1 = inout + m * 2, *b2 = inout + m * 4;
                for (size_t c = m / 2; c > 0; c--, b0 += 4, b1 += 4, b2 += 4, p1++, p2++)
                {
                    C2 t0, u1, u2;
                    t0.load(b0), u1.load(b1), u2.load(b2);
                    C2 x1 = u1.mulConj(p1[0]), x2 = u2.mulConj(p2[0]);
                    C2 s = x1 + x2, d = x1 - x2;
                    C2 a0 = t0 + s;
                    C2 h(t0.real - s.real * H, t0.imag - s.imag * H);
                    C2 g(d.imag * (-S), d.real * S);
                    C2 a1 = h + g, a2 = h - g;
                    if (RIRI_OUT)
                    {
                        a0.permute(), a1.permute(), a2.permute();
                    }
                    a0.store(b0), a1.store(b1), a2.store(b2);
                }
            }

            // ================= D25: radix-3 顶层的 C4 版 =================
            // 原 dif3Stage/idit3Stage 用 Complex2 (RRII): real/imag 各占半个 ymm, 每迭代
            // 只处理 2 个复数, 且 .mul()/.permute() 都在 128-bit 分区间跨 lane。
            // C4 版一次吃 4 个复数, 3 点 DFT 的全部加减/缩放变成同 lane 全宽运算; 更关键的是
            // 它的输出直接就是 C4 布局 -> 下游可以走 difC4<0> (零 shuffle 的 c4load), 省掉
            // 原 difC4<1> 每迭代 4 次 c4loadC2 = 8 条 perm2f128。idit 方向对称。
            template <bool RIRI_IN>
            inline void dif3StageC4(double *inout, size_t m)
            {
                const __m256d vh = _mm256_set1_pd(0.5);
                const __m256d vs = _mm256_set1_pd(double(SQRT3_DIV2));
                const __m256d vns = _mm256_set1_pd(-double(SQRT3_DIV2));
                auto &t1 = getTable3a<double>();
                auto &t2 = getTable3b<double>();
                t1.expand(m), t2.expand(m);
                auto p1 = reinterpret_cast<const double *>(__builtin_assume_aligned(t1.getBegin(m), 32));
                auto p2 = reinterpret_cast<const double *>(__builtin_assume_aligned(t2.getBegin(m), 32));
                double *b0 = reinterpret_cast<double *>(__builtin_assume_aligned(inout, 32));
                double *b1 = b0 + m * 2, *b2 = b0 + m * 4;
                for (size_t c = m / 4; c > 0; c--, b0 += 8, b1 += 8, b2 += 8, p1 += 8, p2 += 8)
                {
                    HINT_PREFETCH(p1 + 16, 0, 1);
                    HINT_PREFETCH(p2 + 16, 0, 1);
                    const C4d a0 = c4loadM<RIRI_IN ? 2 : 1>(b0);
                    const C4d a1 = c4loadM<RIRI_IN ? 2 : 1>(b1);
                    const C4d a2 = c4loadM<RIRI_IN ? 2 : 1>(b2);
                    const __m256d sr = a1.re + a2.re, si = a1.im + a2.im;
                    const __m256d dr = a1.re - a2.re, di = a1.im - a2.im;
                    const __m256d hr = a0.re - sr * vh, hi = a0.im - si * vh;
                    const __m256d gr = di * vns, gi = dr * vs;
                    c4store(b0, C4d{a0.re + sr, a0.im + si});
                    c4store(b1, c4mul(C4d{hr - gr, hi - gi}, c4loadC2(p1)));
                    c4store(b2, c4mul(C4d{hr + gr, hi + gi}, c4loadC2(p2)));
                }
            }
            template <bool RIRI_OUT>
            inline void idit3StageC4(double *inout, size_t m)
            {
                const __m256d vh = _mm256_set1_pd(0.5);
                const __m256d vs = _mm256_set1_pd(double(SQRT3_DIV2));
                const __m256d vns = _mm256_set1_pd(-double(SQRT3_DIV2));
                auto &t1 = getTable3a<double>();
                auto &t2 = getTable3b<double>();
                t1.expand(m), t2.expand(m);
                auto p1 = reinterpret_cast<const double *>(__builtin_assume_aligned(t1.getBegin(m), 32));
                auto p2 = reinterpret_cast<const double *>(__builtin_assume_aligned(t2.getBegin(m), 32));
                double *b0 = reinterpret_cast<double *>(__builtin_assume_aligned(inout, 32));
                double *b1 = b0 + m * 2, *b2 = b0 + m * 4;
                for (size_t c = m / 4; c > 0; c--, b0 += 8, b1 += 8, b2 += 8, p1 += 8, p2 += 8)
                {
                    HINT_PREFETCH(p1 + 16, 0, 1);
                    HINT_PREFETCH(p2 + 16, 0, 1);
                    const C4d t0 = c4load(b0);
                    const C4d x1 = c4mulConj(c4load(b1), c4loadC2(p1));
                    const C4d x2 = c4mulConj(c4load(b2), c4loadC2(p2));
                    const __m256d sr = x1.re + x2.re, si = x1.im + x2.im;
                    const __m256d dr = x1.re - x2.re, di = x1.im - x2.im;
                    const __m256d hr = t0.re - sr * vh, hi = t0.im - si * vh;
                    const __m256d gr = di * vns, gi = dr * vs;
                    c4storeM<RIRI_OUT ? 2 : 1>(b0, C4d{t0.re + sr, t0.im + si});
                    c4storeM<RIRI_OUT ? 2 : 1>(b1, C4d{hr + gr, hi + gi});
                    c4storeM<RIRI_OUT ? 2 : 1>(b2, C4d{hr - gr, hi - gi});
                }
            }

            template <typename Float>
            inline void real_dot_binrev3(Float in_out[], const Float in[], size_t float_len)
            {
                using F2 = Float2<Float>;
                using C2 = Complex2<Float>;
                const size_t m = float_len / 6, blk = m * 2;
                const Float inv = Float(1) / Float(float_len);
                const F2 invx = F2::from1(Float(0.25) / Float(float_len));
                static thread_local BinRevTableC2HP<Float> table(31, 32);
                // ---- 块0: 与 2 幂长度 blk 的实数解包完全同构 ----
                real_dot_binrev<2>(in_out, in, 16, inv);
                for (size_t begin = 16; begin < blk; begin *= 2)
                {
                    table.reset(begin / 2);
                    if constexpr (std::is_same_v<Float, double>)
                    {
                        if (begin >= 32)
                        {
                            const __m256d invv = _mm256_set1_pd(Float(0.25) / Float(float_len));
                            double *q0 = in_out + begin, *q1 = q0 + begin - 8;
                            const double *q2 = in + begin, *q3 = q2 + begin - 8;
                            for (size_t u = begin / 16; u > 0; u--, q0 += 8, q1 -= 8, q2 += 8, q3 -= 8)
                            {
                                const __m256d w0 = table.iterateV();
                                const __m256d w1 = table.iterateV();
                                dot_rfftX4(q0, q1, q2, q3, c4twiddle(w0, w1), invv);
                            }
                            continue;
                        }
                    }
                    auto it0 = in_out + begin, it1 = it0 + begin - 4;
                    auto it2 = in + begin, it3 = it2 + begin - 4;
                    for (; it0 < it1; it0 += 4, it1 -= 4, it2 += 4, it3 -= 4)
                    {
                        dot_rfftX2(it0, it1, it2, it3, table.iterate(), invx);
                    }
                }
                // ---- 块1[p] <-> 块2[M-1-p], twiddle 同序列 * 常数 w_{FL} ----
                Float *o1 = in_out + blk, *o2 = in_out + blk * 2;
                const Float *n1 = in + blk, *n2 = in + blk * 2;
                const Float ang = Float(-HINT_2PI) / Float(float_len);
                const C2 rot(Float(std::cos(ang)), Float(std::sin(ang)));
                {
                    const C2 *sm = smallOmega8<Float>();
                    for (size_t t = 0; t < 4; t++)
                    {
                        const size_t f = 4 * t;
                        dot_rfftX2(o1 + f, o2 + blk - 4 - f, n1 + f, n2 + blk - 4 - f,
                                   sm[t].mul(rot), invx);
                    }
                }
                // 注意: begin 是"块内 float 偏移", 块1 有 M complex = blk floats,
                // 因此层循环上界必须是 blk (不是 m=M, 那是 complex 计数, 会漏掉最后一层)
                for (size_t begin = 16; begin < blk; begin *= 2)
                {
                    table.reset(begin / 2);
                    if constexpr (std::is_same_v<Float, double>)
                    {
                        // begin/4 次 C2 迭代 (begin >= 16 -> 至少 4 次) -> begin/8 次 C4
                        const __m256d invv = _mm256_set1_pd(Float(0.25) / Float(float_len));
                        const C4d rotv{_mm256_set1_pd(rot.real.x0), _mm256_set1_pd(rot.imag.x0)};
                        double *p0 = o1 + begin, *p1 = o2 + blk - 8 - begin;
                        const double *r0 = n1 + begin, *r1 = n2 + blk - 8 - begin;
                        for (size_t u = begin / 8; u > 0; u--, p0 += 8, p1 -= 8, r0 += 8, r1 -= 8)
                        {
                            const __m256d w0 = table.iterateV();
                            const __m256d w1 = table.iterateV();
                            dot_rfftX4(p0, p1, r0, r1, c4mul(c4twiddle(w0, w1), rotv), invv);
                        }
                        continue;
                    }
                    auto i0 = o1 + begin, i1 = o2 + blk - 4 - begin;
                    auto j0 = n1 + begin, j1 = n2 + blk - 4 - begin;
                    for (size_t u = begin / 4; u > 0; u--, i0 += 4, i1 -= 4, j0 += 4, j1 -= 4)
                    {
                        dot_rfftX2(i0, i1, j0, j1, table.iterate().mul(rot), invx);
                    }
                }
            }

#ifdef FFTHIST
            // 诊断用: 统计所有正/逆变换的长度直方图 (不进入正式构建)
            inline std::map<size_t, size_t> &fftHist()
            {
                static std::map<size_t, size_t> h;
                static bool reg = (std::atexit([]
                                               {
                    double work = 0; size_t n = 0;
                    fprintf(stderr, "==== FFT length histogram ====\n");
                    for (auto &kv : fftHist()) {
                        double w = double(kv.first) * std::log2(double(kv.first)) * kv.second;
                        work += w; n += kv.second;
                        fprintf(stderr, "  len=%-9zu x%-7zu  %s  NlogN=%.4g\n",
                                kv.first, kv.second,
                                (kv.first & (kv.first - 1)) ? "fft3" : "2pow", w);
                    }
                    fprintf(stderr, "  TOTAL transforms=%zu  sum(N*logN)=%.6g\n", n, work);
                    if (const char *fn = getenv("FFTHIST_CSV")) {
                        if (FILE *f = fopen(fn, "w")) {
                            fprintf(f, "len,cnt\n");
                            for (auto &kv : fftHist()) fprintf(f, "%zu,%zu\n", kv.first, kv.second);
                            fclose(f);
                        }
                    } }),
                                   true);
                (void)reg;
                return h;
            }
#define FFTHIST_TICK(n) (fftHist()[(n)]++)
#else
#define FFTHIST_TICK(n) ((void)0)
#endif

            // ---- 统一入口: float_len 可以是 2^k 或 3*2^k ----
            template <typename Float>
            inline void rdif(Float *p, size_t float_len)
            {
                FFTHIST_TICK(float_len);
                auto &fft = getSharedFFT<Float>();
                if (!is_fft3(float_len))
                {
                    fft.expand(float_len);
                    fft.template dif<true>(p, float_len);
                    return;
                }
                const size_t m = float_len / 6, blk = m * 2;
                fft.expand(blk);
                if constexpr (std::is_same_v<Float, double>)
                {
                    // D25: radix-3 级直接吐 C4, 三个子块用 IN_MODE=0 (零 shuffle)
                    if (blk > FFT_C4_MIN && (m & 3) == 0)
                    {
                        dif3StageC4<true>(reinterpret_cast<double *>(p), m);
                        fft.template difC4<0>(p, blk);
                        fft.template difC4<0>(p + blk, blk);
                        fft.template difC4<0>(p + blk * 2, blk);
                        return;
                    }
                }
                dif3Stage<true>(p, m);
                fft.template dif<false>(p, blk);
                fft.template dif<false>(p + blk, blk);
                fft.template dif<false>(p + blk * 2, blk);
            }
            template <typename Float>
            inline void ridit(Float *p, size_t float_len)
            {
                FFTHIST_TICK(float_len);
                auto &fft = getSharedFFT<Float>();
                if (!is_fft3(float_len))
                {
                    fft.expand(float_len);
                    fft.template idit<true>(p, float_len);
                    return;
                }
                const size_t m = float_len / 6, blk = m * 2;
                fft.expand(blk);
                if constexpr (std::is_same_v<Float, double>)
                {
                    if (blk > FFT_C4_MIN && (m & 3) == 0)
                    {
                        fft.template iditC4<0>(p, blk);
                        fft.template iditC4<0>(p + blk, blk);
                        fft.template iditC4<0>(p + blk * 2, blk);
                        idit3StageC4<true>(reinterpret_cast<double *>(p), m);
                        return;
                    }
                }
                fft.template idit<false>(p, blk);
                fft.template idit<false>(p + blk, blk);
                fft.template idit<false>(p + blk * 2, blk);
                idit3Stage<true>(p, m);
            }
            template <typename Float>
            inline void rdot(Float *in_out, const Float *in, size_t float_len)
            {
                if (!is_fft3(float_len))
                {
                    real_dot_binrev2(in_out, in, float_len);
                }
                else
                {
                    real_dot_binrev3(in_out, in, float_len);
                }
            }

            template <typename Float>
            inline void real_conv(Float *in_out1, Float *in2, size_t float_len)
            {
                assert(is_2pow(float_len) || is_fft3(float_len));
                assert(float_len <= FFT_MAX_LEN * 2);
                HINT_ASSUME(float_len >= 16);
                rdif(in_out1, float_len);
                if (in_out1 != in2)
                {
                    rdif(in2, float_len);
                }
                rdot(in_out1, in2, float_len);
                ridit(in_out1, float_len);
            }
        }
    }
    constexpr size_t count_base10(uint64_t num)
    {
        size_t count = 0;
        while (num)
        {
            num /= 10;
            count++;
        }
        return count;
    }
    // 64KB 查表法：2 字节 ASCII → 0-99 (从 fusion.cpp 移植)
    struct ParseTable {
        uint8_t table[0x10000];
        constexpr ParseTable() : table() {
            for (uint32_t i = 48; i < 58; ++i)
                for (uint32_t j = 48; j < 58; ++j)
                    table[i << 8 | j] = uint8_t((i & 15) * 10 + (j & 15));
        }
        inline uint32_t operator()(const char* s) const {
            return table[uint32_t(uint8_t(s[0])) << 8 | uint32_t(uint8_t(s[1]))];
        }
    };
    static constexpr ParseTable parseTable{};

    inline uint16_t str4toi(const char *s)
    {
        return uint16_t(parseTable(s) * 100 + parseTable(s + 2));
    }
    // 16 字节 ASCII 数字 → 4 个 uint16 limbs (纯 SSE2, x86-64 baseline)
    // 倒序解析: bytes[0..3]   -> limb[3] (最高位)
    //          bytes[4..7]   -> limb[2]
    //          bytes[8..11]  -> limb[1]
    //          bytes[12..15] -> limb[0] (最低位)
    // 注意: 不用 _mm_maddubs_epi16 (那是 SSSE3, 编译命令无 -mssse3 会报错),
    //       改用 SSE2 的 unpacklo/hi + mullo_epi16 + madd_epi16(ones) 等价实现。
    //       另修正任务原稿 mul4 常量布局 bug: 必须用 _mm_set1_epi32(0x00010064)
    //       (low word=100, high word=1), 否则奇数 word=0 → 12*100+34*0=1200。
    inline void str16to4limbs(const char *p, uint16_t *limbs4)
    {
        __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i *>(p));
        v = _mm_sub_epi8(v, _mm_set1_epi8('0'));
        __m128i zero = _mm_setzero_si128();
        // 拆成低/高 8 字节, 各零扩展到 8 个 16 位
        __m128i lo = _mm_unpacklo_epi8(v, zero);  // [b0..b7] as words
        __m128i hi = _mm_unpackhi_epi8(v, zero);  // [b8..b15] as words
        // 字节对 (10,1) → 2 位数: 先按 [10,1,10,1,...] 缩放, 再用 madd(ones) 合并相邻
        __m128i mul2 = _mm_set_epi16(1, 10, 1, 10, 1, 10, 1, 10);  // low→high: [10,1,...]
        __m128i lo_s = _mm_mullo_epi16(lo, mul2);
        __m128i hi_s = _mm_mullo_epi16(hi, mul2);
        __m128i ones = _mm_set1_epi16(1);
        __m128i lo_d = _mm_madd_epi16(lo_s, ones);  // 4 dwords: [b0b1, b2b3, b4b5, b6b7]
        __m128i hi_d = _mm_madd_epi16(hi_s, ones);  // 4 dwords: [b8b9, b10b11, b12b13, b14b15]
        // 打包 8 个 dword(0-99) → 8 个 word
        __m128i packed = _mm_packs_epi32(lo_d, hi_d);
        // 字对 (100,1) → 4 位数
        __m128i mul4 = _mm_set1_epi32(0x00010064);  // low word=100, high word=1
        __m128i v4 = _mm_madd_epi16(packed, mul4);  // 4 dwords: [bytes0-3, bytes4-7, bytes8-11, bytes12-15]
        // 反转 dword 顺序 → 低位优先 (与 data[0]=LSB 一致)
        v4 = _mm_shuffle_epi32(v4, _MM_SHUFFLE(0, 1, 2, 3));
        // 打包 4 dword(0-9999, 不溢出 int16) → 4 word 并存储
        __m128i result = _mm_packs_epi32(v4, v4);
        _mm_storel_epi64(reinterpret_cast<__m128i *>(limbs4), result);
    }
    // OPT: AVX2 向量化 uint16→double 转换 (16 元素/次), 替代 std::copy 的标量逐元素转换
    // 用于 FFT 输入准备: limb 数组 (uint16) → double 缓冲区
    inline void copyU16ToF64(const uint16_t *src, double *dst, size_t n)
    {
        size_t j = 0;
#if defined(__AVX2__)
        for (; j + 16 <= n; j += 16)
        {
            // 加载 32 字节 = 16 个 uint16, 拆成高低各 8 个
            __m256i vals = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(src + j));
            __m128i lo128 = _mm256_castsi256_si128(vals);      // 低 8 字节 = src[j..j+7]
            __m128i hi128 = _mm256_extracti128_si256(vals, 1); // 高 8 字节 = src[j+8..j+15]
            __m256i lo32 = _mm256_cvtepu16_epi32(lo128);       // 低 8 个 uint16 → 8 个 int32
            __m256i hi32 = _mm256_cvtepu16_epi32(hi128);       // 高 8 个 uint16 → 8 个 int32
            __m256d d0 = _mm256_cvtepi32_pd(_mm256_castsi256_si128(lo32));
            __m256d d1 = _mm256_cvtepi32_pd(_mm256_extracti128_si256(lo32, 1));
            __m256d d2 = _mm256_cvtepi32_pd(_mm256_castsi256_si128(hi32));
            __m256d d3 = _mm256_cvtepi32_pd(_mm256_extracti128_si256(hi32, 1));
            _mm256_storeu_pd(dst + j, d0);
            _mm256_storeu_pd(dst + j + 4, d1);
            _mm256_storeu_pd(dst + j + 8, d2);
            _mm256_storeu_pd(dst + j + 12, d3);
        }
#endif
        for (; j < n; j++)
        {
            dst[j] = src[j];
        }
    }
    // OPT: Merge copyU16ToF64 + std::fill into single pass to reduce memory traffic
    // Copies n uint16 to double, then fills remaining [n, total) with 0.0
    inline void copyU16ToF64AndFill(const uint16_t *src, double *dst, size_t n, size_t total)
    {
        size_t j = 0;
#if defined(__AVX2__)
        // Copy phase: uint16 → double (16 at a time)
        for (; j + 16 <= n; j += 16)
        {
            __m256i vals = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(src + j));
            __m128i lo128 = _mm256_castsi256_si128(vals);
            __m128i hi128 = _mm256_extracti128_si256(vals, 1);
            __m256i lo32 = _mm256_cvtepu16_epi32(lo128);
            __m256i hi32 = _mm256_cvtepu16_epi32(hi128);
            __m256d d0 = _mm256_cvtepi32_pd(_mm256_castsi256_si128(lo32));
            __m256d d1 = _mm256_cvtepi32_pd(_mm256_extracti128_si256(lo32, 1));
            __m256d d2 = _mm256_cvtepi32_pd(_mm256_castsi256_si128(hi32));
            __m256d d3 = _mm256_cvtepi32_pd(_mm256_extracti128_si256(hi32, 1));
            _mm256_storeu_pd(dst + j, d0);
            _mm256_storeu_pd(dst + j + 4, d1);
            _mm256_storeu_pd(dst + j + 8, d2);
            _mm256_storeu_pd(dst + j + 12, d3);
        }
#endif
        for (; j < n; j++)
        {
            dst[j] = src[j];
        }
        // Fill phase: zero remaining [n, total) — use AVX2 _mm256_setzero_pd for 4 at a time
        j = n;
#if defined(__AVX2__)
        __m256d zero = _mm256_setzero_pd();
        for (; j + 4 <= total; j += 4)
        {
            _mm256_storeu_pd(dst + j, zero);
        }
#endif
        for (; j < total; j++)
        {
            dst[j] = 0.0;
        }
    }
    constexpr void itostr4(uint16_t n, char *s)
    {
        s[0] = n / 1000 + '0';
        s[1] = n / 100 % 10 + '0';
        s[2] = n / 10 % 10 + '0';
        s[3] = n % 10 + '0';
    }
    template <typename T>
    struct ViewTy
    {
        const T *ptr;
        size_t size;
        ViewTy() = default;
        ViewTy(const T *ptr, size_t size) : ptr(ptr), size(size) {}
        const T &operator[](size_t index) const
        {
            return ptr[index];
        }
        ViewTy operator+(size_t offset) const
        {
            assert(offset <= size);
            return ViewTy{ptr + offset, size - offset};
        }
        const T *begin() const
        {
            return ptr;
        }
        const T *end() const
        {
            return ptr + size;
        }
    };
    template <typename T>
    struct SpanTy
    {
        T *ptr;
        size_t size;
        SpanTy() = default;
        SpanTy(T *ptr, size_t size) : ptr(ptr), size(size) {}
        const T &operator[](size_t index) const
        {
            return ptr[index];
        }
        T &operator[](size_t index)
        {
            return ptr[index];
        }
        operator ViewTy<T>()
        {
            return ViewTy<T>{ptr, size};
        }
        SpanTy operator+(size_t offset) const
        {
            assert(offset <= size);
            return SpanTy{ptr + offset, size - offset};
        }
        const T *begin() const
        {
            return ptr;
        }
        const T *end() const
        {
            return ptr + size;
        }
        T *begin()
        {
            return ptr;
        }
        T *end()
        {
            return ptr + size;
        }
    };

    template <typename T>
    T add_half(T a, T b, T base, T &cf)
    {
        T r = a + b;
        cf = r >= base;
        T mask = T(0) - T(cf);
        return r - (base & mask);
    }
    template <typename T>
    T sub_half(T a, T b, T base, T &bf)
    {
        bf = a < b;
        T mask = T(0) - T(bf);
        return a - b + (base & mask);
    }

    // D19: the scalar back-scan costs ~3 Ir per zero limb and shows up as 4.9M Ir
    // (100% scalar) inside removeLeadingZero on length_ratio_integer_03, because
    // the mu-division block loop keeps producing operands with long zero tails.
    // vptest clears 16 limbs at a time instead, ~0.25 Ir per limb.
    template <typename T>
    constexpr size_t count_true_length(const T array[], size_t length)
    {
        if (nullptr == array)
        {
            return 0;
        }
        if constexpr (sizeof(T) == 2)
        {
            if (!std::is_constant_evaluated())
            {
                while (length >= 16)
                {
                    __m256i v = _mm256_loadu_si256(
                        reinterpret_cast<const __m256i *>(array + length - 16));
                    if (!_mm256_testz_si256(v, v))
                    {
                        break;
                    }
                    length -= 16;
                }
            }
        }
        // __builtin_expect: trailing zeros are rare for most inputs — hint unlikely
        while (length > 0 && HINT_UNLIKELY(array[length - 1] == 0))
        {
            length--;
        }
        return length;
    }

    // OPT-1: writeTo 输出 4 位查表移到命名空间作用域 constexpr，
    // 消除函数局部 static 的 magic static guard 运行时开销
    namespace
    {
        struct OutTable
        {
            uint32_t t[10000];
            constexpr OutTable() : t()
            {
                for (int i = 0; i < 10000; i++)
                {
                    t[i] = uint32_t(i / 1000 + '0') |
                           (uint32_t(i / 100 % 10 + '0') << 8) |
                           (uint32_t(i / 10 % 10 + '0') << 16) |
                           (uint32_t(i % 10 + '0') << 24);
                }
            }
        };
        constexpr OutTable outTable{};
    }

    class Integer
    {
    public:
        using Limb = uint16_t;
        using Limb2 = uint32_t;
        using DataVec = std::vector<Limb>;
        using Span = SpanTy<Limb>;
        using View = ViewTy<Limb>;
        static constexpr Limb BASE_DIGIT = 4;
        static constexpr Limb BASE = qpow(10, BASE_DIGIT);
        static constexpr Limb HALF_BASE = BASE / 2;
        // Barrett reduction for s/BASE and s%BASE (BASE=10000):
        //   M = ceil(2^64 / 10000) = 0x68DB8BAC710CC
        //   q = divBASE(s) = (uint64_t)((unsigned __int128)s * M >> 64) == s / 10000
        //   r = s - q * BASE                                              == s % 10000
        // S=64 means q = high 64 bits of 128-bit product (no shift needed, one `mul` insn).
        // GCC default uses S=75 (mulq + shrq $11); S=64 saves the shrq in the serial carry chain.
        // Correct for s <= 2200231879019999 (~2^51). FFT convolution sums are bounded by
        // N*(BASE-1)^2 ~ N*10^8; for double-precision FFT N < 2^50/10^8 ~ 11263 limbs
        // => s_max ~ 1.1e12, well within the safe range (2000x margin).
        static constexpr uint64_t BARRETT_M = 0x68DB8BAC710CCULL;
        static uint64_t divBASE(uint64_t s) { return (uint64_t)((unsigned __int128)s * BARRETT_M >> 64); }
        Integer() : data(), sign(false) {}
        
        Integer(const Integer &input) = default;
        
        Integer(Integer &&input) = default;
        
        Integer(const std::string &input) : sign(false)
        {
            fromString(input);
        }
        Integer(const char *input) : sign(false)
        {
            fromString(input);
        }
        Integer(View input) : sign(false), data(input.begin(), input.end())
        {
            removeLeadingZero();
        }
        Integer(Span input) : sign(false), data(input.begin(), input.end())
        {
            removeLeadingZero();
        }
        
        template <typename T>
        Integer(const T &input)
        {
            sign = input < 0;
            uint64_t num = std::abs(input);
            while (num > 0)
            {
                data.push_back(num % BASE);
                num /= BASE;
            }
        }
        
        Integer &operator=(const Integer &input) = default;
        
        Integer &operator=(Integer &&input) = default;

        View getView() const
        {
            return View{data.data(), data.size()};
        }
        Span getSpan()
        {
            return Span{data.data(), data.size()};
        }

        bool isOdd() const
        {
            if (length() == 0)
            {
                return false;
            }
            return data[0] % 2 == 1;
        }

        bool isZero() const
        {
            return length() == 0;
        }
        
        void setSign(bool new_sign)
        {
            sign = new_sign;
        }
        
        bool isNeg() const
        {
            return sign && (length() > 0);
        }
        size_t length() const
        {
            return data.size();
        }

        void removeLeadingZero()
        {
            size_t len = length();
            len = count_true_length(data.data(), len);
            data.resize(len);
            sign = sign && (len > 0);
        }
        void clear()
        {
            data.clear();
            sign = false;
        }
        void fromString(const std::string &str)
        {
            fromCharRange(str.data(), str.data() + str.size());
        }

        std::string toString() const
        {
            std::string res;
            std::vector<char> buf(4);
            if (isZero())
            {
                res = "0";
            }
            else
            {
                if (isNeg())
                {
                    res = '-';
                }
                res += std::to_string(data.back());
                size_t i = data.size() - 1;
                while (i > 0)
                {
                    i--;
                    itostr4(data[i], buf.data());
                    res.append(buf.data(), 4);
                }
            }
            return res;
        }

        // 零拷贝写入到 out，返回写入字节数（不含 '\0'）
        size_t writeTo(char* out) const
        {
            // 4 位数字查表，避免 itostr4 的 4 次除法
            // OPT-1: 表已移至命名空间作用域 constexpr outTable（见 class Integer 之前），
            // 消除函数局部 static 的 magic static guard
            // 原代码（保留作注释）：
            //   static const uint32_t* table = []() {
            //       static uint32_t t[10000];
            //       for (int i = 0; i < 10000; i++) {
            //           t[i] = uint32_t(i / 1000 + '0') |
            //                  (uint32_t(i / 100 % 10 + '0') << 8) |
            //                  (uint32_t(i / 10 % 10 + '0') << 16) |
            //                  (uint32_t(i % 10 + '0') << 24);
            //       }
            //       return t;
            //   }();
            if (isZero())
            {
                out[0] = '0';
                return 1;
            }
            char* p = out;
            if (isNeg())
            {
                *p++ = '-';
            }
            // 最高位 limb 不补前导零
            uint16_t high = data.back();
            char tmp[5];
            int n = 0;
            do
            {
                tmp[n++] = char('0' + high % 10);
                high /= 10;
            } while (high);
            while (n--)
            {
                *p++ = tmp[n];
            }
            // 其余 limb 查表，每 4 位
            size_t i = data.size() - 1;
            // OPT-3: AVX2 8× 展开, 一次输出 32 字节 = 8 个 limb
            // 用标量 store 直接写输出, 避免 _mm256_set_epi32 的 8 次 vmovd+vinserti128 开销
            // __builtin_prefetch: outTable.t[data[k]] is a data-dependent random gather
            //   into 40KB table — hardware prefetcher cannot predict. Explicit prefetch
            //   of next batch's entries overlaps L2 latency (~10 cycles) with current work.
#if defined(__AVX2__)
            while (i >= 8)
            {
                // Prefetch next batch's table entries (data[] is sequential, already in L1)
                if (i >= 16)
                {
                    HINT_PREFETCH(&outTable.t[data[i - 9]], 0, 0);
                    HINT_PREFETCH(&outTable.t[data[i - 10]], 0, 0);
                    HINT_PREFETCH(&outTable.t[data[i - 11]], 0, 0);
                    HINT_PREFETCH(&outTable.t[data[i - 12]], 0, 0);
                    HINT_PREFETCH(&outTable.t[data[i - 13]], 0, 0);
                    HINT_PREFETCH(&outTable.t[data[i - 14]], 0, 0);
                    HINT_PREFETCH(&outTable.t[data[i - 15]], 0, 0);
                    HINT_PREFETCH(&outTable.t[data[i - 16]], 0, 0);
                }
                uint32_t* p32 = reinterpret_cast<uint32_t*>(p);
                p32[0] = outTable.t[data[i - 1]];
                p32[1] = outTable.t[data[i - 2]];
                p32[2] = outTable.t[data[i - 3]];
                p32[3] = outTable.t[data[i - 4]];
                p32[4] = outTable.t[data[i - 5]];
                p32[5] = outTable.t[data[i - 6]];
                p32[6] = outTable.t[data[i - 7]];
                p32[7] = outTable.t[data[i - 8]];
                p += 32;
                i -= 8;
            }
#endif
            // OPT-2: 4× 标量 store, 处理剩余 >= 4 的部分
            while (i >= 4)
            {
                uint32_t* p32 = reinterpret_cast<uint32_t*>(p);
                p32[0] = outTable.t[data[i - 1]];
                p32[1] = outTable.t[data[i - 2]];
                p32[2] = outTable.t[data[i - 3]];
                p32[3] = outTable.t[data[i - 4]];
                p += 16;
                i -= 4;
            }
            // 标量尾处理
            while (i > 0)
            {
                i--;
                std::memcpy(p, &outTable.t[data[i]], 4);
                p += 4;
            }
            return size_t(p - out);
        }
        // 零拷贝读取 [start, end) 的数字字符
        void fromCharRange(const char* start, const char* end)
        {
            size_t len = end - start;
            if (len == 0)
            {
                clear();
                return;
            }
            const char* p_begin = start;
            if (*p_begin == '-')
            {
                sign = true;
                p_begin++;
                len--;
            }
            else
            {
                sign = false;
            }
            data.resize((len + BASE_DIGIT - 1) / BASE_DIGIT);
            size_t i = 0;
            // 优化: 16 字节主循环 (SSE2, 一次 4 limbs) + 8/4/1-3 字节回退
            // 边界安全: 循环条件 end-p_begin>=16 保证 _mm_loadu_si128(end-16,end)
            //           读取范围 [end-16, end) 完全落在 token 内, 不会越界 mmap 末页
            while (end - p_begin >= 16)
            {
                end -= 16;
                str16to4limbs(end, data.data() + i);
                i += 4;
            }
            // 8 字节回退 (一次 2 limbs) - 保留原代码
            while (end - p_begin >= 8)
            {
                end -= 8;
                data[i] = str4toi(end + 4);     // 低 4 位
                data[i + 1] = str4toi(end);     // 高 4 位
                i += 2;
            }
            // 处理剩余 4-7 字节
            while (end - p_begin >= 4)
            {
                end -= 4;
                data[i] = str4toi(end);
                i++;
            }
            // 处理剩余 1-3 字节 (最高位)
            if (end > p_begin)
            {
                data[i] = 0;
                while (end > p_begin)
                {
                    data[i] *= 10;
                    data[i] += p_begin[0] - '0';
                    p_begin++;
                }
                i++;
            }
            // 清零 resize 可能多分配的尾部
            while (i < data.size())
                data[i++] = 0;
            removeLeadingZero();
        }
        operator std::string() const
        {
            return toString();
        }
        void print() const
        {
            std::cout << toString();
        }
        friend std::istream &operator>>(std::istream &is, Integer &num)
        {
            static std::string tmp;
            tmp.clear();
            is >> tmp;
            num.fromString(tmp);
            return is;
        }
        friend std::ostream &operator<<(std::ostream &os, const Integer &num)
        {
            return os << num.toString();
        }
        static int absCompare(View input1, View input2)
        {
            size_t len1 = count_true_length(input1.ptr, input1.size);
            size_t len2 = count_true_length(input2.ptr, input2.size);
            if (len1 != len2)
            {
                return len1 > len2 ? 1 : -1;
            }
            size_t i = len1;
            while (i > 0)
            {
                i--;
                if (input1[i] != input2[i])
                {
                    return input1[i] > input2[i] ? 1 : -1;
                }
            }
            return 0;
        }
        // 双肢打包 + AVX2 版本 absAdd
        // 16 个 limb 一次处理 (8 个打包 uint32 = 1 个 __m256i)
        // 关键不变量: limb 最大 9999, 两 limb 和最大 19998 < 65536, 不会进位到高 16 位
#pragma GCC push_options
#pragma GCC target("avx2,fma,bmi,bmi2")
        static bool absAdd_avx2(View in1, View in2, Span out)
        {
            if (in1.size < in2.size)
            {
                std::swap(in1, in2);
            }
            size_t i = 0;
            // === D11: SWAR 进位链 (同 absSub, 消除 store-forwarding stall) ===
            // t=a+b, G=(t>=BASE) 生成, P=(t==BASE-1) 传播; A=G,B=G|P 使
            // gen=A&B=G, prop=A^B=P, 故 s=A+B+cy 一次算完 16 limb 的进位链。
            uint32_t carry32 = 0;
            const __m256i vbase = _mm256_set1_epi16(static_cast<short>(BASE));
            const __m256i vbasem1 = _mm256_set1_epi16(static_cast<short>(BASE - 1));
            const __m256i vbit = _mm256_setr_epi16(
                1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192,
                static_cast<short>(16384), static_cast<short>(32768));
            for (; i + 15 < in2.size; i += 16)
            {
                __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(in1.ptr + i));
                __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(in2.ptr + i));
                // t = a + b in [0, 2*BASE-2] < 32768 => signed 比较即 unsigned
                __m256i t = _mm256_add_epi16(a, b);
                __m256i gm = _mm256_cmpgt_epi16(t, vbasem1); // t >= BASE   (generate)
                __m256i pm = _mm256_cmpeq_epi16(t, vbasem1); // t == BASE-1 (propagate)
                uint32_t G = _pext_u32(static_cast<uint32_t>(_mm256_movemask_epi8(gm)), 0x55555555u);
                uint32_t P = _pext_u32(static_cast<uint32_t>(_mm256_movemask_epi8(pm)), 0x55555555u);
                uint32_t A = G, B = G | P;
                uint32_t s = A + B + carry32;
                uint32_t cin = s ^ A ^ B; // bit j = 进入 limb j 的进位; bit16 = 总进位
                uint32_t cout = cin >> 1; // bit j = limb j 的进位输出
                carry32 = (s >> 16) & 1u;
                __m256i vcin = _mm256_cmpeq_epi16(
                    _mm256_and_si256(_mm256_set1_epi16(static_cast<short>(cin)), vbit), vbit);
                __m256i vcout = _mm256_cmpeq_epi16(
                    _mm256_and_si256(_mm256_set1_epi16(static_cast<short>(cout)), vbit), vbit);
                // r = t + cin - (有进位输出处减 BASE)
                __m256i r = _mm256_sub_epi16(
                    _mm256_add_epi16(t, _mm256_srli_epi16(vcin, 15)),
                    _mm256_and_si256(vcout, vbase));
                _mm256_storeu_si256(reinterpret_cast<__m256i *>(out.ptr + i), r);
            }
            Limb carry = static_cast<Limb>(carry32);
            // 标量尾部 (8 路展开)
            for (; i + 7 < in2.size; i += 8)
            {
                out[i]   = add_half<Limb>(in1[i],   in2[i]   + carry, BASE, carry);
                out[i+1] = add_half<Limb>(in1[i+1], in2[i+1] + carry, BASE, carry);
                out[i+2] = add_half<Limb>(in1[i+2], in2[i+2] + carry, BASE, carry);
                out[i+3] = add_half<Limb>(in1[i+3], in2[i+3] + carry, BASE, carry);
                out[i+4] = add_half<Limb>(in1[i+4], in2[i+4] + carry, BASE, carry);
                out[i+5] = add_half<Limb>(in1[i+5], in2[i+5] + carry, BASE, carry);
                out[i+6] = add_half<Limb>(in1[i+6], in2[i+6] + carry, BASE, carry);
                out[i+7] = add_half<Limb>(in1[i+7], in2[i+7] + carry, BASE, carry);
            }
            for (; i < in2.size; i++)
            {
                out[i] = add_half<Limb>(in1[i], in2[i] + carry, BASE, carry);
            }
            // 第三段: carry=0 时用 memcpy 快速拷贝
            for (; i < in1.size; i++)
            {
                if (carry == 0)
                {
                    break;
                }
                out[i] = add_half<Limb>(in1[i], carry, BASE, carry);
            }
            if (carry == 0 && i < in1.size && out.ptr != in1.ptr)
            {
                std::memcpy(out.ptr + i, in1.ptr + i, (in1.size - i) * sizeof(Limb));
            }
            return carry;
        }
        // === D11: SWAR 借位链 absSub (替换原 store-forward-stall 版本) ===
        // 原版把 __m256i store 到 tmp[] 后立刻标量 4B 读、再 32B 读回,
        // 宽存窄读 + 窄存宽读两次 store-forwarding 全部失败 (~13cy/次, 每 16 limb 两次),
        // 向量化的只是廉价的 a+bias-b, 昂贵的借位链仍串行 => 实测比纯标量还慢 1.4x。
        //
        // 本版把借位链本身向量化:
        //   借位递推  C[j] = G[j] | (P[j] & C[j-1])   (G=生成: a<b, P=传播: a==b)
        //   与二进制加法进位链同构 —— 取 A=G, B=G|P 则 gen=A&B=G, prop=A^B=P (G,P 互斥),
        //   故一条 32 位整数加法 s=A+B+bin 即把 16 个 limb 的借位一次算完:
        //     cin = s ^ A ^ B   (bit j = 进入 limb j 的借位, bit16 = 总借出)
        //   全程无回写往返。实测 0.22 ns/limb, 较原版 4.9x。
        static bool absSub_avx2(View in1, View in2, Span out)
        {
            assert(in1.size >= in2.size);
            size_t i = 0;
            uint32_t borrow = 0;
            const __m256i vbase = _mm256_set1_epi16(static_cast<short>(BASE));
            const __m256i vbit = _mm256_setr_epi16(
                1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192,
                static_cast<short>(16384), static_cast<short>(32768));
            for (; i + 15 < in2.size; i += 16)
            {
                __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(in1.ptr + i));
                __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(in2.ptr + i));
                // t = a + BASE - b in [1, 2*BASE-1], 恒 < 32768 => signed 比较即 unsigned
                __m256i t = _mm256_sub_epi16(_mm256_add_epi16(a, vbase), b);
                __m256i gm = _mm256_cmpgt_epi16(vbase, t); // t <  BASE <=> a < b   (generate)
                __m256i pm = _mm256_cmpeq_epi16(t, vbase); // t == BASE <=> a == b  (propagate)
                uint32_t G = _pext_u32(static_cast<uint32_t>(_mm256_movemask_epi8(gm)), 0x55555555u);
                uint32_t P = _pext_u32(static_cast<uint32_t>(_mm256_movemask_epi8(pm)), 0x55555555u);
                uint32_t A = G, B = G | P;
                uint32_t s = A + B + borrow;
                uint32_t cin = s ^ A ^ B; // bit j = 进入 limb j 的借位; bit16 = 总借出
                uint32_t cout = cin >> 1; // bit j = limb j 的借出 (bit15 取自 cin.bit16)
                borrow = (s >> 16) & 1u;
                __m256i vcin = _mm256_cmpeq_epi16(
                    _mm256_and_si256(_mm256_set1_epi16(static_cast<short>(cin)), vbit), vbit);
                __m256i vcout = _mm256_cmpeq_epi16(
                    _mm256_and_si256(_mm256_set1_epi16(static_cast<short>(cout)), vbit), vbit);
                // r = t - cin - (无借出处再减 BASE)
                __m256i r = _mm256_sub_epi16(
                    _mm256_sub_epi16(t, _mm256_srli_epi16(vcin, 15)),
                    _mm256_andnot_si256(vcout, vbase));
                _mm256_storeu_si256(reinterpret_cast<__m256i *>(out.ptr + i), r);
            }
            Limb bw = static_cast<Limb>(borrow);
            // 标量尾部 (8 路展开)
            for (; i + 7 < in2.size; i += 8)
            {
                out[i]   = sub_half<Limb>(in1[i],   in2[i]   + bw, BASE, bw);
                out[i+1] = sub_half<Limb>(in1[i+1], in2[i+1] + bw, BASE, bw);
                out[i+2] = sub_half<Limb>(in1[i+2], in2[i+2] + bw, BASE, bw);
                out[i+3] = sub_half<Limb>(in1[i+3], in2[i+3] + bw, BASE, bw);
                out[i+4] = sub_half<Limb>(in1[i+4], in2[i+4] + bw, BASE, bw);
                out[i+5] = sub_half<Limb>(in1[i+5], in2[i+5] + bw, BASE, bw);
                out[i+6] = sub_half<Limb>(in1[i+6], in2[i+6] + bw, BASE, bw);
                out[i+7] = sub_half<Limb>(in1[i+7], in2[i+7] + bw, BASE, bw);
            }
            for (; i < in2.size; i++)
            {
                out[i] = sub_half<Limb>(in1[i], in2[i] + bw, BASE, bw);
            }
            // 高位残段: 无借位时直接搬运 (原地时连搬运都省)
            if (bw == 0)
            {
                if (out.ptr != in1.ptr && i < in1.size)
                    std::memcpy(out.ptr + i, in1.ptr + i, (in1.size - i) * sizeof(Limb));
                return false;
            }
            for (; i < in1.size; i++)
            {
                out[i] = sub_half<Limb>(in1[i], bw, BASE, bw);
                if (bw == 0)
                {
                    i++;
                    if (out.ptr != in1.ptr && i < in1.size)
                        std::memcpy(out.ptr + i, in1.ptr + i, (in1.size - i) * sizeof(Limb));
                    return false;
                }
            }
            return bw;
        }
#pragma GCC pop_options
        static bool absAdd(View in1, View in2, Span out)
        {
            if (in2.size >= 16)
            {
                return absAdd_avx2(in1, in2, out);
            }
            if (in1.size < in2.size)
            {
                std::swap(in1, in2);
            }
            size_t i = 0;
            Limb carry = 0;
            // 8 路展开 + add_half 无分支掩码 (消除除法, 对齐 best/add.cpp 策略)
            for (; i + 7 < in2.size; i += 8)
            {
                out[i]   = add_half<Limb>(in1[i],   in2[i]   + carry, BASE, carry);
                out[i+1] = add_half<Limb>(in1[i+1], in2[i+1] + carry, BASE, carry);
                out[i+2] = add_half<Limb>(in1[i+2], in2[i+2] + carry, BASE, carry);
                out[i+3] = add_half<Limb>(in1[i+3], in2[i+3] + carry, BASE, carry);
                out[i+4] = add_half<Limb>(in1[i+4], in2[i+4] + carry, BASE, carry);
                out[i+5] = add_half<Limb>(in1[i+5], in2[i+5] + carry, BASE, carry);
                out[i+6] = add_half<Limb>(in1[i+6], in2[i+6] + carry, BASE, carry);
                out[i+7] = add_half<Limb>(in1[i+7], in2[i+7] + carry, BASE, carry);
            }
            for (; i < in2.size; i++)
            {
                out[i] = add_half<Limb>(in1[i], in2[i] + carry, BASE, carry);
            }
            // 第三段: carry=0 时用 memcpy 快速拷贝 (避免逐位 add_half 开销)
            for (; i < in1.size; i++)
            {
                if (carry == 0)
                {
                    break;
                }
                out[i] = add_half<Limb>(in1[i], carry, BASE, carry);
            }
            if (carry == 0 && i < in1.size && out.ptr != in1.ptr)
            {
                std::memcpy(out.ptr + i, in1.ptr + i, (in1.size - i) * sizeof(Limb));
            }
            return carry;
        }
        static bool absSub(View in1, View in2, Span out)
        {
            if (in2.size >= 16)
            {
                return absSub_avx2(in1, in2, out);
            }
            assert(in1.size >= in2.size);
            size_t i = 0;
            Limb borrow = 0;
            for (; i + 7 < in2.size; i += 8)
            {
                out[i]   = sub_half<Limb>(in1[i],   in2[i]   + borrow, BASE, borrow);
                out[i+1] = sub_half<Limb>(in1[i+1], in2[i+1] + borrow, BASE, borrow);
                out[i+2] = sub_half<Limb>(in1[i+2], in2[i+2] + borrow, BASE, borrow);
                out[i+3] = sub_half<Limb>(in1[i+3], in2[i+3] + borrow, BASE, borrow);
                out[i+4] = sub_half<Limb>(in1[i+4], in2[i+4] + borrow, BASE, borrow);
                out[i+5] = sub_half<Limb>(in1[i+5], in2[i+5] + borrow, BASE, borrow);
                out[i+6] = sub_half<Limb>(in1[i+6], in2[i+6] + borrow, BASE, borrow);
                out[i+7] = sub_half<Limb>(in1[i+7], in2[i+7] + borrow, BASE, borrow);
            }
            for (; i < in2.size; i++)
            {
                out[i] = sub_half<Limb>(in1[i], in2[i] + borrow, BASE, borrow);
            }
            for (; i < in1.size; i++)
            {
                out[i] = sub_half<Limb>(in1[i], borrow, BASE, borrow);
            }
            return borrow;
        }
        static bool absAdd1(View in1, Limb in2, Span out)
        {
            assert(in1.size > 0);
            Limb carry = 0;
            out[0] = add_half<Limb>(in1[0], in2, BASE, carry);
            for (size_t i = 1; i < in1.size; i++)
            {
                if (carry == 0)
                {
                    // in-place 时后续 out[i] 已等于 in1[i], 直接返回
                    if (in1.ptr == out.ptr) return false;
                    // 非 in-place 时复制剩余部分
                    std::copy(in1.ptr + i, in1.ptr + in1.size, out.ptr + i);
                    return false;
                }
                out[i] = add_half<Limb>(in1[i], carry, BASE, carry);
            }
            return carry;
        }
        static bool absSub1(View in1, Limb in2, Span out)
        {
            assert(in1.size > 0);
            Limb borrow = 0;
            out[0] = sub_half<Limb>(in1[0], in2, BASE, borrow);
            for (size_t i = 1; i < in1.size; i++)
            {
                if (borrow == 0)
                {
                    if (in1.ptr == out.ptr) return false;
                    std::copy(in1.ptr + i, in1.ptr + in1.size, out.ptr + i);
                    return false;
                }
                out[i] = sub_half<Limb>(in1[i], borrow, BASE, borrow);
            }
            return borrow;
        }
        friend bool operator>(const Integer &input1, const Integer &input2)
        {
            if (input1.isNeg() != input2.isNeg())
            {
                return input2.isNeg();
            }
            return (absCompare(input1.getView(), input2.getView()) > 0) != input1.isNeg();
        }
        friend bool operator<(const Integer &input1, const Integer &input2)
        {
            return input2 > input1;
        }
        friend bool operator>=(const Integer &input1, const Integer &input2)
        {
            return !(input1 < input2);
        }
        friend bool operator<=(const Integer &input1, const Integer &input2)
        {
            return !(input1 > input2);
        }
        friend bool operator==(const Integer &input1, const Integer &input2)
        {
            if (input1.isNeg() != input2.isNeg())
            {
                return false;
            }
            return absCompare(input1.getView(), input2.getView()) == 0;
        }
        friend bool operator!=(const Integer &input1, const Integer &input2)
        {
            return !(input1 == input2);
        }
        Integer &add(View input, bool in_sign)
        {
            bool same = input.ptr == this->getView().ptr;
            size_t len1 = this->length(), len2 = input.size;
            if (this->isNeg() == in_sign) 
            {
                size_t add_len = std::max(len1, len2) + 1;
                this->data.resize(add_len);
                auto view1 = View(this->data.data(), len1), view2 = input;
                if (same)
                {
                    view2 = view1;
                }
                this->data[add_len - 1] = absAdd(view1, view2, this->getSpan());
            }
            else
            {
                if (same)
                {
                    return *this = Integer{};
                }
                size_t sub_len = std::max(len1, len2);
                this->data.resize(sub_len);
                auto view1 = View(this->data.data(), len1), view2 = input;
                int cmp = absCompare(view1, view2);
                if (cmp > 0)
                {
                    
                    absSub(view1, view2, this->getSpan());
                }
                else if (cmp < 0)
                {
                    this->setSign(in_sign);
                    absSub(view2, view1, this->getSpan());
                }
                else
                {
                    this->data.clear();
                }
            }
            this->removeLeadingZero();
            return *this;
        }
        static void basicMul(View in1, View in2, Span out)
        {
            if (in1.size > in2.size)
            {
                std::swap(in1, in2);
            }
            if (in1.size == 0)
            {
                return;
            }
            
            
            thread_local std::vector<Limb> buf;
            size_t buf_size = in1.size + in2.size;
            if (buf.size() < buf_size)
                buf.resize(buf_size);
            Limb carry = 0, x = in1[0];
            for (size_t j = 0; j < in2.size; j++)
            {
                Limb2 prod = Limb2(in2[j]) * x + carry;
                buf[j] = prod % BASE;
                carry = prod / BASE;
            }
            buf[in2.size] = carry;
            for (size_t i = 1; i < in1.size; i++)
            {
                x = in1[i], carry = 0;
                for (size_t j = 0; j < in2.size; j++)
                {
                    Limb2 prod = Limb2(in2[j]) * x + carry + buf[i + j];
                    buf[i + j] = prod % BASE;
                    carry = prod / BASE;
                }
                buf[i + in2.size] = carry;
            }
            std::copy(buf.begin(), buf.begin() + buf_size, out.begin());
        }
        static void fftMul(View in1, View in2, Span out)
        {
            size_t len1 = count_true_length(in1.ptr, in1.size);
            size_t len2 = count_true_length(in2.ptr, in2.size);
            if (len1 == 0 || len2 == 0)
            {
                std::fill_n(out.ptr, out.size, Limb(0));
                return;
            }
            size_t conv_len = len1 + len2 - 1, float_len = fft_ceil_lin(conv_len);
            HINT_ASSUME(float_len >= conv_len);
            
            thread_local AlignedVec32<double> tv1, tv2;
            if (tv1.size() < float_len)
                tv1.resize(float_len);
            if (tv2.size() < float_len)
                tv2.resize(float_len);
            double *v1 = tv1.data(), *v2 = tv2.data();
            copyU16ToF64AndFill(in1.ptr, v1, len1, float_len);
            copyU16ToF64AndFill(in2.ptr, v2, len2, float_len);
#ifdef PROFILE_MUL
            auto _t_fft0 = std::chrono::high_resolution_clock::now();
#endif
            transform::fft::real_conv(v1, v2, float_len);
#ifdef PROFILE_MUL
            auto _t_carry0 = std::chrono::high_resolution_clock::now();
#endif
            uint64_t carry = 0;
            size_t i = 0;
            for (; i + 7 < conv_len; i += 8)
            {
                // __builtin_prefetch: v1[] can be 4MB (500k MUL), exceeds L2 (1MB/core).
                //   Prefetch 2 batches ahead (128 bytes) to overlap L3 latency (~40 cycles)
                //   with carry chain serial dependency (~24 cycles per 8× iteration).
                HINT_PREFETCH(v1 + i + 16, 0, 0);
                HINT_PREFETCH(v1 + i + 24, 0, 0);
                // Barrett: q=divBASE(s); out=s-q*BASE; next_s=q+v[i+1]
                // was: s1 = s0 / BASE + ...; out[i] = s0 % BASE; carry = s7 / BASE;
                uint64_t s0 = carry + uint64_t(v1[i]   + 0.5);
                uint64_t q0 = divBASE(s0);
                uint64_t s1 = q0 + uint64_t(v1[i+1] + 0.5);
                uint64_t q1 = divBASE(s1);
                uint64_t s2 = q1 + uint64_t(v1[i+2] + 0.5);
                uint64_t q2 = divBASE(s2);
                uint64_t s3 = q2 + uint64_t(v1[i+3] + 0.5);
                uint64_t q3 = divBASE(s3);
                uint64_t s4 = q3 + uint64_t(v1[i+4] + 0.5);
                uint64_t q4 = divBASE(s4);
                uint64_t s5 = q4 + uint64_t(v1[i+5] + 0.5);
                uint64_t q5 = divBASE(s5);
                uint64_t s6 = q5 + uint64_t(v1[i+6] + 0.5);
                uint64_t q6 = divBASE(s6);
                uint64_t s7 = q6 + uint64_t(v1[i+7] + 0.5);
                uint64_t q7 = divBASE(s7);
                out[i]   = Limb(s0 - q0 * BASE);
                out[i+1] = Limb(s1 - q1 * BASE);
                out[i+2] = Limb(s2 - q2 * BASE);
                out[i+3] = Limb(s3 - q3 * BASE);
                out[i+4] = Limb(s4 - q4 * BASE);
                out[i+5] = Limb(s5 - q5 * BASE);
                out[i+6] = Limb(s6 - q6 * BASE);
                out[i+7] = Limb(s7 - q7 * BASE);
                carry = q7;
            }
            for (; i < conv_len; i++)
            {
                carry += uint64_t(v1[i] + 0.5);
                uint64_t q = divBASE(carry);
                out[i] = Limb(carry - q * BASE);
                carry = q;
            }
            out[conv_len] = Limb(carry);

            if (out.size > conv_len + 1)
            {
                std::fill_n(out.ptr + conv_len + 1, out.size - conv_len - 1, Limb(0));
            }
#ifdef PROFILE_MUL
            auto _t_carry1 = std::chrono::high_resolution_clock::now();
            double _dt_fft = std::chrono::duration<double, std::milli>(_t_carry0 - _t_fft0).count();
            double _dt_carry = std::chrono::duration<double, std::milli>(_t_carry1 - _t_carry0).count();
            fprintf(stderr, "  [prof] fftMul: FFT=%.3fms CARRY=%.3fms conv_len=%zu fl=%zu\n", _dt_fft, _dt_carry, conv_len, float_len);
#endif
        }
        static void fftSqr(View in, Span out)
        {
            size_t len = count_true_length(in.ptr, in.size);
            if (len == 0)
            {
                std::fill_n(out.ptr, out.size, Limb(0));
                return;
            }
            size_t conv_len = len * 2 - 1, float_len = fft_ceil_lin(conv_len);
            HINT_ASSUME(float_len >= conv_len);
            
            thread_local AlignedVec32<double> tv;
            if (tv.size() < float_len)
                tv.resize(float_len);
            double *v = tv.data();
            copyU16ToF64AndFill(in.ptr, v, len, float_len);
            transform::fft::real_conv(v, v, float_len);
            uint64_t carry = 0;
            size_t i = 0;
            for (; i + 7 < conv_len; i += 8)
            {
                // __builtin_prefetch: overlap L3 latency with carry chain computation
                HINT_PREFETCH(v + i + 16, 0, 0);
                HINT_PREFETCH(v + i + 24, 0, 0);
                // Barrett: q=divBASE(s); out=s-q*BASE; next_s=q+v[i+1]
                // was: s1 = s0 / BASE + ...; out[i] = s0 % BASE; carry = s7 / BASE;
                uint64_t s0 = carry + uint64_t(v[i]   + 0.5);
                uint64_t q0 = divBASE(s0);
                uint64_t s1 = q0 + uint64_t(v[i+1] + 0.5);
                uint64_t q1 = divBASE(s1);
                uint64_t s2 = q1 + uint64_t(v[i+2] + 0.5);
                uint64_t q2 = divBASE(s2);
                uint64_t s3 = q2 + uint64_t(v[i+3] + 0.5);
                uint64_t q3 = divBASE(s3);
                uint64_t s4 = q3 + uint64_t(v[i+4] + 0.5);
                uint64_t q4 = divBASE(s4);
                uint64_t s5 = q4 + uint64_t(v[i+5] + 0.5);
                uint64_t q5 = divBASE(s5);
                uint64_t s6 = q5 + uint64_t(v[i+6] + 0.5);
                uint64_t q6 = divBASE(s6);
                uint64_t s7 = q6 + uint64_t(v[i+7] + 0.5);
                uint64_t q7 = divBASE(s7);
                out[i]   = Limb(s0 - q0 * BASE);
                out[i+1] = Limb(s1 - q1 * BASE);
                out[i+2] = Limb(s2 - q2 * BASE);
                out[i+3] = Limb(s3 - q3 * BASE);
                out[i+4] = Limb(s4 - q4 * BASE);
                out[i+5] = Limb(s5 - q5 * BASE);
                out[i+6] = Limb(s6 - q6 * BASE);
                out[i+7] = Limb(s7 - q7 * BASE);
                carry = q7;
            }
            for (; i < conv_len; i++)
            {
                carry += uint64_t(v[i] + 0.5);
                uint64_t q = divBASE(carry);
                out[i] = Limb(carry - q * BASE);
                carry = q;
            }
            out[conv_len] = Limb(carry);

            if (out.size > conv_len + 1)
            {
                std::fill_n(out.ptr + conv_len + 1, out.size - conv_len - 1, Limb(0));
            }
        }

#ifndef FFT_SQR_THRESHOLD
#define FFT_SQR_THRESHOLD 64
#endif
#ifndef FFT_MUL_THRESHOLD
#define FFT_MUL_THRESHOLD 64
#endif
#ifndef FFT_MUL_UNBALANCED_MIN
#define FFT_MUL_UNBALANCED_MIN 16384
#endif
#ifndef FFT_MUL_UNBALANCED_RATIO
#define FFT_MUL_UNBALANCED_RATIO 6
#endif
        static void absSqr(View in, Span out)
        {
            assert(out.size >= in.size * 2);
            if (in.size <= FFT_SQR_THRESHOLD)
            {
                basicMul(in, in, out);
            }
            else
            {
                fftSqr(in, out);
            }
        }
        static void fftMulUnbalanced(View large, View small, Span out)
        {
            size_t len_big = count_true_length(large.ptr, large.size);
            size_t len_sml = count_true_length(small.ptr, small.size);
            if (len_big == 0 || len_sml == 0)
            {
                std::fill_n(out.ptr, out.size, Limb(0));
                return;
            }
            size_t chunk = small.size;
            size_t n_chunks = (len_big + chunk - 1) / chunk;
            size_t float_len = fft_ceil_lin(chunk + chunk - 1);
            
            thread_local AlignedVec32<double> b_dft;
            if (b_dft.size() < float_len) b_dft.resize(float_len);
            prepareDFT(small, b_dft.data(), float_len);
            
            // 保存大数数据副本（large.ptr 可能和 out.ptr 指向同一缓冲区）
            thread_local std::vector<Limb> large_copy;
            if (large_copy.size() < len_big) large_copy.resize(len_big);
            std::copy_n(large.ptr, len_big, large_copy.data());
            
            std::fill_n(out.ptr, out.size, Limb(0));
            
            thread_local std::vector<Limb> tbuf;
            size_t tbuf_max = chunk + chunk;
            if (tbuf.size() < tbuf_max) tbuf.resize(tbuf_max);
            
            for (size_t ci = 0; ci < n_chunks; ci++)
            {
                size_t offset = ci * chunk;
                size_t this_chunk = std::min(chunk, len_big - offset);
                size_t this_conv = this_chunk + chunk - 1;
                
                View chunk_view(large_copy.data() + offset, this_chunk);
                Span temp_span(tbuf.data(), this_conv + 1);
                fftMulPre(chunk_view, b_dft.data(), chunk, float_len, temp_span);
                
                uint64_t carry = 0;
                size_t wpos = offset;
                for (size_t i = 0; i < this_conv; i++)
                {
                    carry += uint64_t(out[wpos]) + tbuf[i];
                    out[wpos] = Limb(carry % BASE);
                    carry /= BASE;
                    wpos++;
                }
                carry += tbuf[this_conv];
                while (carry > 0 && wpos < out.size)
                {
                    carry += out[wpos];
                    out[wpos] = Limb(carry % BASE);
                    carry /= BASE;
                    wpos++;
                }
            }
        }
        static void absMul(View in1, View in2, Span out)
        {
            assert(out.size >= in1.size + in2.size);
            if (in1.ptr == in2.ptr)
            {
                absSqr(in1, out);
                return;
            }
            size_t sml = std::min(in1.size, in2.size);
            if (sml <= FFT_MUL_THRESHOLD)
            {
                basicMul(in1, in2, out);
            }
            else
            {
                size_t big = std::max(in1.size, in2.size);
                if (sml >= FFT_MUL_UNBALANCED_MIN && big >= sml * FFT_MUL_UNBALANCED_RATIO)
                {
                    if (in1.size >= in2.size)
                        fftMulUnbalanced(in1, in2, out);
                    else
                        fftMulUnbalanced(in2, in1, out);
                }
                else
                {
                    fftMul(in1, in2, out);
                }
            }
        }
        
        
        static void prepareDFT(View in, double *dft_buf, size_t float_len)
        {
            assert(is_2pow(float_len) || is_fft3(float_len));
            assert(float_len >= in.size);
            copyU16ToF64AndFill(in.begin(), dft_buf, in.size, float_len);
            transform::fft::rdif(dft_buf, float_len);
        }
        
        
        static void fftMulPre(View a, const double *b_dft, size_t b_len, size_t float_len, Span out)
        {
            size_t a_len = count_true_length(a.ptr, a.size);
            if (a_len == 0)
            {
                std::fill_n(out.ptr, out.size, Limb(0));
                return;
            }
            size_t conv_len = a_len + b_len - 1;
            assert(float_len >= conv_len);
            HINT_ASSUME(float_len >= conv_len);
            thread_local AlignedVec32<double> tv;
            if (tv.size() < float_len)
                tv.resize(float_len);
            double *v = tv.data();
            copyU16ToF64AndFill(a.ptr, v, a_len, float_len);
            transform::fft::rdif(v, float_len);
            transform::fft::rdot(v, b_dft, float_len);
            transform::fft::ridit(v, float_len);
            uint64_t carry = 0;
            size_t i = 0;
            for (; i + 7 < conv_len; i += 8)
            {
                // __builtin_prefetch: overlap L3 latency with carry chain computation
                HINT_PREFETCH(v + i + 16, 0, 0);
                HINT_PREFETCH(v + i + 24, 0, 0);
                // Barrett: q=divBASE(s); out=s-q*BASE; next_s=q+v[i+1]
                // was: s1 = s0 / BASE + ...; out[i] = s0 % BASE; carry = s7 / BASE;
                uint64_t s0 = carry + uint64_t(v[i]   + 0.5);
                uint64_t q0 = divBASE(s0);
                uint64_t s1 = q0 + uint64_t(v[i+1] + 0.5);
                uint64_t q1 = divBASE(s1);
                uint64_t s2 = q1 + uint64_t(v[i+2] + 0.5);
                uint64_t q2 = divBASE(s2);
                uint64_t s3 = q2 + uint64_t(v[i+3] + 0.5);
                uint64_t q3 = divBASE(s3);
                uint64_t s4 = q3 + uint64_t(v[i+4] + 0.5);
                uint64_t q4 = divBASE(s4);
                uint64_t s5 = q4 + uint64_t(v[i+5] + 0.5);
                uint64_t q5 = divBASE(s5);
                uint64_t s6 = q5 + uint64_t(v[i+6] + 0.5);
                uint64_t q6 = divBASE(s6);
                uint64_t s7 = q6 + uint64_t(v[i+7] + 0.5);
                uint64_t q7 = divBASE(s7);
                out[i]   = Limb(s0 - q0 * BASE);
                out[i+1] = Limb(s1 - q1 * BASE);
                out[i+2] = Limb(s2 - q2 * BASE);
                out[i+3] = Limb(s3 - q3 * BASE);
                out[i+4] = Limb(s4 - q4 * BASE);
                out[i+5] = Limb(s5 - q5 * BASE);
                out[i+6] = Limb(s6 - q6 * BASE);
                out[i+7] = Limb(s7 - q7 * BASE);
                carry = q7;
            }
            for (; i < conv_len; i++)
            {
                carry += uint64_t(v[i] + 0.5);
                uint64_t q = divBASE(carry);
                out[i] = Limb(carry - q * BASE);
                carry = q;
            }
            out[conv_len] = Limb(carry);

            if (out.size > conv_len + 1)
            {
                std::fill_n(out.ptr + conv_len + 1, out.size - conv_len - 1, Limb(0));
            }
        }

        // fftMulModBm1: 计算 a * b mod (B^m - 1), 结果长度 m (cyclic convolution)
        // m 必须是合法 FFT 长度 (2^k 或 3*2^k), 且 a_len <= m, b_len <= m
        //   模数 B^m-1 只要求 m 落在调用方的整数区间内, 对 m 的因子分解无要求;
        //   放开 3*2^k 后 mn=fft_ceil(k+1) 最坏 1.5k (原 int_ceil2 最坏 2k) -> cyclic 死区消失.
        // 用途: absInvNewton GMP 风格两步分解, FFT 长度从 ceil2(2m-1) 降到 m
        // 算法: cyclic FFT (float_len=m) + 进位传播 + cyclic carry 折叠 (B^m ≡ 1 mod B^m-1)
        static void fftMulModBm1(View a, View b, size_t m, Span out)
        {
            assert(is_2pow(m) || is_fft3(m));
            assert(out.size >= m);
            size_t a_len = count_true_length(a.ptr, a.size);
            size_t b_len = count_true_length(b.ptr, b.size);
            if (a_len == 0 || b_len == 0)
            {
                std::fill_n(out.ptr, m, Limb(0));
                if (out.size > m)
                    std::fill_n(out.ptr + m, out.size - m, Limb(0));
                return;
            }
            assert(a_len <= m && b_len <= m);

            thread_local AlignedVec32<double> tv;
            if (tv.size() < 2 * m)
                tv.resize(2 * m);
            double *va = tv.data();
            double *vb = tv.data() + m;

            copyU16ToF64AndFill(a.ptr, va, a_len, m);
            copyU16ToF64AndFill(b.ptr, vb, b_len, m);

            transform::fft::rdif(va, m);
            transform::fft::rdif(vb, m);
            transform::fft::rdot(va, vb, m);
            transform::fft::ridit(va, m);

            // 进位传播 (8 路展开, 同 fftMulPre)
            uint64_t carry = 0;
            size_t i = 0;
            for (; i + 7 < m; i += 8)
            {
                uint64_t s0 = carry + uint64_t(va[i]   + 0.5);
                uint64_t q0 = divBASE(s0);
                uint64_t s1 = q0 + uint64_t(va[i+1] + 0.5);
                uint64_t q1 = divBASE(s1);
                uint64_t s2 = q1 + uint64_t(va[i+2] + 0.5);
                uint64_t q2 = divBASE(s2);
                uint64_t s3 = q2 + uint64_t(va[i+3] + 0.5);
                uint64_t q3 = divBASE(s3);
                uint64_t s4 = q3 + uint64_t(va[i+4] + 0.5);
                uint64_t q4 = divBASE(s4);
                uint64_t s5 = q4 + uint64_t(va[i+5] + 0.5);
                uint64_t q5 = divBASE(s5);
                uint64_t s6 = q5 + uint64_t(va[i+6] + 0.5);
                uint64_t q6 = divBASE(s6);
                uint64_t s7 = q6 + uint64_t(va[i+7] + 0.5);
                uint64_t q7 = divBASE(s7);
                out[i]   = Limb(s0 - q0 * BASE);
                out[i+1] = Limb(s1 - q1 * BASE);
                out[i+2] = Limb(s2 - q2 * BASE);
                out[i+3] = Limb(s3 - q3 * BASE);
                out[i+4] = Limb(s4 - q4 * BASE);
                out[i+5] = Limb(s5 - q5 * BASE);
                out[i+6] = Limb(s6 - q6 * BASE);
                out[i+7] = Limb(s7 - q7 * BASE);
                carry = q7;
            }
            for (; i < m; i++)
            {
                carry += uint64_t(va[i] + 0.5);
                uint64_t q = divBASE(carry);
                out[i] = Limb(carry - q * BASE);
                carry = q;
            }

            // cyclic 折叠: B^m ≡ 1 (mod B^m-1), 所以 out_final = (out + carry) mod (B^m-1)
            // 进位传播 carry 到 out, 最多绕 1 圈 (carry 衰减极快)
            while (carry > 0)
            {
                bool wrapped = true;
                for (size_t j = 0; j < m && carry > 0; j++)
                {
                    uint64_t s = uint64_t(out[j]) + carry;
                    uint64_t q = divBASE(s);
                    out[j] = Limb(s - q * BASE);
                    carry = q;
                    if (carry == 0) { wrapped = false; break; }
                }
                // 若绕完整一圈且 carry 仍 > 0, 说明 sum(out) + carry >= B^m
                // (sum(out) + carry) mod (B^m-1) = (sum(out_new) + carry*B^m) mod (B^m-1)
                //   = (sum(out_new) + carry) mod (B^m-1)   [因为 B^m ≡ 1]
                // 实际中 carry 经第一轮后 < BASE, 第二轮必收敛, 最多 2 轮
                if (wrapped && carry > 0)
                {
                    // carry 应 = 1 (sum(out_old)+carry_initial < 2*B^m)
                    // 结果 = out_new + 1 (mod B^m-1), 不是 0!
                    // 旧代码填 0 是 bug: B^m mod (B^m-1) = 1, 不是 0
                    if (carry == 1)
                    {
                        // out += 1, cyclic carry (若全 BASE-1, 结果 = 0)
                        Limb c = 1;
                        for (size_t j = 0; j < m; j++)
                        {
                            if (out[j] + c < BASE) { out[j] = Limb(out[j] + c); c = 0; break; }
                            else { out[j] = Limb(out[j] + c - BASE); }
                        }
                        carry = 0;
                    }
                    // carry > 1 理论上不会发生, 但防御性继续 while
                }
            }

            if (out.size > m)
                std::fill_n(out.ptr + m, out.size - m, Limb(0));
        }

        // fftMulModBm1Pre: fftMulModBm1 的预计算 DFT 版本 (b 的 DFT 预先计算)
        // b_dft 长度 = m, 由 prepareDFT(b, b_dft, m) 预计算
        static void fftMulModBm1Pre(View a, const double *b_dft, size_t b_len, size_t m, Span out)
        {
            assert(is_2pow(m) || is_fft3(m));
            assert(out.size >= m);
            size_t a_len = count_true_length(a.ptr, a.size);
            if (a_len == 0)
            {
                std::fill_n(out.ptr, m, Limb(0));
                if (out.size > m)
                    std::fill_n(out.ptr + m, out.size - m, Limb(0));
                return;
            }
            assert(a_len <= m);

            thread_local AlignedVec32<double> tv;
            if (tv.size() < m)
                tv.resize(m);
            double *v = tv.data();

            copyU16ToF64AndFill(a.ptr, v, a_len, m);

            transform::fft::rdif(v, m);
            transform::fft::rdot(v, b_dft, m);
            transform::fft::ridit(v, m);

            // 进位传播 (cyclic mod B^m-1) — 同 fftMulModBm1
            uint64_t carry = 0;
            size_t i = 0;
            for (; i + 7 < m; i += 8)
            {
                // __builtin_prefetch: overlap L3 latency with carry chain computation
                HINT_PREFETCH(v + i + 16, 0, 0);
                HINT_PREFETCH(v + i + 24, 0, 0);
                uint64_t s0 = carry + uint64_t(v[i]   + 0.5);
                uint64_t q0 = divBASE(s0);
                uint64_t s1 = q0 + uint64_t(v[i+1] + 0.5);
                uint64_t q1 = divBASE(s1);
                uint64_t s2 = q1 + uint64_t(v[i+2] + 0.5);
                uint64_t q2 = divBASE(s2);
                uint64_t s3 = q2 + uint64_t(v[i+3] + 0.5);
                uint64_t q3 = divBASE(s3);
                uint64_t s4 = q3 + uint64_t(v[i+4] + 0.5);
                uint64_t q4 = divBASE(s4);
                uint64_t s5 = q4 + uint64_t(v[i+5] + 0.5);
                uint64_t q5 = divBASE(s5);
                uint64_t s6 = q5 + uint64_t(v[i+6] + 0.5);
                uint64_t q6 = divBASE(s6);
                uint64_t s7 = q6 + uint64_t(v[i+7] + 0.5);
                uint64_t q7 = divBASE(s7);
                out[i]   = Limb(s0 - q0 * BASE);
                out[i+1] = Limb(s1 - q1 * BASE);
                out[i+2] = Limb(s2 - q2 * BASE);
                out[i+3] = Limb(s3 - q3 * BASE);
                out[i+4] = Limb(s4 - q4 * BASE);
                out[i+5] = Limb(s5 - q5 * BASE);
                out[i+6] = Limb(s6 - q6 * BASE);
                out[i+7] = Limb(s7 - q7 * BASE);
                carry = q7;
            }
            for (; i < m; i++)
            {
                carry += uint64_t(v[i] + 0.5);
                uint64_t q = divBASE(carry);
                out[i] = Limb(carry - q * BASE);
                carry = q;
            }
            while (carry > 0)
            {
                bool wrapped = true;
                for (size_t j = 0; j < m && carry > 0; j++)
                {
                    uint64_t s = uint64_t(out[j]) + carry;
                    uint64_t q = divBASE(s);
                    out[j] = Limb(s - q * BASE);
                    carry = q;
                    if (carry == 0) { wrapped = false; break; }
                }
                if (wrapped && carry > 0)
                {
                    if (carry == 1)
                    {
                        Limb c = 1;
                        for (size_t j = 0; j < m; j++)
                        {
                            if (out[j] + c < BASE) { out[j] = Limb(out[j] + c); c = 0; break; }
                            else { out[j] = Limb(out[j] + c - BASE); }
                        }
                        carry = 0;
                    }
                }
            }

            if (out.size > m)
                std::fill_n(out.ptr + m, out.size - m, Limb(0));
        }
        Integer &square()
        {
            this->setSign(false);
            size_t len = this->length();
            this->data.resize(len * 2);
            absSqr(View(this->data.data(), len), this->getSpan());
            this->removeLeadingZero();
            return *this;
        }
        // === D19: SWAR 进位链 absMul1 (single-limb multiply) ===
        // 原版是纯标量进位链: 每 limb 一次 imul + 两次 magic 除法 (~26 Ir/limb),
        // 在 lri_03 的归一化路径 (dividend/divisor 各乘 factor, 共 50 万 limb)
        // 折算 13.0M Ir, 是画像里最大的 100% 标量块。
        //
        // 拆解串行依赖:  prod[i] = p[i] + carry[i],  p[i] = in[i]*x  (< BASE^2, 无依赖)
        //   记 p[i] = q[i]*BASE + r[i], 则
        //     out[i]     = (r[i] + carry[i]) mod BASE
        //     carry[i+1] = q[i] + [r[i] + carry[i] >= BASE]
        //   由 q[i] <= BASE-2 保证 carry[i] <= BASE-1, 故 r[i]+carry[i] < 2*BASE,
        //   涟漪进位至多 1 —— 于是 t[i] = r[i] + q[i-1] 可整体向量化,
        //   只剩下这条至多-1 的涟漪链, 用 D11 的 SWAR 一次算完 16 limb:
        //     G = (t >= BASE) 生成, P = (t == BASE-1) 传播 (二者互斥),
        //     s = G + (G|P) + cin,  cin_bits = s ^ G ^ (G|P),  cout = cin_bits >> 1
        static Limb absMul1(View in, Limb x, Span out)
        {
            static_assert(sizeof(Limb) == 2 && BASE == 10000,
                          "absMul1 AVX2 path assumes uint16 limbs in base 1e4");
            size_t i = 0;
            uint32_t carry_q = 0;   // 上一 limb 的高位 q
            uint32_t carry_rip = 0; // 上一 limb 的涟漪进位
            if (in.size >= 16)
            {
                const __m256i vx = _mm256_set1_epi32(int(uint32_t(x)));
                const __m256i vmagic = _mm256_set1_epi32(109951163); // ceil(2^40 / 1e4)
                const __m256i vbase32 = _mm256_set1_epi32(int(BASE));
                const __m256i vbase16 = _mm256_set1_epi16(short(BASE));
                const __m256i vbm1 = _mm256_set1_epi16(short(BASE - 1));
                const __m256i vbit = _mm256_setr_epi16(
                    1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192,
                    short(16384), short(32768));
                for (; i + 15 < in.size; i += 16)
                {
                    __m256i a16 = _mm256_loadu_si256(
                        reinterpret_cast<const __m256i *>(in.ptr + i));
                    __m256i a0 = _mm256_cvtepu16_epi32(_mm256_castsi256_si128(a16));
                    __m256i a1 = _mm256_cvtepu16_epi32(_mm256_extracti128_si256(a16, 1));
                    // p < (BASE-1)^2 < 1e8, 32 位无溢出
                    __m256i p0 = _mm256_mullo_epi32(a0, vx);
                    __m256i p1 = _mm256_mullo_epi32(a1, vx);
                    // q = floor(p * ceil(2^40/BASE) / 2^40) —— 对 p < 1e8 精确
                    auto divb = [&](__m256i p) -> __m256i
                    {
                        __m256i ev = _mm256_mul_epu32(p, vmagic);
                        __m256i od = _mm256_mul_epu32(_mm256_srli_epi64(p, 32), vmagic);
                        ev = _mm256_srli_epi64(ev, 40);
                        od = _mm256_srli_epi64(od, 40);
                        return _mm256_blend_epi32(ev, _mm256_slli_epi64(od, 32), 0xAA);
                    };
                    __m256i q0 = divb(p0), q1 = divb(p1);
                    __m256i r0 = _mm256_sub_epi32(p0, _mm256_mullo_epi32(q0, vbase32));
                    __m256i r1 = _mm256_sub_epi32(p1, _mm256_mullo_epi32(q1, vbase32));
                    // 32 -> 16 位打包 (q, r 均在 [0, BASE), 不会饱和)
                    __m256i q16 = _mm256_permute4x64_epi64(
                        _mm256_packus_epi32(q0, q1), 0xD8);
                    __m256i r16 = _mm256_permute4x64_epi64(
                        _mm256_packus_epi32(r0, r1), 0xD8);
                    // qsh[j] = q[j-1], 最低位填入上一块残留的 carry_q
                    __m256i qsh = _mm256_alignr_epi8(
                        q16, _mm256_permute2x128_si256(q16, q16, 0x08), 14);
                    qsh = _mm256_insert_epi16(qsh, int(carry_q), 0);
                    carry_q = uint32_t(uint16_t(_mm256_extract_epi16(q16, 15)));
                    __m256i t = _mm256_add_epi16(r16, qsh); // < 2*BASE < 32768
                    __m256i gm = _mm256_cmpgt_epi16(t, vbm1); // t >= BASE
                    __m256i pm = _mm256_cmpeq_epi16(t, vbm1); // t == BASE-1
                    uint32_t G = _pext_u32(uint32_t(_mm256_movemask_epi8(gm)), 0x55555555u);
                    uint32_t P = _pext_u32(uint32_t(_mm256_movemask_epi8(pm)), 0x55555555u);
                    uint32_t A = G, B = G | P;
                    uint32_t sw = A + B + carry_rip;
                    uint32_t cin = sw ^ A ^ B; // bit j = 进入 limb j 的进位
                    uint32_t cout = cin >> 1;  // bit j = limb j 的进位输出
                    carry_rip = (sw >> 16) & 1u;
                    __m256i vcin = _mm256_cmpeq_epi16(
                        _mm256_and_si256(_mm256_set1_epi16(short(cin)), vbit), vbit);
                    __m256i vcout = _mm256_cmpeq_epi16(
                        _mm256_and_si256(_mm256_set1_epi16(short(cout)), vbit), vbit);
                    __m256i res = _mm256_sub_epi16(
                        _mm256_add_epi16(t, _mm256_srli_epi16(vcin, 15)),
                        _mm256_and_si256(vcout, vbase16));
                    _mm256_storeu_si256(reinterpret_cast<__m256i *>(out.ptr + i), res);
                }
            }
            Limb carry = Limb(carry_q + carry_rip);
            for (; i < in.size; i++)
            {
                Limb2 prod = Limb2(in[i]) * x + carry;
                out[i] = prod % BASE;
                carry = prod / BASE;
            }
            return carry;
        }
        static Limb absDiv1(View in, Limb x, Span out)
        {
            Limb rem = 0;
            size_t i = in.size;
            while (i > 0)
            {
                i--;
                Limb2 prod = Limb2(in[i]) + Limb2(rem) * BASE;
                out[i] = prod / x;
                rem = prod % x;
            }
            return rem;
        }
        Limb selfDivRem1(Limb x)
        {
            Limb rem = absDiv1(this->getView(), x, this->getSpan());
            this->removeLeadingZero();
            return rem;
        }
        static void absDivBasicCore(Span dividend, View divisor, Span quotient)
        {
            if (dividend.size <= divisor.size)
            {
                return;
            }
            assert(divisor.size > 0);
            size_t len1 = dividend.size, len2 = divisor.size;
            Limb divisor_high = divisor[len2 - 1];
            assert(divisor_high >= HALF_BASE);
            size_t quot_idx = len1 - len2;

            // ================= D31: Knuth Algorithm D 步骤 D3 =================
            // 实测 (callgrind, r_nearly_zero_01): 本函数发出 267086 次 absMul1 却发出
            // 361815 次 absSub —— 多出的 94729 次全是 qhat 估大后的回退修正,
            // 修正率 35.5%。规范化 (v_high >= BASE/2) 只保证 qhat <= q+2, 真正把
            // 误差压到 "q 或 q+1 (后者概率 ~2/BASE)" 的是 Knuth 的 v[n-2] 测试,
            // 而原实现缺了这一步。补上之后:
            //   1) 每次迭代的全长 absCompare 彻底消失 —— 该比较在 r≈0 的用例上
            //      要一路深扫才能定出大小, 是 absDivBasicCore 自身 11% Ir 的主因;
            //      改由 absSub 已经免费返回的借位来判定。
            //   2) 35.5% 概率的全长修正 absSub -> 0.02% 概率的加回 absAdd。
            //   3) 一个几乎不可预测的分支 (35.5% 命中) -> 恒不跳转的分支。
            //
            // 顺带: 用 Granlund-Montgomery 乘法倒数消掉内循环里的硬件 32 位除法。
            // divisor_high 在整个除法期间不变, 故倒数只算一次, 摊到 ~67 次迭代上。
            //   m = floor(2^42/d) + 1, 对所有 n < 2^27 满足 floor(m*n/2^42) == n/d;
            //   充分条件: m*d - 2^42 = d - (2^42 mod d) <= 2^(42-27) = 32768,
            //   而 d <= BASE-1 = 9999 < 32768 恒成立 ✓
            //   n = high = high1*BASE + high2 <= 99999999 < 2^27 = 134217728 ✓
            //   m <= 2^42/5000 < 2^30, m*n < 2^57 < 2^64 ✓ (无溢出)
            const uint64_t dh_magic = (uint64_t(1) << 42) / divisor_high + 1;
            const Limb divisor_high2 = (len2 >= 2) ? divisor[len2 - 2] : Limb(0);

            thread_local std::vector<Limb> tprod;
            if (tprod.size() < len2 + 1)
                tprod.resize(len2 + 1);
            while (quot_idx > 0)
            {
                quot_idx--;
                len1 = quot_idx + len2;
                Limb high1 = dividend[len1], high2 = dividend[len1 - 1];
                Limb2 high = Limb2(high1) * BASE + high2;
                Limb2 qh;
                if (high1 >= divisor_high)
                {
                    qh = BASE - 1;
                }
                else
                {
                    qh = Limb2((dh_magic * uint64_t(high)) >> 42);
#ifdef DIV_D31_VERIFY
                    assert(qh == high / divisor_high);
#endif
                }
                Limb2 rhat = high - qh * Limb2(divisor_high);
                // Knuth D3: 只要 qhat*v[n-2] > rhat*BASE + u[j+n-2] 就说明 qhat 估大
                // (rhat >= BASE 时无需再测, 此时 qhat 必已正确)。至多循环 2 次。
                if (len2 >= 2)
                {
                    Limb2 u2 = dividend[len1 - 2];
                    while (rhat < Limb2(BASE) &&
                           qh * Limb2(divisor_high2) > rhat * Limb2(BASE) + u2)
                    {
                        qh--;
                        rhat += divisor_high;
                    }
                }
                Limb qhat = Limb(qh);
                Span prod_span(tprod.data(), len2 + 1);
                prod_span[len2] = absMul1(divisor, qhat, prod_span);
                if (prod_span[len2] == 0)
                {
                    prod_span.size = len2;
                }
                Span dividend_span(dividend + quot_idx);
                // 先减再看借位: 借位 <=> qhat 估大 (D3 之后概率 ~2/BASE)
                bool bf = absSub(dividend_span, prod_span, dividend_span);
                while (bf)
                {
                    qhat--;
                    // 加回除数产生的进位正好抵消之前的借位; 没进位说明还差, 继续
                    bf = !absAdd(dividend_span, divisor, dividend_span);
                }
                quotient[quot_idx] = qhat;
                dividend.size = len1;
            }
        }

        
        // B-1 优化: m_dft/m_dft_float_len 为预计算的 m 的 DFT（可选）
        // 最外层可传入以省 1 次 DFT(m)；递归层不传（m+s 子段 DFT 未预计算）
        // absInvNewton base case 阈值: k<=阈值时走 O(k^2) 学校除法
        // 理论分析: k=128 时 FFT 路径 ~10750 ops vs base case ~16384 ops, FFT 更快
        // k=64 时 FFT 路径 ~4000 ops vs base case ~4096 ops, 接近平衡
        // 当前默认 64 (保守), 可通过 -DINV_NEWTON_BASE_THRESHOLD=128 实验
#ifndef INV_NEWTON_BASE_THRESHOLD
#define INV_NEWTON_BASE_THRESHOLD 64
#endif
        static void absInvNewton(View m, Span inv,
                                 const double *m_dft = nullptr, size_t m_dft_float_len = 0)
        {
            size_t k = m.size;
            assert(k > 0);
            assert(inv.size >= k + 1);
            if (k <= INV_NEWTON_BASE_THRESHOLD)
            {
                Limb b_2k[INV_NEWTON_BASE_THRESHOLD * 2 + 2];
                b_2k[k * 2] = 1;
                std::fill_n(b_2k, k * 2, Limb(0));
                absDivBasicCore(Span(b_2k, k * 2 + 1), m, inv);
                return;
            }
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            size_t s = (k - 1) / 2;
            
            absInvNewton(m + s, inv);    
            size_t inv0_len = k - s + 1; 
            Span inv0(inv.ptr, inv0_len);
            
            
            thread_local std::vector<Limb> tprod, tinv2;
            size_t prod_size = inv0_len * 2 + k;
            size_t inv2_size = k + 1;
            if (tprod.size() < prod_size) tprod.resize(prod_size);
            if (tinv2.size() < inv2_size) tinv2.resize(inv2_size);
            std::fill_n(tinv2.data(), s, Limb(0)); 
            Span prod_span(tprod.data(), prod_size), inv2_span(tinv2.data(), inv2_size);
            bool cf = absAdd(inv0, inv0, inv2_span + s); 
            assert(!cf);                                 
#ifdef PROFILE_DIV
            auto _inv_t0 = std::chrono::high_resolution_clock::now();
#endif
            absSqr(inv0, prod_span);
#ifdef PROFILE_DIV
            auto _inv_t1 = std::chrono::high_resolution_clock::now();
            PROF_PRINT("  [prof]   absInvNewton(k=%zu): absSqr (inv0_len=%zu): %.3f ms\n",
                    k, inv0_len, std::chrono::duration<double, std::milli>(_inv_t1 - _inv_t0).count());
            auto _inv_t2 = std::chrono::high_resolution_clock::now();
#endif
            // B-1 优化: 若有预计算的 m DFT 且 float_len 匹配，用 fftMulPre 省 1 次 DFT(m)
            {
                size_t conv_len = inv0_len * 2 + k - 1;
                size_t need_float_len = fft_ceil_lin(conv_len);
                if (m_dft != nullptr && m_dft_float_len == need_float_len)
                {
                    fftMulPre(View(tprod.data(), inv0_len * 2), m_dft, k, need_float_len, prod_span);
                }
                else
                {
                    absMul(View(tprod.data(), inv0_len * 2), m, prod_span);
                }
            }
#ifdef PROFILE_DIV
            auto _inv_t3 = std::chrono::high_resolution_clock::now();
            PROF_PRINT("  [prof]   absInvNewton(k=%zu): absMul/fftMulPre (conv_len=%zu, fl=%zu): %.3f ms\n",
                    k, inv0_len * 2 + k - 1, fft_ceil_lin(inv0_len * 2 + k - 1),
                    std::chrono::duration<double, std::milli>(_inv_t3 - _inv_t2).count());
#endif
            prod_span = prod_span + 2 * (k - s);        
            assert(prod_span[prod_span.size - 1] == 0); 
            prod_span.size--;                           
            absSub(inv2_span, prod_span, inv);          
        }

        // GMP 风格的 Newton 逆计算 (两步分解 + cyclic convolution mod B^mn-1)
        // 设计文档: FFT_MULMOD_INTEGRATION_DESIGN.md
        // 当前版本: 仅 fallback 路径 (与 absInvNewton 等价的 HIGH-half 提取法)
        // cyclic 路径留作 TODO, 后续逐步实现 (正剩余类优先, 负剩余类用 assert 捕获)
        static void absInvNewtonGMP(View m, Span inv,
                                    const double *m_dft = nullptr, size_t m_dft_float_len = 0)
        {
            size_t k = m.size;  // GMP n
            INVPROF_SCOPE(k);
            assert(k > 0);
            assert(inv.size >= k + 1);

            // base case: 与 absInvNewton 一致, 走 O(k^2) 学校除法
            if (k <= INV_NEWTON_BASE_THRESHOLD)
            {
                Limb b_2k[INV_NEWTON_BASE_THRESHOLD * 2 + 2];
                b_2k[k * 2] = 1;
                std::fill_n(b_2k, k * 2, Limb(0));
                absDivBasicCore(Span(b_2k, k * 2 + 1), m, inv);
                return;
            }

#ifndef CYCLIC_MIN_K
#define CYCLIC_MIN_K 4096
#endif
            // === D9: 自适应 split 点 s (cyclic 窗口对齐) ===
            //   Newton 精度要求 rn >= ceil(k/2), 即 s <= (k-1)/2  —— 硬上界, s 只能减不能增.
            //   cyclic 生效要求 p2 = ceil2(k+1) <= k + rn = 2k - s, 即 s <= 2k - p2.
            //
            //   ★ 关键观察: 默认 s=(k-1)/2 会让比值 r = k/ceil2(k+1) 成为**不动点**
            //     (k -> k/2 时 p2 -> p2/2, r 不变). cyclic 需要 r >= 2/3;
            //     若顶层 r 落在死区 [1/2, 2/3), 则**整条 Newton 阶梯每一层都关闭 cyclic**.
            //     实测 D4 瓶颈 k=83334: r=0.636, 11 层 cyclic 启用 0 层 —— absInvNewtonGMP
            //     完全退化成 absInvNewton, 还白背了额外开销.
            //   ★ 对策: 顶层把 s 下调到 2k-p2, 一次性把 r 抬进 >= 2/3 的吸引域,
            //     之后各层沿用默认 s 即自动保持在吸引域内 (k=83334: 0/11 -> 5/11, 且是最大的 5 层).
            //   代价: 该层 rn 由 0.5k 增至 (p2-k), 递归子问题变大; 收益: 该层顶层卷积
            //     由 fallback(absSqr ceil2(2*inv0_len) + absMul 262144) 降为单次 cyclic(131072).
            //   护栏: 组合逻辑有 assert(2k >= 3rn) 即 rn <= 0.667k; 这里取 rn <= 0.65k 留余量.
            //
            // === D10 追加: 本层开不了 cyclic 时, 为**子层**铺路 ===
            //   有一类形状 (k 刚过 2 幂, 如 a_max_b_random_02 的 k=146990, r=0.561) 连
            //   s=2k-p2 都救不了 —— 需要 rn >= p2-k = 0.783k, 超过 2k/3 硬上限. D9 遇到
            //   这种直接放弃, 整条阶梯 0/12 全关.
            //   ★ 但 fallback 的两次 FFT 尺寸是 ceil2 台阶函数: rn 从 0.5k 抬到 0.65k
            //     往往**落在同一个台阶内**, 本层代价分文不涨; 而子层 k'=rn 的比值
            //     rn/ceil2(rn+1) 会被一次性抬进 >= 2/3 吸引域 —— 一次免费投入, 整条
            //     子阶梯受益. 实测 a_max_b_random_02: 0/12 -> 5/12, 倒数 FFT -16.1%.
            //   闸门 (缺一不可, 否则反亏): 子层须 (a) >= CYCLIC_MIN_K, (b) ceil2(k'+1) <= 1.65k'.
            //   模拟器 lc_bench/vm/ladder2.py 全域扫 k0 in [4096,300000]: 0 个形状变差, 总量 -9.9%.
            //
            // ★ D12 现状: mn 改用 fft_ceil 后 rn_cyc <= rn_min **恒成立**(证明见下方 mn 选择处),
            //   下面 `rn_cyc > rn_min` 的判据恒假 -> 整块自动失效, 默认 split 就已开着 cyclic.
            //   保留代码的意义: -DNO_FFT3 时 fft_ceil 退化为 int_ceil2, 死区回来, 该 hack
            //   随之复活并精确还原 D10 行为 -> A/B 对比时两侧都是各自的最优形态.
            size_t s = (k - 1) / 2;
#ifndef DISABLE_ADAPTIVE_SPLIT
            if (k >= CYCLIC_MIN_K)
            {
                // 必须与 mn 用同一档位函数, 否则会基于错误的 mn 白抬 rn (纯亏)
                const size_t p2_adj = fft_ceil_mn_q(k + 1);
                const size_t rn_min = k - s;                            // = ceil(k/2), Newton 精度下界
                const size_t rn_max = (k * 13) / 20;                    // 0.65k
                const size_t rn_cyc = (p2_adj > k) ? (p2_adj - k) : 0;  // 本层开 cyclic 所需最小 rn

                if (rn_cyc > rn_min)  // rn_cyc <= rn_min 表示默认 split 就已经开着, 不动
                {
                    if (rn_cyc <= rn_max)
                    {
                        // (1) 本层能开: 把 rn 抬到刚好开启 cyclic
                        s = k - rn_cyc;
                    }
                    else
                    {
                        // (2) 本层开不了: 在不抬高本层 fallback FFT 档位的前提下把 rn 顶到最大
                        const size_t c1 = fft_ceil_mn_q(2 * (rn_min + 1));           // absSqr(inv0)
                        const size_t c2 = fft_ceil_mn_q(2 * (rn_min + 1) + k - 1);   // absMul(inv0^2, m)
                        const size_t lim1 = c1 / 2 - 1;                          // 2*(rn+1)     <= c1
                        const size_t lim2 = (c2 - k - 1) / 2;                    // 2*(rn+1)+k-1 <= c2
                        const size_t rn_free = std::min(std::min(lim1, lim2), rn_max);
                        if (rn_free > rn_min)
                        {
                            // 候选: rn_free, 以及区间内最大的 2^j-1 (比值 ~1 的局部最优点).
                            // 比较 r/ceil2(r+1) 用交叉相乘, 避免浮点.
                            size_t cand = rn_free;
                            const size_t p = int_ceil2(rn_free + 1) / 2 - 1;
                            if (p >= rn_min && p <= rn_free &&
                                p * fft_ceil_mn_q(cand + 1) > cand * fft_ceil_mn_q(p + 1))
                                cand = p;
                            if (cand >= CYCLIC_MIN_K && fft_ceil_mn_q(cand + 1) * 20 <= cand * 33)
                                s = k - cand;
                        }
                    }
                }
            }
#endif
            // 递归调用自身 (后续 cyclic 路径启用后, 递归层也会走 GMP 风格)
            absInvNewtonGMP(m + s, inv);
            size_t rn = k - s;        // GMP rn
            size_t inv0_len = rn + 1;
            Span inv0(inv.ptr, inv0_len);

            // === mn 选择 (cyclic convolution 长度) ===
            // 约束: fftMulModBm1 要求 m 是合法 FFT 长度 (2^k 或 3*2^k)
            // 生效条件: mn >= k+1 (容纳 n+1) 且 mn <= k+rn (GMP ASSERT(n >= mn-rn))
            //
            // ★ D12: int_ceil2 -> fft_ceil, cyclic 死区被彻底消除
            //   旧: mn=int_ceil2(k+1) 最坏 ~2k, 要开 cyclic 需 rn >= mn-k ~ k, 但组合逻辑
            //       硬上限 rn <= 2k/3 -> 大片 k 永远开不了 cyclic (D9/D10 的"自适应 split"
            //       就是为了把 rn 抬进吸引域而生的 hack).
            //   新: mn=fft_ceil(k+1) 最坏 1.5k. 设 p=int_ceil2(k+1):
            //       - 若 0.75p >= k+1: mn=0.75p, 且 k+1 > p/2 => mn-k <= 0.75p-p/2 = p/4
            //         <= ceil((k+1)/2) = rn(默认 split) ✓
            //       - 否则: mn=p < (4/3)(k+1) => mn-k < (k+4)/3 ~ 0.33k < rn ✓
            //       两种情形下 **默认 split 恒满足 mn <= k+rn** -> cyclic 全程可开.
            // 阈值: k >= CYCLIC_MIN_K 时启用, 小 k 走 fallback (精度不足风险)
            //   实测: k=112/2001 有数据相关 FAIL (b_top=4/8), k>=6250 benchmark 全 PASS
            //   阈值 4096 平衡: 小 k 性能收益小, 风险高; 大 k 收益大, 已验证安全
            size_t mn = fft_ceil_mn(k + 1);
            // ★ D12 修复: 退化除数守卫 —— GMP 布局要求 inv0 的整数位恒为 1.
            //   m 归一化后 top limb >= BASE/2. 记子问题 m' = m[s..k-1] (rn 位),
            //   则 inv0 = floor(B^{2rn}/m') ∈ (B^rn, 2*B^rn]; 仅当 m' == B^rn/2 **恰好**
            //   (即 m'.top == BASE/2 且其余 limb 全 0) 时取到上界 2*B^rn, 此时
            //   inv.ptr[rn] == 2 且低 rn 位全 0.
            //   GMP invertappr 全程假定 ip = inv0 - B^rn 是 rn 位小数、整数位隐含为 1;
            //   整数位=2 会让 L217 的 MPN_DECR_U(ip-rn, rn, cy) 在全零串上减 -> 下溢 abort.
            //   (实测 b = 5*10^n / 25*10^n / 125*10^n / 5*10^n+1, 归一化后 m = 5000*B^{k-1}.)
            //   fallback (absInvNewton HIGH-half 提取法) 天然支持整数位=2, 故直接退回.
            //   触发条件极窄 (m' 必须是 B^rn/2 的精确形式), 对正常输入零开销.
            const bool inv0_degenerate = (inv.ptr[rn] != 1);
#ifndef DISABLE_2NXN_CYCLIC
            bool use_cyclic = (mn >= k + 1) && (mn <= k + rn) && (k >= CYCLIC_MIN_K)
                              && !inv0_degenerate;
#else
            bool use_cyclic = false;
#endif
            INVPROF_SET(s, rn, mn, (int)use_cyclic);
#ifdef DIV_INVPROBE
            if (k >= 1024)
                fprintf(stderr, "[invprobe] k=%zu s=%zu rn=%zu mn=%zu cyclic=%d\n",
                        k, s, rn, mn, (int)use_cyclic);
#endif

            if (use_cyclic)
            {
                // === cyclic 路径: 验证版本 ===
                // 目标: 验证 fftMulModBm1 + GMP 修正的正确性
                // 当前: cyclic 计算 + full mul 验证 + 原组合逻辑 (无性能提升, 但保证正确性)
                // 后续: 实现 GMP 组合逻辑以获得性能提升 (见 FFT_MULMOD_INTEGRATION_DESIGN.md)

                // --- 第一步: cyclic convolution ---
                // xp_mod[0..mn-1] = (inv0 * m) mod (B^mn - 1)
                thread_local std::vector<Limb> txp_mod;
                if (txp_mod.size() < mn + 2) txp_mod.resize(mn + 2);
                std::fill_n(txp_mod.data(), mn + 2, Limb(0));
                Span xp_mod(txp_mod.data(), mn);

                INVPH_DECL(conv);
                if (m_dft != nullptr && m_dft_float_len == mn)
                {
                    fftMulModBm1Pre(inv0, m_dft, k, mn, xp_mod);
                }
                else
                {
                    fftMulModBm1(inv0, m, mn, xp_mod);
                }
                INVPH_END(conv, k, "conv");
                INVPH_DECL(corr);

#ifdef DIV_INVCHECK
                // 探针: 用线性卷积 + 手工折叠算出 (inv0*m) mod (B^mn-1) 的参考值,
                //       与 fftMulModBm1 的输出逐位比对 -> 区分"卷积错"还是"GMP 修正错".
                {
                    size_t il = count_true_length(inv0.ptr, inv0.size);
                    size_t ml = count_true_length(m.ptr, m.size);
                    std::vector<Limb> ref(il + ml + 2, 0);
                    absMul(View(inv0.ptr, il), View(m.ptr, ml), Span(ref.data(), il + ml));
                    std::vector<Limb> acc(mn + 1, 0);
                    for (size_t off = 0; off < il + ml; off += mn)
                    {
                        size_t len = std::min(mn, il + ml - off);
                        Limb c = 0;
                        for (size_t j = 0; j < mn; j++)
                        {
                            Limb2 sv = Limb2(acc[j]) + Limb2(j < len ? ref[off + j] : Limb(0)) + c;
                            acc[j] = Limb(sv % BASE);
                            c = Limb(sv / BASE);
                        }
                        while (c)
                        {
                            for (size_t j = 0; j < mn && c; j++)
                            {
                                Limb2 sv = Limb2(acc[j]) + c;
                                acc[j] = Limb(sv % BASE);
                                c = Limb(sv / BASE);
                            }
                        }
                    }
                    size_t bad = mn;
                    for (size_t j = 0; j < mn; j++)
                        if (acc[j] != xp_mod[j]) { bad = j; break; }
                    if (bad != mn)
                        fprintf(stderr, "[invchk] CONV-MISMATCH k=%zu rn=%zu mn=%zu at j=%zu got=%u ref=%u\n",
                                k, rn, mn, bad, (unsigned)xp_mod[bad], (unsigned)acc[bad]);
                    else
                        fprintf(stderr, "[invchk] conv-ok k=%zu rn=%zu mn=%zu\n", k, rn, mn);
                }
#endif

                // --- GMP 修正: xp = (xp - B^{rn+k}) mod (B^mn - 1) ---
                // 注: GMP 源码 L176-L182 是 (ip*dp + dp*B^rn - B^{rn+n}) mod B^mn-1
                //     其中 GMP 的 ip 是 rn 位小数部分, dp*B^rn 用于凑成 inv0 = ip + B^rn
                // 本项目 inv0 已是 rn+1 位 (含 B^rn 整数位), fftMulModBm1(inv0, m, ...) 直接得 inv0*m
                // 因此只需减 B^{rn+k} = B^{rn+n}, 即 X = inv0*m - B^{rn+k}, 满足 |2X| < B^mn-1
                //    B^{rn+k} mod (B^mn-1) = B^{(rn+k) mod mn} = B^{rn+k-mn} (因为 mn <= rn+k < 2*mn)
                //    在位置 (rn+k-mn) 减 1, 处理借位 (mod B^mn-1, 可能 wrap)
                {
                    size_t sub_pos = rn + k - mn;
                    assert(sub_pos < mn);
                    Limb borrow = 1;
                    size_t j = sub_pos;
                    size_t guard = 0; // 防死循环
                    while (borrow > 0 && guard < 2 * mn)
                    {
                        if (xp_mod[j] >= borrow)
                        {
                            xp_mod[j] -= borrow;
                            borrow = 0;
                        }
                        else
                        {
                            xp_mod[j] = Limb(xp_mod[j] + BASE - borrow);
                            borrow = 1;
                            j = (j + 1) % mn;
                        }
                        guard++;
                    }
                    // 若绕一圈仍有 borrow, 说明 xp_mod 原 = 0, 减 1 = -1 mod (B^mn-1) = B^mn-2
                    // 这是负剩余类的情况, xp_mod[k] 应为 BASE-1
                    if (borrow > 0)
                    {
                        // xp_mod = -1 mod (B^mn-1) = B^mn - 2
                        // 表示为 [BASE-2, BASE-1, ..., BASE-1] (低位 BASE-2, 高位 BASE-1)
                        std::fill_n(xp_mod.ptr, mn, Limb(BASE - 1));
                        xp_mod[0] = Limb(BASE - 2);
                    }
                }

                // c) 正/负剩余类判定
                //    xp_mod[k] < 2 → 正剩余类 (X >= 0)
                //    xp_mod[k] >= BASE-2 → 负剩余类 (X < 0)
                Limb xpn = xp_mod[k];
                bool is_positive;
#ifdef DIV_INVPROBE
                fprintf(stderr, "[invres] k=%zu xpn=%u class=%s\n", k, (unsigned)xpn,
                        (xpn < 2) ? "POS" : (xpn >= BASE - 2 ? "NEG" : "ODD"));
#endif
                if (xpn < 2)
                {
                    is_positive = true;
                }
                else if (xpn >= BASE - 2)
                {
                    is_positive = false;
                }
                else
                {
                    // 异常: 既不是正也不是负剩余类
                    // 可能原因: FFT 浮点精度误差, 或 mn 选择导致 |2X| >= B^mn-1
                    // 安全回退到 fallback (与 absInvNewton 一致)
                    is_positive = false;
                    goto gmp_newton_fallback;
                }

                // === 阶段 B: GMP 两步分解 (cyclic + mul_n + 组合) ===
                // D9: 正/负剩余类**均已实现** (GMP invertappr.c L225-L266), 尾部 mul_n+组合共用.
                //   D4 只实现了正剩余类, 负剩余类 goto fallback —— 那等于 cyclic 白算一遍再把
                //   fallback 整个跑一遍, 比不开 cyclic 还慢. 实测 length_ratio_integer_02 上
                //   10 次 cyclic 有 2 次落负类, 其中一次正是最贵的顶层 k=83334.
                {
                    // 1. 把 inv0 从 inv.ptr[0..rn] 移到 inv.ptr[s..k] (与 GMP 布局对齐)
                    //    移动后: inv.ptr[s..k-1] = inv0 低 rn 位, inv.ptr[k] = 1 (整数位)
                    std::copy_backward(inv.ptr, inv.ptr + rn + 1, inv.ptr + k + 1);

                    // 2. 分配 xp_full 缓冲区 (长度 2k+2, 初始清零)
                    thread_local std::vector<Limb> txp_full;
                    if (txp_full.size() < 2 * k + 2) txp_full.resize(2 * k + 2);
                    std::fill_n(txp_full.data(), 2 * k + 2, Limb(0));

                    // 3. 把 cyclic 结果 xp_mod[0..mn-1] 复制到 xp_full[0..mn-1]
                    std::copy(xp_mod.ptr, xp_mod.ptr + mn, txp_full.data());

                    Limb *xp = txp_full.data();
                    Limb cy = 0;  // cyclic 模式 cy=0 (GMP L183)
                    assert(2 * k >= 3 * rn && "xp_high and mul_out must not overlap");

                  if (is_positive)
                  {
                    // 4a. GMP L188-L199: 正剩余类修正
                    //   cy = xp[n]; if (cy++ && !sub_n) { ASSERT_CARRY(sub_n); ++cy; }
                    cy = xp[k];  // 0 or 1
                    if (cy > 0)
                    {
                        // xp[0..k-1] -= m[0..k-1] (仅低 k 位, 不动 xp[k])
                        bool borrow = absSub(View(xp, k), m, Span(xp, k));
                        cy = 2;  // cy++ 后
                        if (!borrow)
                        {
                            // borrow=0 表示 xp_low >= m, 再减一次 (ASSERT_CARRY: 必须借位)
                            borrow = absSub(View(xp, k), m, Span(xp, k));
                            assert(borrow && "ASSERT_CARRY: second sub must borrow");
                            cy = 3;
                        }
                    }
                    else
                    {
                        cy = 1;  // cy++ 后
                    }

                    // 4b. GMP L211: if (mpn_cmp(xp, dp-n, n) > 0) { sub_n; ++cy; }
                    if (absCompare(View(xp, k), m) > 0)
                    {
                        bool borrow = absSub(View(xp, k), m, Span(xp, k));
                        assert(!borrow && "ASSERT_NOCARRY");
                        cy++;
                    }

                    // 4c. GMP L215: sub_nc(xp_high, dp-rn, xp+n-rn, rn, borrow_in)
                    //   xp_high = m[s..k-1] - xp[s..k-1] - borrow_in
                    //   borrow_in = (xp[0..s-1] > m[0..s-1]) ? 1 : 0
                    //   注: dp-rn 对应 m.ptr+s, xp+n-rn 对应 xp+s
                    assert(2 * k >= 3 * rn && "xp_high and mul_out must not overlap");
                    Limb *xp_high = xp + 2 * k - rn;
                    Limb bf = (absCompare(View(xp, s), View(m.ptr, s)) > 0) ? 1 : 0;
                    for (size_t i = 0; i < rn; i++)
                    {
                        xp_high[i] = sub_half<Limb>(m.ptr[s + i], xp[s + i] + bf, BASE, bf);
                    }
                    assert(bf == 0 && "ASSERT_NOCARRY: xp_high sub overflow");

                    // 4d. GMP L217: MPN_DECR_U(ip-rn, rn, cy)
                    //   inv0_low_rn (inv.ptr[s..k-1]) -= cy
                    {
                        bool bf2 = absSub1(View(inv.ptr + s, rn), cy, Span(inv.ptr + s, rn));
                        assert(!bf2 && "MPN_DECR_U underflow");
                    }
                  }
                  else
                  {
                    // === 负剩余类 (GMP invertappr.c L258-L266) ===
                    //   xp[n] ∈ {BASE-2, BASE-1}. cyclic 模式 cy=0, 故 MPN_DECR_U(xp,n+1,cy) 为空操作.
                    //   if (xp[n] != BASE-1) { INCR_U(ip-rn, rn, 1); ASSERT_CARRY(add_n(xp, xp, dp-n, n)); }
                    //   mpn_com(xp + 2n - rn, xp + n - rn, rn)   —— 按位取反在 base B 下是 (BASE-1) - x
                    if (xp[k] != Limb(BASE - 1))
                    {
                        assert(xp[k] == Limb(BASE - 2) && "negative class: xp[n] must be BASE-2");
                        bool cf3 = absAdd1(View(inv.ptr + s, rn), 1, Span(inv.ptr + s, rn));
                        assert(!cf3 && "MPN_INCR_U overflow into integer bit");
                        (void)cf3;
                        bool carry3 = absAdd(View(xp, k), m, Span(xp, k));
                        assert(carry3 && "ASSERT_CARRY: add_n must carry out");
                        (void)carry3;
                    }
                    Limb *xp_high_neg = xp + 2 * k - rn;
                    for (size_t i = 0; i < rn; i++)
                        xp_high_neg[i] = Limb(BASE - 1 - xp[s + i]);
                  }

                    // 5. GMP L228: mul_n: xp[0..2rn-1] = xp_high * inv0_low_rn
                    //   xp_high = xp[2k-rn..2k-1] (rn 位), inv0_low_rn = inv.ptr[s..k-1] (rn 位)
                    INVPH_END(corr, k, "corr");
                    {
                        INVPH_DECL(muln);
                        View xp_high_view(xp + 2 * k - rn, rn);
                        View inv0_low_view(inv.ptr + s, rn);
                        Span mul_out(xp, 2 * rn);
                        absMul(xp_high_view, inv0_low_view, mul_out);
                        INVPH_END(muln, k, "mul_n");
                    }

                    // 6. GMP L229-L231: 组合
                    // L229: xp[rn..rn+add_len-1] += xp_high[0..add_len-1] (add_len = 2rn-n)
                    size_t add_len = 2 * rn - k;
                    Limb carry = 0;
                    if (add_len > 0)
                    {
                        carry = absAdd(View(xp + rn, add_len),
                                       View(xp + 2 * k - rn, add_len),
                                       Span(xp + rn, add_len)) ? 1 : 0;
                    }

                    // L230: inv.ptr[0..s-1] = xp[3rn-k..2rn-1] + xp_high[2rn-k..rn-1] + carry
                    //   注: xp[3rn-k..2rn-1] 是 mul_n 输出高位, xp_high[2rn-k..rn-1] 是 xp_high 高 s 位
                    {
                        Limb *xph = xp + 2 * k - rn;
                        for (size_t i = 0; i < s; i++)
                        {
                            Limb2 sum = Limb2(xp[3 * rn - k + i]) + Limb2(xph[2 * rn - k + i]) + carry;
                            inv.ptr[i] = Limb(sum % BASE);
                            carry = Limb(sum / BASE);
                        }
                    }

                    // L231: MPN_INCR_U(ip-rn, rn, carry)
                    //   inv.ptr[s..k-1] += carry (进位传播)
                    if (carry > 0)
                    {
                        bool cf2 = absAdd1(View(inv.ptr + s, rn), carry, Span(inv.ptr + s, rn));
                        assert(!cf2 && "MPN_INCR_U overflow into integer bit");
                    }

                    // === GMP mpn_invert (invert.c L69-L84): 修正 invertappr 的 off-by-one ===
                    // ★ D12: 这是 D9 引入 cyclic 后的潜伏 bug, D10 带着它 AC 了 #390217.
                    //   ni_invertappr 语义是"近似"倒数, 契约 inv <= 真值, 误差只可能 0 或 1
                    //   (invertappr.c L75 "Assume the error can only be 0 or 1").
                    //   而本项目的 fallback (absInvNewton HIGH-half 提取法) 是**精确**的, 下游
                    //   (absDivMu / absDivNewtonWithInv / 反归一化 selfDivRem1) 全部按"精确 inv"
                    //   假定工作 -> cyclic 少 1 -> 商少 -> 余数非整除 -> assert(rem==0) 炸.
                    //   实测 a=全9 / b=10^k+1 (l2=8191): cyclic 给 1.2499..9999, 真值 1.2500..0000.
                    //
                    // GMP 的修正: 用保守标志 e 检测被丢弃低位可能产生的进位, 命中才验证:
                    //   e = (xp[3rn-n-1] > MAX-7)        (invertappr.c L275 "Be conservative")
                    //   if (e) { 算 dp*(inv+1); 若无进位说明 inv 真的少 1 -> inv+1 }
                    // 本项目 base=10^4 (非 2^64), 阈值放宽到 BASE-16 更保守. 触发率 ~0.16%,
                    // 平均代价可忽略, 但堵死了 all-9 / B^k+1 这类对抗性输入的 off-by-one.
                    // 修正后仍满足 inv <= 真值 (判据就是 (inv+1)*m < B^{2k}), 不破坏 Newton 单侧误差.
                    {
                        assert(3 * rn >= k + 1);
                        Limb inv_probe = xp[3 * rn - k - 1];
#ifdef DIV_INVPROBE
                        fprintf(stderr, "[invfix] k=%zu rn=%zu probe=%u thr=%u\n",
                                k, rn, (unsigned)inv_probe, (unsigned)(BASE - 16));
#endif
#ifdef DIV_INVFIX_ALWAYS
                        if (true)
#else
                        if (inv_probe > Limb(BASE - 16))
#endif
                        {
                            INVPH_DECL(fix);
                            thread_local std::vector<Limb> tinv_p1, tchk;
                            if (tinv_p1.size() < k + 2) tinv_p1.resize(k + 2);
                            if (tchk.size() < 2 * k + 3) tchk.resize(2 * k + 3);
                            std::copy(inv.ptr, inv.ptr + k + 1, tinv_p1.data());
                            bool cf_p1 = absAdd1(View(tinv_p1.data(), k + 1), 1,
                                                 Span(tinv_p1.data(), k + 1));
                            assert(!cf_p1 && "inv+1 overflow");
                            (void)cf_p1;
                            std::fill_n(tchk.data(), 2 * k + 2, Limb(0));
                            absMul(View(tinv_p1.data(), k + 1), m, Span(tchk.data(), 2 * k + 1));
                            // 判据: (inv+1)*m <= B^{2k}  ->  inv 确实少 1, 补上.
                            // ★ 注意是 "<=" 不是 GMP 的 "<". 设 V=floor(B^2k/m)-B^k (精确值),
                            //   r = B^2k mod m, GMP 判 (inv+1)*m >= B^2k 就不加:
                            //     inv=V-1, r>0 -> (V+B^k)*m = B^2k-r < B^2k  -> 加   ✓
                            //     inv=V-1, r=0 -> = B^2k, 被判"有进位"        -> 不加 ✗ 漏!
                            //   GMP base=2^64 时 r=0 需 m | 2^{128n}, 即 m=2^{64n-1} 唯一退化点,
                            //   实际不会撞上; 但本项目 base=10^4, m=8000*B^{k-1}=8*10^{4k-1} 整除
                            //   10^{8k} —— 退化情况常见 (任何 2^a*5^b*10^j 型 m). 故必须用 <=.
                            bool need_inc = false;
                            if (tchk[2 * k] == 0)
                            {
                                need_inc = true;  // < B^{2k}
                            }
                            else if (tchk[2 * k] == 1)
                            {
                                // 恰好 == B^{2k} ? (低 2k limb 必须全 0). 从高位往低位扫, 通常秒退.
                                need_inc = true;
                                for (size_t j = 2 * k; j-- > 0;)
                                    if (tchk[j] != 0) { need_inc = false; break; }
                            }
#ifdef DIV_INVPROBE
                            fprintf(stderr, "[invfix] k=%zu tchk[2k]=%u -> %s\n", k,
                                    (unsigned)tchk[2 * k], need_inc ? "APPLY +1" : "no-op");
#endif
                            if (need_inc)
                            {
                                bool cfx = absAdd1(View(inv.ptr, k + 1), 1, Span(inv.ptr, k + 1));
                                assert(!cfx && "invert correction overflow");
                                (void)cfx;
                            }
                            INVPH_END(fix, k, "offby1fix");
                        }
                    }

#ifdef DIV_INVDUMP
                    DIV_INVDUMP_EMIT("cyc", k, inv);
#endif
                    return;
                }
                // 负剩余类或异常: 回退到 fallback (full mul 组合逻辑, 与 absInvNewton 一致)
            }

        gmp_newton_fallback:
#ifdef INVPROF
            if (_ips.key.cyc == 1) _ips.key.cyc = 2;  // cyclic 试过但退回 fallback
#endif
            // === fallback: 原 absInvNewton 的 HIGH-half 提取法 ===
            // Newton 一步: inv = 2*inv0 - (inv0^2 * m)_high
            // 注: 此分支与 absInvNewton 逻辑完全一致, 用于保证正确性
            // 触发条件: (1) use_cyclic=false (2) 负剩余类 (3) 异常剩余类 (FFT 精度/mn 选择)
            thread_local std::vector<Limb> tprod, tinv2;
            size_t prod_size = inv0_len * 2 + k;
            size_t inv2_size = k + 1;
            if (tprod.size() < prod_size) tprod.resize(prod_size);
            if (tinv2.size() < inv2_size) tinv2.resize(inv2_size);
            std::fill_n(tinv2.data(), s, Limb(0));
            Span prod_span(tprod.data(), prod_size), inv2_span(tinv2.data(), inv2_size);
            bool cf = absAdd(inv0, inv0, inv2_span + s);
            assert(!cf);
            absSqr(inv0, prod_span);
            // B-1 优化: 若有预计算的 m DFT 且 float_len 匹配, 用 fftMulPre 省 1 次 DFT(m)
            // 注意: cyclic 路径下 m_dft 的 float_len 应为 mn, 与这里 need_float_len 不一致
            //       所以 float_len 不匹配时不复用 (保持与 absInvNewton 一致的行为)
            {
                size_t conv_len = inv0_len * 2 + k - 1;
                size_t need_float_len = fft_ceil_lin(conv_len);
                if (m_dft != nullptr && m_dft_float_len == need_float_len)
                {
                    fftMulPre(View(tprod.data(), inv0_len * 2), m_dft, k, need_float_len, prod_span);
                }
                else
                {
                    absMul(View(tprod.data(), inv0_len * 2), m, prod_span);
                }
            }
            prod_span = prod_span + 2 * (k - s);
            assert(prod_span[prod_span.size - 1] == 0);
            prod_span.size--;
            absSub(inv2_span, prod_span, inv);
#ifdef DIV_INVDUMP
            DIV_INVDUMP_EMIT("fbk", k, inv);
#endif
        }
        static void absDivNewtonWithInv(Span dividend, View divisor, Span quotient, View inv_span)
        {
            assert(dividend.size <= divisor.size * 2);
            if (dividend.size <= divisor.size)
            {
                return;
            }
            size_t k = divisor.size;
            Span divid_high = dividend + (k - 1);
            
            thread_local std::vector<Limb> tqhat, tprod;
            size_t qhat_len = divid_high.size + inv_span.size;
            size_t prod_len = qhat_len - 1;
            if (tqhat.size() < qhat_len)
                tqhat.resize(qhat_len);
            if (tprod.size() < prod_len)
                tprod.resize(prod_len);
            
            Span qhat_span(tqhat.data(), qhat_len), prod_span(tprod.data(), prod_len);
            absMul(inv_span, divid_high, qhat_span); 
            qhat_span = qhat_span + (k + 1);         
            absMul(divisor, qhat_span, prod_span);   
            prod_span.size = count_true_length(prod_span.ptr, prod_span.size);
            
            while (absCompare(prod_span, dividend) > 0)
            {
                absSub(prod_span, divisor, prod_span); 
                absSub1(qhat_span, 1, qhat_span);      
            }
            absSub(dividend, prod_span, dividend); 
            dividend.size = k;
            while (absCompare(dividend, divisor) >= 0)
            {
                absSub(dividend, divisor, dividend);
                absAdd1(qhat_span, 1, qhat_span);
            }
            assert(qhat_span[qhat_span.size - 1] == 0);
            qhat_span.size--;
            std::copy(qhat_span.begin(), qhat_span.end(), quotient.begin());
        }
        
        
        static void absDivNewtonWithInvFast(Span dividend, View divisor, Span quotient, View inv_span, const double *inv_dft, size_t inv_float_len, const double *divisor_dft, size_t divisor_float_len)
        {
            assert(dividend.size <= divisor.size * 2);
            if (dividend.size <= divisor.size)
            {
                return;
            }
            size_t k = divisor.size;
            Span divid_high = dividend + (k - 1);
            thread_local std::vector<Limb> tqhat, tprod;
            size_t qhat_len = divid_high.size + inv_span.size;
            size_t prod_len = qhat_len - 1;
            if (tqhat.size() < qhat_len)
                tqhat.resize(qhat_len);
            if (tprod.size() < prod_len)
                tprod.resize(prod_len);
            Span qhat_span(tqhat.data(), qhat_len), prod_span(tprod.data(), prod_len);
            fftMulPre(divid_high, inv_dft, inv_span.size, inv_float_len, qhat_span);
            qhat_span = qhat_span + (k + 1);
            fftMulPre(qhat_span, divisor_dft, divisor.size, divisor_float_len, prod_span);
            // OPT: 删除 count_true_length — absCompare 内部会重算
            while (absCompare(prod_span, dividend) > 0)
            {
                absSub(prod_span, divisor, prod_span);
                absSub1(qhat_span, 1, qhat_span);
            }
            // OPT: 修正后 prod <= dividend, 真实长度 <= dividend.size, 截断后保证 absSub 安全
            prod_span.size = std::min(prod_span.size, dividend.size);
            absSub(dividend, prod_span, dividend);
            dividend.size = k;
            while (absCompare(dividend, divisor) >= 0)
            {
                absSub(dividend, divisor, dividend);
                absAdd1(qhat_span, 1, qhat_span);
            }
             assert(qhat_span[qhat_span.size - 1] == 0);
             qhat_span.size--;
             std::copy(qhat_span.begin(), qhat_span.end(), quotient.begin());
         }
         
         // 宽松分块除法: dead code in div.cpp (未被调用), 已用 #if 0 移除
#if 0
         static void absDivNewtonWithInvLoose(Span dividend, View divisor, Span quotient, View inv_span)
         {
             assert(dividend.size <= divisor.size * 2);
             if (dividend.size <= divisor.size)
             {
                 return;
             }
             size_t k = divisor.size;
             Span divid_high = dividend + (k - 1);

             thread_local std::vector<Limb> tqhat, tprod;
             size_t qhat_len = divid_high.size + inv_span.size;
             size_t prod_len = qhat_len - 1;
             if (tqhat.size() < qhat_len)
                 tqhat.resize(qhat_len);
             if (tprod.size() < prod_len)
                 tprod.resize(prod_len);

             Span qhat_span(tqhat.data(), qhat_len), prod_span(tprod.data(), prod_len);
             absMul(inv_span, divid_high, qhat_span);
             qhat_span = qhat_span + (k + 1);
             absMul(divisor, qhat_span, prod_span);
             prod_span.size = count_true_length(prod_span.ptr, prod_span.size);

             // 修正循环: 允许多达 5 次迭代（增加容错能力）
             int corrections = 0;
             while (absCompare(prod_span, dividend) > 0 && corrections < 5)
             {
                 absSub(prod_span, divisor, prod_span);
                 absSub1(qhat_span, 1, qhat_span);
                 corrections++;
             }
             absSub(dividend, prod_span, dividend);
             dividend.size = k;
             // 最终检查: 同样允许多次修正
             corrections = 0;
             while (absCompare(dividend, divisor) >= 0 && corrections < 5)
             {
                 absSub(dividend, divisor, dividend);
                 absAdd1(qhat_span, 1, qhat_span);
                 corrections++;
             }
             assert(qhat_span[qhat_span.size - 1] == 0);
             qhat_span.size--;
             std::copy(qhat_span.begin(), qhat_span.end(), quotient.begin());
         }
#endif
         
         static void absDivNewtonCore1(Span dividend, View divisor, Span quotient)
        {
            if (dividend.size <= divisor.size || dividend.size >= divisor.size * 2)
            {
                return;
            }
            assert(divisor.size > 0);
            size_t len1 = dividend.size, len2 = divisor.size, quot_len = len1 - len2, shift_len = len2 - quot_len;
            assert(divisor[len2 - 1] >= HALF_BASE);
            Span dividend_high = dividend + shift_len;
            View divisor_high = divisor + shift_len;
            if (absCompare(dividend_high + quot_len, divisor_high) >= 0)
            {
                std::fill_n(quotient.begin(), quot_len, Limb(BASE - 1));
                
                
                
                absSub(dividend_high + quot_len, divisor_high, dividend_high + quot_len);
                dividend_high[quot_len] = absAdd(dividend_high, divisor_high, dividend_high);
            }
            else
            {
                thread_local std::vector<Limb> t_inv;
                size_t inv_size = divisor_high.size + 1;
                if (t_inv.size() < inv_size) t_inv.resize(inv_size);
                Span inv_span(t_inv.data(), inv_size);

                // OPT: divisor_high 足够大时用 fast 路径 (fftMulPre + DFT 复用, 4 FFT)
                //     否则用 slow 路径 (absMul, 6 FFT)
                if (divisor_high.size >= FFT_MUL_THRESHOLD)
                {
                    thread_local AlignedVec32<double> inv_dft_buf_c1, div_dft_buf_c1;
                    size_t inv_fl = fft_ceil_lin(2 * divisor_high.size + 1);
                    size_t div_fl = fft_ceil_lin(2 * divisor_high.size);
                    if (inv_dft_buf_c1.size() < inv_fl) inv_dft_buf_c1.resize(inv_fl);
                    if (div_dft_buf_c1.size() < div_fl) div_dft_buf_c1.resize(div_fl);
                    prepareDFT(divisor_high, div_dft_buf_c1.data(), div_fl);
                    absInvNewton(divisor_high, inv_span, div_dft_buf_c1.data(), div_fl);
                    prepareDFT(inv_span, inv_dft_buf_c1.data(), inv_fl);
                    absDivNewtonWithInvFast(dividend_high, divisor_high, quotient, inv_span,
                                            inv_dft_buf_c1.data(), inv_fl,
                                            div_dft_buf_c1.data(), div_fl);
                }
                else
                {
                    absInvNewton(divisor_high, inv_span);
                    absDivNewtonWithInv(dividend_high, divisor_high, quotient, inv_span);
                }
            }
            
            thread_local std::vector<Limb> t_prod;
            size_t prod_size = quot_len + shift_len;
            if (t_prod.size() < prod_size) t_prod.resize(prod_size);
            Span prod_span(t_prod.data(), prod_size);
            View divisor_low(divisor.begin(), shift_len);
            absMul(divisor_low, quotient, prod_span);
            prod_span.size = count_true_length(prod_span.ptr, prod_span.size);
            dividend.size = count_true_length(dividend.ptr, dividend.size);
            dividend_high.size = count_true_length(dividend_high.ptr, dividend_high.size);
            int count = 0;
            while (absCompare(prod_span, dividend) > 0)
            {
                assert(count < 2);
                count++;
                absSub1(quotient, 1, quotient); 
                quotient.size = count_true_length(quotient.ptr, quotient.size);
                if (absAdd(dividend_high, divisor_high, dividend_high)) 
                {
                    size_t add_len = std::max(dividend_high.size, divisor_high.size);
                    dividend_high[add_len] = 1;
                    dividend_high.size = add_len + 1;
                }
                else
                {
                    dividend_high.size = std::max(dividend_high.size, divisor_high.size);
                }
                dividend.size = dividend_high.size + shift_len;
                absSub(prod_span, divisor_low, prod_span); 
                prod_span.size = count_true_length(prod_span.ptr, prod_span.size);
            }
            absSub(dividend, prod_span, dividend);
            dividend.size = count_true_length(dividend.ptr, dividend.size);
            assert(absCompare(dividend, divisor) < 0);
        }

        // GMP mu_div_qr 风格分块除法: 用 in 位近似逆替代 len2+1 位精确逆
        // dividend: len1 位（原地修改为余数）
        // divisor: len2 位（已归一化，高位 >= HALF_BASE）
        // quotient: len1 - len2 位（输出，高位 1 位已预处理）
        // in: 近似逆精度（in <= len2）
        static void absDivMu(Span dividend, View divisor, Span quotient, size_t in, bool allow_cyclic = true)
        {
            if (dividend.size <= divisor.size)
            {
                return;
            }
            size_t len1 = dividend.size, len2 = divisor.size;
            assert(in <= len2);
            assert(in >= 1);
            assert(divisor[len2 - 1] >= HALF_BASE);

#ifdef PROFILE_DIV
            auto _mu_t0 = std::chrono::high_resolution_clock::now();
#endif

            // 1. 计算近似逆: divisor 高 in 位的精确逆
            // absInvNewton(divisor_high, inv) = floor(B^(2*in) / divisor_high), 输出 in+1 位
            // inv * divisor_high ≈ B^(2*in), inv ≈ B^(len2+in) / divisor
            //
            // OPT: inv 精度提升 — 多算 inv_extra 位, 截断后 inv 误差从 ±1 降到 ±1/B^inv_extra
            //   避免 qhat top_limb != 0 触发 basicMul fallback (burnikel_01: 251 次/64.5ms → 0)
            //   inv_float_len 不变 (int_ceil2(2*(in+2)+1) == int_ceil2(2*in+1) 对绝大多数 in)
            thread_local std::vector<Limb> t_inv;
            const size_t inv_extra = (in + 2 <= len2) ? 2 : 0;
            size_t inv_size = in + 1 + inv_extra;
            if (t_inv.size() < inv_size) t_inv.resize(inv_size);
            Span inv_span_full(t_inv.data(), inv_size);      // 完整 inv (含冗余低位)
            Span inv_span(t_inv.data() + inv_extra, in + 1); // 实际使用的高 in+1 位

            // 2. DFT 预计算
            // fftMulPre #1: divid_high(this_in+1) * inv(in+1), 卷积长度 <= 2*in+1
            // fftMulPre #2: qhat(this_in+1) * divisor(len2), 卷积长度 <= len2+in
            thread_local AlignedVec32<double> inv_dft_buf, divisor_dft_buf;
            size_t inv_float_len = fft_ceil_lin(2 * in + 1);
            // OPT: divisor_float_len 从 ceil(len2+in+1) 改为 ceil(len2+in)
            //   conv_len = qhat_span.size + len2 - 1 = (this_in+1) + len2 - 1 = this_in + len2 <= in + len2
            //   安全: fft_ceil(len2+in) >= conv_len
            size_t divisor_float_len = fft_ceil_lin(len2 + in);
            if (inv_dft_buf.size() < inv_float_len) inv_dft_buf.resize(inv_float_len);
            if (divisor_dft_buf.size() < divisor_float_len) divisor_dft_buf.resize(divisor_float_len);
            // FIX: cyclic 路径在 cyclic_m >= 线性卷积长度时退化, fftMulModBm1Pre 精度差,
            //   导致 r-based 修正过度调整 qhat (burnikel_ziegler_bound base=10/10^9 RE).
            //   运行时检测退化, 切换到 linear 路径. 保留有意义的 cyclic (如 1M/500k 22% 提升).
            bool use_cyclic = false;
#ifndef DISABLE_2NXN_CYCLIC
            // 2NXN 循环卷积 (GMP mu_div_qr.c L288-303):
            // fftMulPre #2 用 cyclic mod (B^m-1), FFT 大小 m=int_ceil2(len2+1)
            // 1M/500k: m=131072 (vs 线性卷积 262144), FFT 减半
            // unwrap: prod_low = C - window_high (GMP 近似, 修正循环处理误差)
            thread_local AlignedVec32<double> divisor_dft_mod_buf;
            // FIX: cyclic_m 需满足 2*cyclic_m > len2+in >= len2+this_in, 否则:
            //   wn = len2+this_in-cyclic_m > cyclic_m → Span(ptr,wn) 越界 + (cyclic_m-wn) 下溢
            //   且 unwrap 只处理一次 wrap (k=1), k>=2 时残差完全错误
            // 方案: 取 max(fft_ceil(len2+1), fft_ceil((len2+in)/2+1))
            //   fft_ceil(n) >= n, 故 2*fft_ceil((len2+in)/2+1) >= 2*((len2+in)/2+1)
            //   >= len2+in+1 > len2+in >= len2+this_in ✓ (len2+in 为奇数时取等前者)
            // 性能: 大比例(a>>b)时 cyclic_m≈非cyclicFFT/2, 仍保留约2倍FFT提升
            size_t cyclic_m = std::max(fft_ceil_cycm(len2 + 1), fft_ceil_cycm((len2 + in) / 2 + 1));
            // (DIV_MUSHAPE 探针已下移至 use_cyclic 计算之后, 以打印真实决策口径)
            // FIX: unwrap 近似误差在多块场景(blocks>>1)累积超出 r-based 修正容错
            // 当 est_blocks>10 时增大 cyclic_m 到 int_ceil2(len2+in+1) > len2+this_in, 消除 unwrap
            // 安全: cyclic 卷积 = 线性卷积 (2*cyclic_m > conv_len), tprod=精确prod, r-based 修正准确
            // 性能: FFT 大小≈非cyclic, 但保留 absDivMu 预计算 inv/divisor DFT 复用优势
            // D3: 不放大 cyclic_m, 真 cyclic (半尺寸 FFT) 对所有块数生效
            // 运行时检测: cyclic_m >= in+len2 时循环卷积退化为线性 (conv_len_max = in+len2),
            //   fftMulModBm1Pre 精度无优势且更差, 改用 fftMulPre + 双向修正
            // D4 防御: in<64 时 cyclic_m 与线性卷积尺寸相近 (cyclic 收益可忽略),
            //   强制走精确线性卷积, 消除小 in 浮点舍入导致的 qhat 顶端偏差风险.
            // D4: cyclic 仅当调用方允许 (allow_cyclic). 调用方在 mu_in>=64 (D3 已验证 cyclic 安全) 时允许,
            //   在 mu_in<64 (burnikel 等对抗性余数) 时禁止 -> 走精确线性卷积, 避免 cyclic unwrap 出错.
            //
            // ★ D12 FIX (fuzz 回归: l2=8192/8193, a=全9 / b=全9|pow10 商错, rem!=0 断言炸):
            //   把「是否用 cyclic」的决策口径与「模数多大」解耦.
            //   - 决策 (cyclic_m_gate) 恒用 2 幂口径 => D12 开 cyclic 的配置集合与 D11 **逐位相同**,
            //     不新增任何一个未经验证的 cyclic 形状 (absDivMu 的 unwrap 是近似修正, 对
            //     全9/pow10 这类对抗性余数本就脆弱, 见 mu_in<64 那条历史记录).
            //   - 尺寸 (cyclic_m) 仍走 fft_ceil => 已验证安全的那些配置照样吃到 fft3 的 FFT 缩减.
            //   反例: len2=in=8192 时 int_ceil2 给 gate=16384, 不 < in+len2=16384 -> 线性 (D11 行为);
            //     若用 fft_ceil 则 12288 < 16384 -> 凭空开出一个 wn=4096 的深 wrap cyclic -> 错.
            // ★ D14: unwrap 精确化后, 闸门口径可以从"保守的 2 幂"放宽到"真实档位"。
            //   D12 之所以把闸门锁成 int_ceil2, 是因为近似 unwrap 在深 wrap 下会错
            //   (漏了 mpn_incr_u(tp, cx-cy), 见下方 unwrap 处的推导)。
            //   现在 unwrap 与 GMP 逐步等价, 判据回归 GMP 本义: "模数 < 线性卷积长度
            //   ⟺ 发生 wrap ⟺ 有收益", 且 wrap 深度天然安全 (wn < this_in <= len2,
            //   且 len2+this_in <= 2*len2 < 2*m, 恒单次 wrap)。
            //   收益样本 length_ratio_integer_00 (len2=166667,in=83334):
            //     旧: int_ceil2(166668)=262144 不 < 250001 -> 线性卷积 Fl=262144
            //     新: fft_ceil_cycm(166668)=196608 < 250001 -> 循环卷积 Fc=196608 (-25%)
#ifdef GATE_POW2
            const size_t cyclic_m_gate = std::max(int_ceil2(len2 + 1),
                                                  int_ceil2((len2 + in) / 2 + 1));
#else
            const size_t cyclic_m_gate = cyclic_m;
#endif
            use_cyclic = (cyclic_m_gate < in + len2) && allow_cyclic;
#ifdef DIV_MUSHAPE
            {
                // 探针用**真实**决策 (use_cyclic), 不再复制一份可能走样的旧口径。
                const size_t _nblk = (len1 - len2 + in - 1) / in;
                auto _W = [](size_t n) { return n * std::log2((double)n); };
                const size_t _Fc = use_cyclic ? cyclic_m : divisor_float_len;
                const double _w = _nblk * (2 * _W(inv_float_len) + 2 * _W(_Fc));
                std::fprintf(stderr,
                             "[mushape] len1=%zu len2=%zu qn=%zu in=%zu nblk=%zu "
                             "Fi=%zu Fl=%zu Fc=%zu cyc=%d allow=%d blkW=%.6g\n",
                             len1, len2, len1 - len2, in, _nblk,
                             inv_float_len, divisor_float_len, cyclic_m,
                             (int)use_cyclic, (int)allow_cyclic, _w);
            }
#endif
            if (use_cyclic && divisor_dft_mod_buf.size() < cyclic_m) divisor_dft_mod_buf.resize(cyclic_m);
#endif

            // B-1 优化: 当 in == len2 时, divisor 切片 = 完整 divisor, 可复用 DFT 给倒数
            if (in == len2)
            {
#if !defined(DISABLE_2NXN_CYCLIC) && !defined(DISABLE_INLEN2_GMP)
                // === D9: in==len2 改走 GMP 风格倒数 ===
                //   D7 曾试过这个改动并回归, 根因是 in==len2 时 k=len2 落在比值死区
                //   [1/2, 2/3) -> GMP 的 cyclic 全程禁用, 退化成 absInvNewton 还多算 DFT.
                //   adaptive split (见 absInvNewtonGMP) 修掉死区后, 该路径才真正有意义.
                //
                //   ★ 额外红利: GMP 顶层 cyclic 长度 mn = ceil2(k+1) = ceil2(len2+1),
                //     而 absDivMu 的 cyclic_m = max(ceil2(len2+1), ceil2((len2+in)/2+1));
                //     in==len2 时两者恒等 => divisor 的 DFT 只需算一次(cyclic_m 尺寸),
                //     倒数与块循环共用, 同时**彻底省掉 divisor_float_len(=ceil2(2*len2)) 的大 DFT**.
                if (use_cyclic)
                {
                    prepareDFT(divisor, divisor_dft_mod_buf.data(), cyclic_m);
                    absInvNewtonGMP(divisor, inv_span, divisor_dft_mod_buf.data(), cyclic_m);
                }
                else
                {
                    prepareDFT(divisor, divisor_dft_buf.data(), divisor_float_len);
                    absInvNewton(divisor, inv_span, divisor_dft_buf.data(), divisor_float_len);
                }
#else
                prepareDFT(divisor, divisor_dft_buf.data(), divisor_float_len);
                absInvNewton(divisor, inv_span, divisor_dft_buf.data(), divisor_float_len);
#ifndef DISABLE_2NXN_CYCLIC
                if (use_cyclic) {
                    prepareDFT(divisor, divisor_dft_mod_buf.data(), cyclic_m);
                }
#endif
#endif
            }
            else
            {
                // GMP 风格 Newton 逆 (默认启用): 1M/500k 实测 17.68ms vs absInvNewton 18.88ms (-6.3%)
                // CYCLIC_MIN_K=4096 阈值保证; k<4096 时 absInvNewtonGMP 内部回退到 absInvNewton 逻辑
                if (inv_extra > 0) {
                    // inv 精度提升: m 长度 in+inv_extra → inv 长度 in+1+inv_extra
                    // 截断低 inv_extra 位后, inv 误差 ±1 → ±1/B^inv_extra
                    // 避免 qhat top_limb != 0 触发 basicMul fallback
                    absInvNewtonGMP(divisor + (len2 - in - inv_extra), inv_span_full);
                } else {
                    absInvNewtonGMP(divisor + (len2 - in), inv_span);
                }
#ifndef DISABLE_2NXN_CYCLIC
                if (use_cyclic) {
                    prepareDFT(divisor, divisor_dft_mod_buf.data(), cyclic_m);
                } else
#endif
                {
                    prepareDFT(divisor, divisor_dft_buf.data(), divisor_float_len);
                }
            }
            prepareDFT(inv_span, inv_dft_buf.data(), inv_float_len);

#ifdef PROFILE_DIV
            auto _mu_t1 = std::chrono::high_resolution_clock::now();
            PROF_PRINT("  [prof] absDivMu: absInvNewton+prepareDFT: %.3f ms (in=%zu, inv_fl=%zu, div_fl=%zu)\n",
                    std::chrono::duration<double, std::milli>(_mu_t1 - _mu_t0).count(), in, inv_float_len, divisor_float_len);
            auto _mu_t1b = std::chrono::high_resolution_clock::now();
#endif

            // 3. 分块循环（从高位向低位）
            // qn = quotient.size, 每个 block 处理 this_in = min(in, qn_remaining) 位商
            // 当 qn 不是 in 整数倍时, 最后一个 block 用较小 this_in (精度仍为 in+1, 安全)
            thread_local std::vector<Limb> tqhat, tprod;
            // OPT: 循环外预 resize 到最大 (qhat_len_max = in + in + 2, prod_len_max = len2 + in + 1)
            size_t qhat_len_max = 2 * in + 2;
            size_t prod_len_max = len2 + in + 1;
#ifndef DISABLE_2NXN_CYCLIC
            // fftMulModBm1Pre 需要 cyclic_m 长度缓冲, cyclic_m 可能 > prod_len_max
            prod_len_max = std::max(prod_len_max, cyclic_m);
#endif
            if (tqhat.size() < qhat_len_max) tqhat.resize(qhat_len_max);
            if (tprod.size() < prod_len_max) tprod.resize(prod_len_max);
            size_t qn = quotient.size;
            size_t qn_remaining = qn;
#ifdef PROFILE_DIV
            auto _mu_t2 = std::chrono::high_resolution_clock::now();
            PROF_PRINT("  [prof] absDivMu: gap1 (fprintf=%.3f ms, resize=%0.3f ms)\n",
                    std::chrono::duration<double, std::milli>(_mu_t1b - _mu_t1).count(),
                    std::chrono::duration<double, std::milli>(_mu_t2 - _mu_t1b).count());
#endif

            while (qn_remaining > 0)
            {
                size_t this_in = std::min(in, qn_remaining);

                // 当前块窗口: dividend[qn_remaining - this_in .. qn_remaining + len2 - 1]
                // 物理上 len1 = len2 + qn, 窗口长度 = len2 + this_in
                Span window(dividend.ptr + (qn_remaining - this_in), len2 + this_in);
                Span quot_block(quotient.ptr + (qn_remaining - this_in), this_in);

                // divid_high = window 高 this_in+1 位 (从 window[len2-1] 开始)
                Span divid_high = window + (len2 - 1);

                // qhat_full = divid_high * inv, 长度 = this_in + in + 2
                size_t qhat_len = divid_high.size + inv_span.size;
                Span qhat_full(tqhat.data(), qhat_len);
#ifdef PROFILE_DIV
                auto _blk_t0 = std::chrono::high_resolution_clock::now();
#endif
                fftMulPre(divid_high, inv_dft_buf.data(), inv_span.size, inv_float_len, qhat_full);
#ifdef PROFILE_DIV
                auto _blk_t1 = std::chrono::high_resolution_clock::now();
                PROF_PRINT("  [prof]   block %zu: fftMulPre #1 (divid_high*inv, fl=%zu): %.3f ms\n",
                        (qn - qn_remaining) / in, inv_float_len,
                        std::chrono::duration<double, std::milli>(_blk_t1 - _blk_t0).count());
#endif

                // qhat = qhat_full 的高 this_in+1 位 (跳过低 in+1 位)
                // qhat ≈ divid_high * inv / B^(in+1) ≈ Q_block (本块真实商)
                Span qhat_span = qhat_full + (in + 1);

                // FFT precision fix: if qhat top limb != 0, FFT rounding error
                // propagated via carry chain to top limb (±B^this_in level error).
                // r-based correction loop (±1 adjustments) cannot handle this.
                // Recompute divid_high * inv with exact basicMul.
                // Triggers mainly for adversarial inputs (e.g. burnikel_ziegler_bound);
                // for random inputs FFT is exact, so performance impact is negligible.
                bool _fallback_triggered = false;
#ifdef PROFILE_DIV
                auto _fb_t0 = std::chrono::high_resolution_clock::now();
#endif
                if (qhat_span.size > 0 && qhat_span[qhat_span.size - 1] != Limb(0))
                {
                    _fallback_triggered = true;
#ifdef PROFILE_DIV
                    Limb _fb_top = qhat_span[qhat_span.size - 1];
                    PROF_PRINT("  [prof]   block %zu: basicMul FALLBACK top_limb=%u (divid_high=%zu, inv=%zu)\n",
                            (qn - qn_remaining) / in, unsigned(_fb_top), divid_high.size, inv_span.size);
#endif
                    basicMul(divid_high, inv_span, qhat_full);
                }
#ifdef PROFILE_DIV
                auto _fb_t1 = std::chrono::high_resolution_clock::now();
                if (_fallback_triggered) {
                    PROF_PRINT("  [prof]   block %zu: basicMul FALLBACK triggered (divid_high=%zu, inv=%zu): %.3f ms\n",
                            (qn - qn_remaining) / in, divid_high.size, inv_span.size,
                            std::chrono::duration<double, std::milli>(_fb_t1 - _fb_t0).count());
                }
#endif

                // prod = divisor * qhat, 长度 = len2 + this_in + 1
                size_t prod_len = len2 + qhat_span.size;
                Span prod_span(tprod.data(), prod_len);
#ifdef PROFILE_DIV
                auto _blk_t2 = std::chrono::high_resolution_clock::now();
#endif
#ifndef DISABLE_2NXN_CYCLIC
                // 2NXN 循环卷积 (GMP mu_divappr_q.c L220-L232):
                // C = (qhat * divisor) mod (B^cyclic_m - 1), FFT 大小 cyclic_m (减半)
                // unwrap (GMP L226-L227): tp[0..wn-1] -= rp[dn-wn..dn-1] (rp 高 wn 位 ≈ prod 高 wn 位)
                // 修正 (GMP L228-L230): cx-cy 进位调整, 反映 cyclic 边界误差
                if (use_cyclic)
                {
                    Span prod_mod_span(tprod.data(), cyclic_m);
                    fftMulModBm1Pre(qhat_span, divisor_dft_mod_buf.data(), len2, cyclic_m, prod_mod_span);
                    // FIX: 当 len2+this_in <= cyclic_m 时, 卷积长度 <= cyclic_m, 循环卷积 == 线性卷积, 无需 unwrap
                    //   (原代码 size_t wn = len2+this_in-cyclic_m 在此情况下 underflow 成巨大数, 导致越界 SIGSEGV)
                    if (len2 + this_in > cyclic_m)
                    {
                        size_t wn = len2 + this_in - cyclic_m;
                        // GMP L226: tp[0..wn-1] -= rp[dn-wn..dn-1]
                        //   moptm window 布局: [np_new(低 this_in), rp_old(高 len2)]
                        //   rp_old = window[this_in .. len2+this_in-1]
                        //   rp_old[dn-wn..dn-1] = window[len2+this_in-wn .. len2+this_in-1]
                        Span prod_low_wn(prod_mod_span.ptr, wn);
                        View rp_high_wn(window.ptr + (len2 + this_in - wn), wn);
                        bool borrow = absSub(prod_low_wn, rp_high_wn, prod_low_wn);
                        // GMP L227: cy = mpn_sub_1 (tp + wn, tp + wn, tn - wn, cy)
                        // FIX: 捕获 absSub1 返回值 (最终 borrow), 对应 GMP cy 更新
                        //   原 bug: 返回值被丢弃, borrow 保持原值, 导致 cx-cy 修正错误
                        Span prod_rest(prod_mod_span.ptr + wn, cyclic_m - wn);
                        if (borrow) borrow = absSub1(prod_rest, 1, prod_rest);
                        // ★ D14: 精确 unwrap 修正 —— 抛弃 GMP 的 cx/cy 启发式。
                        //
                        //   GMP L228-L230 用 cx = mpn_cmp(rp+dn-in, tp+dn, tn-dn) < 0 探测
                        //   "低位借位是否越过 B^m 边界"。该判据是**单向**的: 它假定 qhat <= q
                        //   (GMP 的 inv 恒偏小), 于是 e = np_new - R_new < 0, 只可能借位。
                        //   moptm 的 absInvNewton 可能偏大 => qhat > q => R_new < 0 => e > 0
                        //   且可超过 B^len2 => 变成**进位** => rp段 < tp段 成立 => cx 误报。
                        //   实测 medium_02: 5/5 次 cx=1, 而 basicMul ground truth 显示 incr=0,
                        //   照 GMP 加 1 直接把结果打飞 (这正是当年"方向错误"被禁用的真因)。
                        //
                        //   改用**精确 O(1) 判据**:
                        //     tp0 = P mod (B^m - 1) = P_hi + P_lo - j*(B^m - 1),  j ∈ {0,1}
                        //     tp  = tp0 - A   (A = rp_high_wn),  δ = P_hi - A ∈ {-1,0,1}
                        //     => tp ≡ P_lo + (δ + j) (mod B^m),  err := δ + j ∈ {-1,0,1,2}
                        //   B^2 = 1e8 >> 2, 故 err = (tp - P) mod B^2 取有符号代表即唯一确定;
                        //   而 P mod B^2 = (qhat*d) mod B^2 只需各取 2 个 limb。
                        //   相比 GMP: 双向正确, 且省掉 O(cmp_len) 的 mpn_cmp。
                        constexpr uint64_t B2 = uint64_t(BASE) * uint64_t(BASE);
                        uint64_t _q0 = qhat_span.size > 0 ? uint64_t(qhat_span[0]) : 0;
                        uint64_t _q1 = qhat_span.size > 1 ? uint64_t(qhat_span[1]) : 0;
                        uint64_t _d0 = uint64_t(divisor[0]);
                        uint64_t _d1 = divisor.size > 1 ? uint64_t(divisor[1]) : 0;
                        uint64_t _pmod = (_q0 * _d0 + (_q0 * _d1 + _q1 * _d0) * uint64_t(BASE)) % B2;
                        uint64_t _tmod = (uint64_t(prod_mod_span[0])
                                          + uint64_t(prod_mod_span[1]) * uint64_t(BASE)) % B2;
                        int64_t _err = int64_t((_tmod + B2 - _pmod) % B2);
                        if (_err > int64_t(B2 / 2)) _err -= int64_t(B2);
                        // ★ D14: 恢复 GMP 的精确 unwrap 修正 (早期被禁用)。
                        //
                        //   为什么必须有它 —— 推导:
                        //     P = qhat*d, 且 R*B^in + np_new = P + R_new  (0 <= R_new < d)
                        //     => P = R*B^in + (np_new - R_new),  |np_new - R_new| < B^in
                        //     故 P div B^m 与 (R*B^in) div B^m = R_hi 至多差 1;
                        //     差 1 <=> R*B^in 的低 m 位距 0 或 B^m 不到 B^in
                        //          <=> R 的低 (m-in) 位全 0 或全 (BASE-1)。
                        //   => 随机输入下概率 ~ B^-(m-in) ≈ 0 (所以旧代码"看着没事"),
                        //      但**全 9 / 10^k 这类对抗输入必然命中** —— 正是 D12 fuzz 回归
                        //      (l2=8192/8193, a=全9, b=全9|pow10 商错) 的签名。
                        //   漏掉它 => tp = P_low - 1 => 余数偏大 1; r-based 修正循环只能整
                        //      "除数"地加减, 修不了 1 => 该块余数错 => 后续块全错。
                        //
                        //   为什么当年"方向错误": 那时 `borrow` 还没被正确捕获 (见上方
                        //   "FIX: 捕获 absSub1 返回值" —— 该修复是后来才补的), cy 恒等于
                        //   第一段借位, cx-cy 自然算错。cy 已修正, 现在可以恢复。
                        //
                        //   D14 恢复它的目的: unwrap 精确后, cyclic 闸门才敢从 int_ceil2
                        //   口径放宽到真实 fft3 档位 (见 cyclic_m_gate), length_ratio_integer_00
                        //   这类 (int_ceil2(166668)=262144, 57% 浪费) 才能吃到 196608 半尺寸卷积。
                        // UNWRAP_MODE: 0=不修正(D13 行为) 1=只加(GMP 假定 cx>=cy) 2=有符号
#ifndef UNWRAP_MODE
#define UNWRAP_MODE 0
#endif
#ifdef UNWRAP_PROBE
                        {
                            // ground truth: 用 basicMul 算精确 P = qhat*divisor, 取低 cyclic_m 位
                            // 与当前 tp 比对, 反推"真正需要的 incr"。仅小规模启用 (O(n^2))。
                            int _true = 99;  // 99 = 未测
                            if (cyclic_m <= 8192)
                            {
                                size_t _pl = len2 + qhat_span.size;
                                std::vector<Limb> _ex(_pl + 2, Limb(0));
                                basicMul(View(divisor.ptr, divisor.size),
                                         View(qhat_span.ptr, qhat_span.size),
                                         Span(_ex.data(), _pl));
                                View _plo(_ex.data(), cyclic_m);
                                std::vector<Limb> _t(cyclic_m);
                                if (absCompare(_plo, View(prod_mod_span.ptr, cyclic_m)) == 0)
                                {
                                    _true = 0;
                                }
                                else
                                {
                                    std::memcpy(_t.data(), prod_mod_span.ptr, cyclic_m * sizeof(Limb));
                                    Span _ts(_t.data(), cyclic_m);
                                    absAdd1(_ts, 1, _ts);
                                    if (absCompare(_plo, View(_t.data(), cyclic_m)) == 0)
                                    {
                                        _true = 1;
                                    }
                                    else
                                    {
                                        std::memcpy(_t.data(), prod_mod_span.ptr, cyclic_m * sizeof(Limb));
                                        absSub1(_ts, 1, _ts);
                                        if (absCompare(_plo, View(_t.data(), cyclic_m)) == 0)
                                            _true = -1;
                                    }
                                }
                            }
                            // 对照 GMP 的旧判据, 量化其误报
                            size_t _cmp_len = cyclic_m - len2;
                            bool _cx = (absCompare(View(window.ptr + len2, _cmp_len),
                                                   View(tprod.data() + len2, _cmp_len)) < 0);
                            int _gmp_incr = int(_cx) - int(borrow);
                            // 一致性: _err 应恒等于 -_true
                            if (_err != 0 || _true != 0 || _gmp_incr != 0)
                                std::fprintf(stderr,
                                             "[unwrap] len2=%zu this_in=%zu m=%zu wn=%zu "
                                             "gmp_incr=%d err=%d TRUE=%d\n",
                                             len2, this_in, cyclic_m, wn, _gmp_incr,
                                             int(_err), _true);
                        }
#endif
                        // 应用精确修正: tp -= err  (err ∈ {-1,0,1,2})
                        if (_err > 0)
                            absSub1(prod_mod_span, Limb(_err), prod_mod_span);
                        else if (_err < 0)
                            absAdd1(prod_mod_span, Limb(-_err), prod_mod_span);
                    }
                    // 不重建 prod 高位! 真实 prod 高 wn 位由 r-based 修正循环处理
                }
                else
#endif
                {
                    fftMulPre(qhat_span, divisor_dft_buf.data(), len2, divisor_float_len, prod_span);
                }
#ifdef PROFILE_DIV
                auto _blk_t3 = std::chrono::high_resolution_clock::now();
#ifndef DISABLE_2NXN_CYCLIC
                PROF_PRINT("  [prof]   block %zu: fftMulModBm1Pre #2 (qhat*divisor mod B^%zu-1): %.3f ms\n",
                        (qn - qn_remaining) / in, cyclic_m,
                        std::chrono::duration<double, std::milli>(_blk_t3 - _blk_t2).count());
#else
                PROF_PRINT("  [prof]   block %zu: fftMulPre #2 (qhat*divisor, fl=%zu): %.3f ms\n",
                        (qn - qn_remaining) / in, divisor_float_len,
                        std::chrono::duration<double, std::milli>(_blk_t3 - _blk_t2).count());
#endif
#endif

                // === 简单双向修正 (qhat 可能偏大或偏小) ===
                // 先处理 qhat 偏大: 若 product > window, 则 product -= divisor, qhat -= 1
                // 再计算 remainder = window - product
                // 最后处理 qhat 偏小: 若 remainder >= divisor, 则 remainder -= divisor, qhat += 1
#ifdef PROFILE_DIV
                auto _blk_t4 = std::chrono::high_resolution_clock::now();
#endif
                size_t wnd_full_len = len2 + this_in;
                size_t prod_full_len;
#ifndef DISABLE_2NXN_CYCLIC
                if (use_cyclic) {
                    prod_full_len = cyclic_m;
                } else
#endif
                {
                    prod_full_len = prod_len;
                }

#ifndef DISABLE_2NXN_CYCLIC
                if (use_cyclic)
                {
                // === cyclic 路径修正逻辑 (GMP mu_divappr_q.c L235-271, r 方法 + 双向修正) ===
                // GMP: r = rp[dn-in] - tp[dn], 反映 qhat 偏差
                //   映射: dn=len2, in=this_in, rp[dn-in]=window[len2], tp[dn]=tprod[len2]
                //   window=[np_new(this_in), rp_old(len2)] → rp_old[i]=window[this_in+i]
                //   rp[dn-in]=rp_old[len2-this_in]=window[len2]
                // GMP 保证 r >= 0 (inv 偏小 → qhat 偏小); moptm 的 absInvNewton 可能产生偏大 inv,
                //   导致 r < 0 (qhat 偏大), 需要双向修正
                int32_t r = int32_t(window[len2]) - int32_t(tprod[len2]);

                // 减法: tprod[0..len2-1] = window[0..len2-1] - tprod[0..len2-1] (GMP L239-248)
                //   window[0..this_in-1]=np_new, window[this_in..len2-1]=rp_old[0..len2-this_in-1]
                //   cy = 高位 borrow
                bool cy;
                {
                    View np_chunk(window.ptr, this_in);
                    Span tp_low(tprod.data(), this_in);
                    cy = absSub(np_chunk, tp_low, tp_low);
                    if (len2 != this_in) {
                        Span tp_high(tprod.data() + this_in, len2 - this_in);
                        View rp_low(window.ptr + this_in, len2 - this_in);
                        bool cy2 = absSub(rp_low, tp_high, tp_high);
                        bool cy3 = false;
                        if (cy) {
                            cy3 = absSub1(tp_high, 1, tp_high);
                        }
                        cy = cy2 || cy3;
                    }
                }

                // GMP L254: r -= cy
                r -= int32_t(cy);

                // 双向修正: r > 0 表示 qhat 偏小 (需 qhat+1), r < 0 表示 qhat 偏大 (需 qhat-1)
                // GMP L255-264: while (r != 0) { qhat++; rp -= divisor; r -= cy; }
                //   加 10 次限制防止 unwrap 误差导致的异常循环
                int corr_cnt = 0;
                while (r != 0 && corr_cnt < 10) {
                    if (r > 0) {
                        // qhat 偏小: qhat+1, rp -= divisor
                        absAdd1(qhat_span, 1, qhat_span);
                        bool b = absSub(Span(tprod.data(), len2), divisor, Span(tprod.data(), len2));
                        r -= int32_t(b);  // b=1 表示 rp < divisor (有借位), r 减小
                    } else {
                        // qhat 偏大: qhat-1, rp += divisor
                        absSub1(qhat_span, 1, qhat_span);
                        bool carry = absAdd(Span(tprod.data(), len2), divisor, Span(tprod.data(), len2));
                        r += int32_t(carry);  // carry=1 表示溢出 (rp 原为补码负数), r 增大
                    }
                    corr_cnt++;
                }

                // GMP L265-270: if (rp >= divisor) { qhat++; rp -= divisor; }
                if (absCompare(Span(tprod.data(), len2), divisor) >= 0) {
                    absSub(Span(tprod.data(), len2), divisor, Span(tprod.data(), len2));
                    absAdd1(qhat_span, 1, qhat_span);
                }
                }
                else
#endif
                {
                // --- 修正 1: product > window ? (qhat 偏大) ---
                int corr_down = 0;
                while (corr_down < 10) {
                    // 比较 product 和 window (只比较 window 长度的部分)
                    bool prod_gt = false;
                    // product 高位(超过 wnd_full_len 的部分)是否非零
                    if (prod_full_len > wnd_full_len) {
                        for (size_t i = wnd_full_len; i < prod_full_len; i++) {
                            if (tprod[i] != 0) { prod_gt = true; break; }
                        }
                    }
                    if (!prod_gt) {
                        int cmp = absCompare(View(tprod.data(), wnd_full_len), window);
                        prod_gt = (cmp > 0);
                    }
                    if (!prod_gt) break;
                    // product -= divisor (只减低 len2 位, 借位传播)
                    bool borrow = absSub(Span(tprod.data(), len2), divisor, Span(tprod.data(), len2));
                    size_t bi = len2;
                    while (borrow && bi < prod_full_len) {
                        if (tprod[bi] > 0) {
                            tprod[bi]--;
                            borrow = false;
                        } else {
                            tprod[bi] = Limb(BASE - 1);
                            bi++;
                        }
                    }
                    absSub1(qhat_span, 1, qhat_span);
                    corr_down++;
                }

                // --- 计算 new_rp = window - product (完整 len2+this_in 位) ---
                // 低 len2 位: tprod[0..len2-1] = window[0..len2-1] - product[0..len2-1]
                // 高 this_in 位: tprod[len2..len2+this_in-1] = window高位 - product高位 - cy
                //   (tprod[len2..] 仍是修正1后的 product 高位, 未被低 len2 位 absSub 覆盖)
                bool cy;
                {
                    View np_chunk(window.ptr, this_in);
                    Span tp_low(tprod.data(), this_in);
                    cy = absSub(np_chunk, tp_low, tp_low);
                    if (len2 != this_in) {
                        Span tp_high(tprod.data() + this_in, len2 - this_in);
                        View rp_low(window.ptr + this_in, len2 - this_in);
                        bool cy2 = absSub(rp_low, tp_high, tp_high);
                        bool cy3 = false;
                        if (cy) {
                            cy3 = absSub1(tp_high, 1, tp_high);
                        }
                        cy = cy2 || cy3;
                    }
                }
                // 计算 new_rp 高 this_in 位
                {
                    Span tp_extra(tprod.data() + len2, this_in);
                    View wnd_extra(window.ptr + len2, this_in);
                    bool cy_high = absSub(wnd_extra, tp_extra, tp_extra);
                    if (cy) {
                        bool cy3 = absSub1(tp_extra, 1, tp_extra);
                        cy_high = cy_high || cy3;
                    }
                    // cy_high 应为 false (修正1保证 product <= window)
                }

                // --- 修正 2: remainder >= divisor ? (qhat 偏小) ---
                // FIX: 检查 new_rp 完整长度 (len2+this_in), 高位非零 → new_rp >= B^len2 > divisor
                //   原 bug: 只比较低 len2 位, 当 new_rp >= B^len2 但低 len2 位 < divisor 时不修正
                int corr_up = 0;
                while (corr_up < 10) {
                    bool rp_ge_div = false;
                    for (size_t i = len2; i < len2 + this_in; i++) {
                        if (tprod[i] != 0) { rp_ge_div = true; break; }
                    }
                    if (!rp_ge_div) {
                        rp_ge_div = (absCompare(Span(tprod.data(), len2), divisor) >= 0);
                    }
                    if (!rp_ge_div) break;
                    // new_rp -= divisor (减低 len2 位, 借位传播到高 this_in 位)
                    bool borrow = absSub(Span(tprod.data(), len2), divisor, Span(tprod.data(), len2));
                    size_t bi = len2;
                    while (borrow && bi < len2 + this_in) {
                        if (tprod[bi] > 0) { tprod[bi]--; borrow = false; }
                        else { tprod[bi] = Limb(BASE - 1); bi++; }
                    }
                    absAdd1(qhat_span, 1, qhat_span);
                    corr_up++;
                }
                }

                // --- 把新 rp 复制到 window ---
                std::copy(tprod.data(), tprod.data() + len2, window.ptr);
                window.size = len2;
#ifdef PROFILE_DIV
                auto _blk_t5 = std::chrono::high_resolution_clock::now();
                PROF_PRINT("  [prof]   block %zu: correct+sub: %.3f ms\n",
                        (qn - qn_remaining) / in,
                        std::chrono::duration<double, std::milli>(_blk_t5 - _blk_t4).count());
#endif

                // 复制 qhat 到 quotient (去掉最高位 0)
                // 修正后 qhat = Q_block < B^this_in, 所以 qhat_span[this_in] = 0
                assert(qhat_span[qhat_span.size - 1] == 0);
                qhat_span.size--;
                std::copy(qhat_span.begin(), qhat_span.end(), quot_block.begin());

                qn_remaining -= this_in;
            }

#ifdef PROFILE_DIV
            auto _mu_t3 = std::chrono::high_resolution_clock::now();
            PROF_PRINT("  [prof] absDivMu: blocks loop: %.3f ms (qn=%zu, in=%zu, blocks=%zu)\n",
                    std::chrono::duration<double, std::milli>(_mu_t3 - _mu_t2).count(), qn, in, (qn + in - 1) / in);
            PROF_PRINT("  [prof] absDivMu: total: %.3f ms (len1=%zu, len2=%zu)\n",
                    std::chrono::duration<double, std::milli>(_mu_t3 - _mu_t0).count(), len1, len2);
#endif
            // 清零高位（仅低 len2 位是有效 remainder）
            if (len2 < dividend.size) {
                std::fill(dividend.ptr + len2, dividend.ptr + dividend.size, Limb(0));
            }
        }

        static void absDivNewtonCore2(Span dividend, View divisor, Span quotient)
        {
            if (dividend.size <= divisor.size)
            {
                return;
            }
            assert(divisor.size > 0);
            size_t len1 = dividend.size, len2 = divisor.size;
            Limb divisor_high = divisor[len2 - 1];
            assert(divisor_high >= HALF_BASE);
#ifdef PROFILE_DIV
            auto _p_t0 = std::chrono::high_resolution_clock::now();
#endif
            thread_local std::vector<Limb> t_inv;
            size_t inv_size = len2 + 1;
            if (t_inv.size() < inv_size) t_inv.resize(inv_size);
            Span inv_span(t_inv.data(), inv_size);
            size_t blocks = len1 / len2, len1_rem = len2 * blocks;
            auto divid_it = dividend.ptr + (len1_rem - len2);
            auto quot_it = quotient.ptr + (len1_rem - len2);

             // 预计算 divisor_dft（同时用于 absInvNewton 的 DFT 复⽤和 fast blocks）
             thread_local AlignedVec32<double> inv_dft_buf, divisor_dft_buf;
             size_t inv_float_len = fft_ceil_lin(len2 * 2 + 1);
             size_t divisor_float_len = fft_ceil_lin(len2 * 2);
             bool has_divisor_dft = false;
              // 方案 D: blocks >= 2 启用 fast path（原为 blocks >= 3）
              if (blocks >= 2) /* if (blocks >= 3) */
              {
                  if (divisor_dft_buf.size() < divisor_float_len)
                      divisor_dft_buf.resize(divisor_float_len);
                  prepareDFT(divisor, divisor_dft_buf.data(), divisor_float_len);
                  has_divisor_dft = true;
              }
            if (has_divisor_dft)
            {
                absInvNewton(divisor, inv_span, divisor_dft_buf.data(), divisor_float_len);
            }
            else
            {
                absInvNewton(divisor, inv_span);
            }
#ifdef PROFILE_DIV
            auto _p_t1 = std::chrono::high_resolution_clock::now();
            PROF_PRINT("  [prof] absInvNewton: %.3f ms (len2=%zu, dft_reuse=%d)\n",
                    std::chrono::duration<double, std::milli>(_p_t1 - _p_t0).count(), len2, has_divisor_dft);
#endif

            // 方案 D: blocks >= 2 启用 fast path（原为 blocks >= 3）
            if (blocks >= 2) /* if (blocks >= 3) */
            {
                if (inv_dft_buf.size() < inv_float_len)
                    inv_dft_buf.resize(inv_float_len);
                prepareDFT(inv_span, inv_dft_buf.data(), inv_float_len);
#ifdef PROFILE_DIV
                auto _p_t2 = std::chrono::high_resolution_clock::now();
                PROF_PRINT("  [prof] prepareDFT: %.3f ms (inv_float_len=%zu, divisor_float_len=%zu)\n",
                        std::chrono::duration<double, std::milli>(_p_t2 - _p_t1).count(), inv_float_len, divisor_float_len);
                auto _p_t3 = std::chrono::high_resolution_clock::now();
#endif
                absDivNewtonWithInvFast(dividend + (len1_rem - len2), divisor, quotient + (len1_rem - len2), inv_span, inv_dft_buf.data(), inv_float_len, divisor_dft_buf.data(), divisor_float_len);
                while (divid_it > dividend.ptr)
                {
                    divid_it -= len2;
                    quot_it -= len2;
                    absDivNewtonWithInvFast(Span(divid_it, len2 * 2), divisor, Span(quot_it, len2), inv_span, inv_dft_buf.data(), inv_float_len, divisor_dft_buf.data(), divisor_float_len);
                }
#ifdef PROFILE_DIV
                auto _p_t4 = std::chrono::high_resolution_clock::now();
                PROF_PRINT("  [prof] Core2 loop: %.3f ms (%zu blocks, %zu fftMulPre calls)\n",
                        std::chrono::duration<double, std::milli>(_p_t4 - _p_t3).count(), blocks, size_t(blocks * 2));
                PROF_PRINT("  [prof] Total Core2: %.3f ms (len1=%zu, len2=%zu)\n",
                        std::chrono::duration<double, std::milli>(_p_t4 - _p_t0).count(), len1, len2);
#endif
            }
            else
            {
#ifdef PROFILE_DIV
                auto _p_t3 = std::chrono::high_resolution_clock::now();
#endif
                absDivNewtonWithInv(dividend + (len1_rem - len2), divisor, quotient + (len1_rem - len2), inv_span);
                while (divid_it > dividend.ptr)
                {
                    divid_it -= len2;
                    quot_it -= len2;
                    absDivNewtonWithInv(Span(divid_it, len2 * 2), divisor, Span(quot_it, len2), inv_span);
                }
#ifdef PROFILE_DIV
                auto _p_t4 = std::chrono::high_resolution_clock::now();
                PROF_PRINT("  [prof] Core1 loop (slow): %.3f ms (%zu blocks)\n",
                        std::chrono::duration<double, std::milli>(_p_t4 - _p_t3).count(), blocks);
                PROF_PRINT("  [prof] Total Core2: %.3f ms (len1=%zu, len2=%zu)\n",
                        std::chrono::duration<double, std::milli>(_p_t4 - _p_t0).count(), len1, len2);
#endif
            }
        }
            
        void absDivRem(const Integer &divisor, Integer &quotient, Integer &remainder) const
        {
            size_t len1 = this->length(), len2 = divisor.length();
            int cmp = absCompare(this->getView(), divisor.getView());
            if (cmp == 0)
            {
                quotient = Limb(1);
                remainder = Limb(0);
            }
            else if (cmp < 0)
            {
                quotient = Limb(0);
                remainder = *this;
            }
            else if (len2 == 1)
            {
                quotient = *this;
                remainder = quotient.selfDivRem1(divisor.data[0]);
            }
            else
            {
                
                Limb factor = divisorNormalizeFactor(divisor.getView());
                Integer dividend_norm, divisor_norm;
                if (factor == 1)
                {
                    dividend_norm = *this;
                    divisor_norm = divisor;
                }
                else
                {
                    dividend_norm = (*this) * factor;
                    divisor_norm = divisor * factor;
                }
                size_t len1 = dividend_norm.length(), len2 = divisor_norm.length();
                assert(len2 == divisor.length());
                size_t quot_len = len1 - len2 + 1;
                quotient.data.resize(quot_len);
                Span dividend_span = dividend_norm.getSpan(), divisor_span = divisor_norm.getSpan();
                Span high = dividend_span + (quot_len - 1); 
                if (absCompare(View(high), View(divisor_span)) >= 0)
                {
                    quotient.data[quot_len - 1] = 1;
                    absSub(high, divisor_span, high);
                }
                else
                {
                    quotient.data[quot_len - 1] = 0;
                }
                
                Span quot_span(quotient.data.data(), len1 - len2);
                if (len2 <= 64 || (len1 - len2) <= 64)
                {
                    absDivBasicCore(dividend_span, divisor_span, quot_span);
                }
                else if (len1 < len2 * 2)
                {
                    absDivNewtonCore1(dividend_span, divisor_span, quot_span);
                }
                else
                {
                    // GMP mu_div_qr 风格: 计算近似逆精度 in
                    size_t qn_mu = len1 - len2;
                    size_t mu_in;
                    if (qn_mu > len2)
                    {
                        // GMP: nb = ceil(qn/len2) 的下界, in = ceil(qn/nb) <= len2
                        size_t nb = (qn_mu - 1) / len2 + 1;
#ifdef DIV_MU_NB_DELTA
                        // 实验开关: 强制多切 DELTA 块 —— 用来测"倒数变便宜 vs 块数变多"的权衡
                        nb += (size_t)(DIV_MU_NB_DELTA);
#else
                        // ================= D13: nb 代价模型 =================
                        // GMP 的 nb = ceil(qn/len2) 只是「in <= len2」的**可行性下界**, 没有任何
                        // 代价比较。在本项目的**离散档位阶梯** {2^k, 3*2^k} 上这会踩坑:
                        // in 稍微偏大就把 Fi 顶上一个档位, 而 Fi 同时驱动
                        //   (a) 整个 Newton 倒数, 以及 (b) **每个块的第一次乘法**
                        //       (absDivNewtonWithInvFast 的 fftMulPre 用的是 inv_float_len!)
                        // 所以 Fi 掉一档的收益常常远大于多切一块的开销。
                        //
                        // 实测锚点 a_max_b_random_02 (len1=500001, len2=206025):
                        //   nb=2: in=146988 -> 2*in+1=293977 -> Fi=393216 (浪费 33.7%)
                        //   nb=3: in= 97992 -> 2*in+1=195985 -> Fi=196608 (浪费  0.3%)
                        //   块数 2->3 (+50%), 但倒数 26.4->14.0 ms, 总耗时 -21.6%。
                        //
                        // 代价模型 (cyclic 情形, INVPROF 逐层标定):
                        //   cost = A*W(Fi) + W(Fi) + W(Fc) + nblk*(2W(Fi) + 2W(Fc)),  W(N)=N*log2 N
                        //   A≈6.8 = Newton 倒数相对「顶层一次变换」的倍数 (实测 6.8~7.0)
                        // 适用域: 仅大规模 (in>=16384)。小规模 (burnikel/r_nearly_zero/medium,
                        //   len2<=11025) 上常数不成立, 模型会给出 -30% 的假收益而实测 0% ->
                        //   直接门限排除, 保持 GMP 原行为。全 26 例实测 argmin 在门限内 100% 命中。
                        // 安全边际: 只有当候选 cost < 0.95*cost(nb_min) 才偏离 GMP 下界,
                        //   避免 length_ratio_integer_03 那种 0.99 平局被噪声带偏 (实测 +3.5%)。
                        // 正确性: nb 是纯自由参数 (in 仍 <= len2, 下游一切不变),
                        //   delta 0..4 全 26 例逐字节一致 -> 零风险。
#ifndef DIV_MU_NB_MODEL_OFF
                        {
                            const size_t in_nat = (qn_mu - 1) / nb + 1;
                            if (in_nat >= 16384)
                            {
                                // D16: 代价函数抽到 muBlockCost (与第二分支共用同一份口径)
                                auto cost_of = [&](size_t nbc) -> double {
                                    return muBlockCost(qn_mu, len2, (qn_mu - 1) / nbc + 1);
                                };
                                const double base = cost_of(nb);
                                size_t best_nb = nb;
                                double best_c = base;
                                for (size_t c = nb + 1; c <= nb + 4; c++)
                                {
                                    const double cc = cost_of(c);
                                    if (cc < best_c) { best_c = cc; best_nb = c; }
                                }
                                if (best_nb != nb && best_c < base * 0.95) nb = best_nb;
                            }
                        }
#endif
#endif
                        mu_in = (qn_mu - 1) / nb + 1;
                    }
                    else if (3 * qn_mu > len2)
                    {
                        // 自适应 mu_in 选择: 比较 2块 vs 4块 的 divisor FFT size
                        // profiling 发现: blocks loop FFT 与 in 有关 (非原注释所述"无关")
                        // 当 4块的 fft_ceil(len2+in4) < 2块的 fft_ceil(len2+in2) 时,
                        //   FFT size 减半的收益 > 块数翻倍的开销, 用 4块
                        // 否则用 2块, 避免无效增加块数
                        size_t in2 = (qn_mu - 1) / 2 + 1;
                        size_t in4 = (qn_mu - 1) / 4 + 1;
                        if (in2 > len2) in2 = len2;
                        if (in4 > len2) in4 = len2;
#ifdef DIV_MU_IN_QUARTER
                        mu_in = in4;  // 强制 4 块
#elif defined(DIV_MU_IN_HALF)
                        mu_in = in2;  // 强制 2 块 (旧版 default)
#else
                        size_t div_fl2 = fft_ceil_lin_q(len2 + in2);
                        size_t div_fl4 = fft_ceil_lin_q(len2 + in4);
                        mu_in = (div_fl4 < div_fl2) ? in4 : in2;
                        // ★ D16: 这个 2-vs-4 的判据只看 Fl = fft_ceil(len2+in), 完全无视
                        //   (a) Fi = fft_ceil(2*in+1) —— 它同时驱动整个 Newton 倒数**和**
                        //       每块的第一次乘法, 是真正的双重杠杆;
                        //   (b) 走 cyclic 时第二次乘法用的是 Fc≈Fl/2 而不是 Fl。
                        //   => 判据与真实成本脱节。用与第一分支同一份 muBlockCost 重选。
                        //   实测锚点 length_ratio_integer_00 (len1=333334, len2=166667,
                        //   qn_mu=166667 == len2 故落在本分支):
                        //     旧: in2=83334 (2块), Fi=196608, cost 5.81e7
                        //     新: nb=3 -> in=55556, Fi=131072 (降一档!), cost 5.50e7
                        //     ratio 0.946 < 0.95 -> 触发切换。
                        //   门限/边际与第一分支一致: 仅大规模 (>=16384) 生效, 且必须
                        //   优于原选择 5% 以上才偏离, 小用例保持 GMP 原行为。
#ifndef DIV_MU_NB_MODEL_OFF
                        if (mu_in >= 16384)
                        {
                            const double base = muBlockCost(qn_mu, len2, mu_in);
                            size_t best_in = mu_in;
                            double best_c = base;
                            for (size_t nbc = 2; nbc <= 8; nbc++)
                            {
                                size_t inc = (qn_mu - 1) / nbc + 1;
                                if (inc > len2) inc = len2;
                                const double cc = muBlockCost(qn_mu, len2, inc);
                                if (cc < best_c) { best_c = cc; best_in = inc; }
                            }
                            if (best_in != mu_in && best_c < base * 0.95) mu_in = best_in;
                        }
#endif
#endif
                    }
                    else
                    {
                        mu_in = qn_mu;
                    }
                    // in=len2 + cyclic 路径的 r-based 修正在多块场景不可靠
                    // (divisor[len2]=0 → qhat偏小1时 r 可能=0, FFT精度加剧)
                    // 限制: mu_in==len2 时仅 est_blocks<=10 走 absDivMu, 否则回退 Core2
                    // FFT 精度限制: in < 64 时 FFT 点数少, 浮点舍入差异(LC g++ 11.4)可能导致
                    //   qhat 最高位 ±1 偏差 → B^this_in 级别误差, r-based 修正循环无法处理
                    //   burnikel_ziegler_bound 用例(in=22/52)在 LC 上触发此问题
                    // D4: 去除 Core2 兜底 — 多块除法一律走 absDivMu.
                    //   allow_cyclic: 仅当自然 mu_in>=64 (D3 已验证 cyclic 安全) 才用半尺寸循环卷积;
                    //     自然 mu_in<64 (如 burnikel_ziegler_bound in=22/52) 走精确线性卷积, 避免 cyclic
                    //     unwrap 在对抗性余数下出错 —— 这正是原 Core2 兜底所掩盖的 latent bug.
                    //   in 钳制 [64,len2]: 地板 64 避免 in 过小导致"多小块"反而比 Core2 慢; 封顶 len2 是结构限制.
                    //   正确性: in>=块长 → Newton 商误差界 qhat∈±1; 线性卷积精确; cyclic 仅对 mu_in>=64 启用(已验证).
                    size_t in_used = mu_in;
                    if (in_used > len2) in_used = len2;
                    bool allow_cyclic = (mu_in >= 64);
                    if (in_used < 64)   in_used = 64;
                    absDivMu(dividend_span, divisor_span, quot_span, in_used, allow_cyclic);
                }
                dividend_norm.removeLeadingZero();
                
                if (factor != 1)
                {
                    Limb rem = dividend_norm.selfDivRem1(factor);
                    assert(rem == 0);
                }
                remainder = std::move(dividend_norm);
                remainder.removeLeadingZero();
                quotient.removeLeadingZero();
            }
        }
        static Limb divisorNormalizeFactor(View divisor)
        {
            assert(divisor.size > 0);
            constexpr int HALF_BASE_BITS = hint_bit_length<uint32_t>(HALF_BASE);
            Limb high_limb = divisor[divisor.size - 1];
            if (high_limb >= HALF_BASE)
            {
                return 1;
            }
            int bits = hint_bit_length<uint32_t>(high_limb);
            int shift = HALF_BASE_BITS - bits;
            Limb2 carry = 0;
            for (size_t i = 0; i < divisor.size - 1; i++)
            {
                carry += Limb2(divisor[i]) << shift;
                carry /= BASE;
            }
            carry += Limb2(high_limb) << shift;
            if (carry < HALF_BASE)
            {
                shift++;
            }
            else if (carry >= BASE)
            {
                shift--;
            }
            return Limb(1) << shift;
        }

        Integer &operator+=(const Integer &input)
        {
            return this->add(input.getView(), input.isNeg());
        }
        Integer &operator-=(const Integer &input)
        {
            return this->add(input.getView(), !input.isNeg());
        }
        Integer &operator*=(const Integer &input)
        {
            if (input.isZero() || this->isZero())
            {
                this->clear();
            }
            else
            {
                size_t len1 = this->length(), len2 = input.length();
                this->data.resize(len1 + len2);
                absMul(Span(this->data.data(), len1), input.getView(), this->getSpan());
            }
            this->setSign(this->isNeg() != input.isNeg());
            this->removeLeadingZero();
            return *this;
        }
        Integer &operator*=(Limb input)
        {
            if (input == 0)
            {
                this->clear();
            }
            else if (input > 1)
            {
                assert(input < BASE);
                Limb carry = absMul1(this->getSpan(), input, this->getSpan());
                if (carry > 0)
                {
                    this->data.push_back(carry);
                }
            }
            this->removeLeadingZero();
            return *this;
        }
        Integer &operator/=(const Integer &input)
        {
            Integer quotient, remainder;
            this->absDivRem(input, quotient, remainder);
            if (this->isNeg() == input.isNeg())
            {
                quotient.setSign(false);
            }
            else
            {
                if (!remainder.isZero())
                {
                    quotient += Limb(1);
                }
                quotient.setSign(true);
            }
            quotient.removeLeadingZero();
            *this = std::move(quotient);
            return *this;
        }
        Integer &operator%=(const Integer &input)
        {
            Integer quotient, remainder;
            this->absDivRem(input, quotient, remainder);
            remainder.setSign(input.isNeg());
            if ((!remainder.isZero()) && (this->isNeg() != input.isNeg()))
            {
                remainder = input - remainder;
            }
            remainder.removeLeadingZero();
            *this = std::move(remainder);
            return *this;
        }

        friend Integer operator+(Integer lhs, const Integer &rhs)
        {
            lhs += rhs;
            return lhs;
        }
        friend Integer operator-(Integer lhs, const Integer &rhs)
        {
            lhs -= rhs;
            return lhs;
        }
        friend Integer operator*(Integer lhs, const Integer &rhs)
        {
            lhs *= rhs;
            return lhs;
        }
        friend Integer operator/(Integer lhs, const Integer &rhs)
        {
            lhs /= rhs;
            return lhs;
        }
        friend Integer operator%(Integer lhs, const Integer &rhs)
        {
            lhs %= rhs;
            return lhs;
        }

        friend Integer operator*(Integer lhs, Limb rhs)
        {
            if (rhs == 0)
            {
                return Integer(0);
            }
            if (rhs > 1)
            {
                assert(rhs < BASE);
                Span lhs_span = lhs.getSpan();
                Limb carry = absMul1(lhs_span, rhs, lhs_span);
                if (carry != 0)
                {
                    lhs.data.push_back(carry);
                }
            }
            lhs.removeLeadingZero();
            return lhs;
        }

    private:
        DataVec data;
        bool sign;
    };

    // Force emission of global symbols for static member functions
    // that would otherwise be inlined-only in this TU.
    [[gnu::unused]] volatile auto _hint_export_absMul  = Integer::absMul;
    [[gnu::unused]] volatile auto _hint_export_absSub  = Integer::absSub;
    [[gnu::unused]] volatile auto _hint_export_absAdd  = Integer::absAdd;
    [[gnu::unused]] volatile auto _hint_export_absSub1 = Integer::absSub1;
    [[gnu::unused]] volatile auto _hint_export_absAdd1 = Integer::absAdd1;
    [[gnu::unused]] volatile auto _hint_export_absCompare = Integer::absCompare;
    [[gnu::unused]] volatile auto _hint_export_absSqr = Integer::absSqr;

    static const Integer fib_table[] = {0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89,
                                        144, 233, 377, 610, 987, 1597, 2584, 4181, 6765};
    constexpr size_t table_size = sizeof(fib_table) / sizeof(Integer);
    
    void fib1(size_t n, Integer &fib_n, Integer &fib_n1)
    {
        if (n < table_size - 1)
        {
            fib_n = fib_table[n];
            fib_n1 = fib_table[n + 1];
            return;
        }
        Integer fib_m, fib_m_p1;
        
        
        
        
        
        
        
        size_t m = n / 2;
        fib1(m, fib_m, fib_m_p1);
        fib_m_p1 *= fib_m; 
        fib_m.square();    
        fib_n = fib_m_p1 + fib_m_p1 - fib_m;
        fib_n1 = fib_m_p1 + fib_m + fib_m;
        fib_n1 += Integer(m % 2 == 0 ? 1 : -1);
        if (n % 2 == 1)
        {
            std::swap(fib_n, fib_n1);
            fib_n1 += fib_n;
        }
    }
    Integer fib1(size_t n)
    {
        if (n < table_size)
        {
            return fib_table[n];
        }
        Integer fib_m, fib_m_p1;
        fib1(n / 2, fib_m, fib_m_p1);
        if (n % 2 == 0)
        {
            fib_m_p1 += fib_m_p1;
            return (fib_m_p1 -= fib_m) * fib_m;
        }
        return fib_m.square() + fib_m_p1.square();
    }

    
    void fib2(size_t n, Integer &fib_n, Integer &fib_n_m1)
    {
        if (n < table_size)
        {
            fib_n = fib_table[n];
            fib_n_m1 = fib_table[n - 1];
            return;
        }
        Integer fib_m, fib_m_m1;
        size_t m = n / 2;
        fib2(m, fib_m, fib_m_m1);
        fib_m_m1.square(); 
        fib_m.square();    
        
        
        fib_n = fib_m * 4 - fib_m_m1;
        fib_n += Integer(m % 2 == 0 ? 2 : -2);
        fib_n_m1 = fib_m_m1 + fib_m;
        if (n % 2 == 1)
        {
            fib_n_m1 = fib_n - fib_n_m1;
        }
        else
        {
            fib_n -= fib_n_m1;
        }
    }
    Integer fib2(size_t n)
    {
        if (n < table_size)
        {
            return fib_table[n];
        }
        Integer fib_m, fib_m_m1;
        fib2(n / 2, fib_m, fib_m_m1);
        if (n % 2 == 0)
        {
            return fib_m * (fib_m + fib_m_m1 * 2);
        }
        fib_m *= 2;
        return (fib_m + fib_m_m1) * (fib_m - fib_m_m1) + Integer((n / 2) % 2 == 0 ? 2 : -2);
    }
}

#endif

// === 快速输出 (oBuffer 零拷贝, 查表替代除法) ===
#include <cstdio>
#include <cstring>
#ifdef __linux__
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#endif
namespace {
#if defined(HINT_OP_DIV)
    static constexpr size_t OBUF_SIZE = 32 << 20;  // 32MB (DIV T≤2×10⁶ 无总字符数限制)
#else
    static constexpr size_t OBUF_SIZE = 8 << 20;   // 8MB (ADD/MUL 总输出 ≤4MB)
#endif
    // oBuffer: prefer mmap + MADV_HUGEPAGE (set in initInput) to reduce TLB miss;
    // fallback to static array if mmap unavailable. Verified 5-6% gain in fast_io bench.
    static char oBufferFallback[OBUF_SIZE];
    static char* oBuffer = oBufferFallback;
    static char* oCursor = oBufferFallback;

    void writeHint(const hint::Integer& val) {
        oCursor += val.writeTo(oCursor);
    }

    void flushOutput() {
        // 直接 write() syscall, 绕过 fwrite 的 libc 内部缓冲 (默认可能仅 4KB/8KB),
        // 避免大输出时多次 write() 调用
#ifdef __linux__
        ssize_t total = oCursor - oBuffer;
        const char* ptr = oBuffer;
        while (total > 0) {
            ssize_t n = write(STDOUT_FILENO, ptr, total);
            if (n <= 0) break;
            ptr += n;
            total -= n;
        }
#else
        std::fwrite(oBuffer, 1, oCursor - oBuffer, stdout);
#endif
    }

    // === 快速输入 (mmap 零拷贝 Linux / fread 一次性 Windows fallback) ===
    // 对齐 best/add.cpp 策略: SWAR 64-bit 批量找分隔符, 绕过 cin 流提取
    static char iBuffer[8 << 20];  // 8MB (LC 总输入 ≤ 4MB)
    static const char *iCursor = iBuffer, *iEnd = iBuffer;

    static void initInput() {
        // FTZ + DAZ: 防止 FFT 蝶形产生 denormal 浮点数 (x86 处理 denormal 慢 20-50x)
        // FTZ (Flush-To-Zero): 输出 denormal → 0; DAZ (Denormals-Are-Zero): 输入 denormal → 0
        // 安全性: FFT 输入 uint16→double (0~9999), twiddle cos/sin ([-1,1]) 均非 denormal;
        //         denormal 只在蝶形相近值相减时出现, 此时值已极小, 对结果贡献可忽略 (FFTW 默认启用)
        _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
        _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#ifdef __linux__
        // Upgrade output buffer to mmap + MADV_HUGEPAGE (reduces TLB miss for large
        // output buffers; 32MB DIV case benefits most). Only upgrade once.
        if (oBuffer == oBufferFallback) {
            void *m = mmap(nullptr, OBUF_SIZE, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (m != MAP_FAILED) {
                madvise(m, OBUF_SIZE, MADV_HUGEPAGE);
                oBuffer = static_cast<char *>(m);
                oCursor = oBuffer;
            }
        }
        // === Input mmap (zero-copy) ===
        struct stat status;
        if (fstat(STDIN_FILENO, &status) == 0 && status.st_size > 0) {
            void *p = mmap(nullptr, status.st_size, PROT_READ, MAP_PRIVATE, STDIN_FILENO, 0);
            if (p != MAP_FAILED) {
                // Hint kernel: sequential access + prefetch (reduces TLB miss / page fault stall)
                madvise(p, status.st_size, MADV_SEQUENTIAL | MADV_WILLNEED);
                iCursor = reinterpret_cast<const char *>(p);
                iEnd = iCursor + status.st_size;
                return;
            }
        }
#endif
        size_t n = std::fread(iBuffer, 1, sizeof(iBuffer) - 1, stdin);
        iBuffer[n] = '\n';  // 哨兵, 确保 SWAR 能终止
        iCursor = iBuffer;
        iEnd = iBuffer + n;
    }

    // SWAR 64-bit: 找从 p 起第一个 < 0x21 的字节 (空白/换行/制表), 返回 token 长度
    // 对齐 best/add.cpp readFromCursor 的位运算技巧
    // 边界安全: SWAR 仅在剩余 >=8 字节时运行; 末尾 0-7 字节走标量回退,
    //           避免 mmap 末页 (文件大小恰为页整数倍且无尾换行) 越界读
    static inline size_t swarTokenLen(const char *p) {
        size_t offset = 0;
        while (p + offset + sizeof(uint64_t) <= iEnd) {
            uint64_t data;
            std::memcpy(&data, p + offset, sizeof(data));
            uint64_t mask = (~data) & (data - 0x2121212121212121ULL) & 0x8080808080808080ULL;
            if (mask) {
                return offset + size_t(__builtin_ctzll(mask)) / 8;
            }
            offset += 8;
        }
        // 标量回退: 末尾 0-7 字节
        while (p + offset < iEnd && p[offset] >= 0x21) {
            offset++;
        }
        return offset;
    }

    // Unchecked variant: skips digit validation (caller guarantees digits)
    // Used when readToken already verified token boundaries on well-formed input
    static inline bool tryParseI64Unchecked(const char *start, size_t len, int64_t &val) {
        if (len == 0 || len > 19) return false;
        bool neg = false;
        size_t i = 0;
        if (start[0] == '-') {
            neg = true;
            i = 1;
            if (len == 1) return false;
        }
        size_t digit_len = len - i;
        if (digit_len == 0 || digit_len > 18) return false;
        // 4-byte grouped parse (str4toi reduces 64-bit multiply count)
        int64_t v = 0;
        while (len - i >= 4) {
            v = v * 10000 + hint::str4toi(start + i);
            i += 4;
        }
        while (i < len) {
            v = v * 10 + (start[i] - '0');
            i++;
        }
        val = neg ? -v : v;
        return true;
    }

    // SWAR parse 8 ASCII digits from uint64 value (no memcpy needed)
    // Daniel Lemire technique: 3 multiplies to combine 8 digit nibbles
    static inline uint64_t parse8SWAR_u64(uint64_t u) {
        u = (u & 0x0F0F0F0F0F0F0F0FULL) * 2561ULL;
        u = ((u >> 8) & 0x00FF00FF00FF00FFULL) * 6553601ULL;
        u = ((u >> 16) & 0x0000FFFF0000FFFFULL) * 42949672960001ULL;
        return u >> 32;
    }

    // Combined parse + boundary detection: parse positive integer from s,
    // stop at first non-digit byte, set len to token length.
    // V7: 8-byte umask check + ctz length + shift-align SWAR (rogeryoungh technique)
    // umask = u & (u + 0x06..06) & 0xF0..F0; if == 0x30..30 then all 8 bytes digits
    // else dl = ctz(umask ^ 0x30..30) >> 3 = digit count; shift align + SWAR parse
    // Verified -9.8% vs V4 merged in fast_io.cpp bench (7.595 vs 8.416ms).
    static inline int64_t parsePositiveUntilNondigit(const char *s, size_t &len) {
        if (s + 8 > iEnd) {
            int64_t v = 0;
            size_t i = 0;
            while (s + i < iEnd && s[i] >= '0' && s[i] <= '9') {
                v = v * 10 + (s[i] - '0');
                i++;
            }
            len = i;
            return v;
        }

        uint64_t u;
        std::memcpy(&u, s, 8);

        constexpr uint64_t cx30 = 0x3030303030303030ULL;
        uint64_t umask = u & (u + 0x0606060606060606ULL) & 0xF0F0F0F0F0F0F0F0ULL;

        if (umask == cx30) {
            // All 8 bytes are digits
            uint64_t r = parse8SWAR_u64(u);
            size_t i = 8;

            if (s + 16 > iEnd) {
                while (s + i < iEnd && s[i] >= '0' && s[i] <= '9') {
                    r = r * 10 + (s[i] - '0');
                    i++;
                }
                len = i;
                return (int64_t)r;
            }

            uint64_t u2;
            std::memcpy(&u2, s + 8, 8);
            uint64_t umask2 = u2 & (u2 + 0x0606060606060606ULL) & 0xF0F0F0F0F0F0F0F0ULL;

            if (umask2 == cx30) {
                // 16 digits
                uint64_t r2 = parse8SWAR_u64(u2);
                r = r * 100000000ULL + r2;
                i = 16;
                if (s[16] >= '0' && s[16] <= '9') {
                    r = r * 10 + (s[16] - '0');
                    i = 17;
                    if (s[17] >= '0' && s[17] <= '9') {
                        r = r * 10 + (s[17] - '0');
                        i = 18;
                        while (s + i + 8 <= iEnd) {
                            uint64_t d;
                            std::memcpy(&d, s + i, 8);
                            uint64_t mask = (~d) & (d - 0x2121212121212121ULL) & 0x8080808080808080ULL;
                            if (mask) { i += size_t(__builtin_ctzll(mask)) / 8; goto done7; }
                            i += 8;
                        }
                        while (s[i] >= '0' && s[i] <= '9') i++;
                    }
                done7:
                    len = i;
                    return (int64_t)r;
                }
                len = i;
                return (int64_t)r;
            } else {
                // 8 + (0-7) digits
                uint64_t dl2 = __builtin_ctzll(umask2 ^ cx30) >> 3;
                if (dl2 > 0) {
                    u2 <<= 64 - (dl2 << 3);
                    uint64_t r2 = parse8SWAR_u64(u2);
                    static const uint64_t p10[] = {1, 10, 100, 1000, 10000,
                                                   100000, 1000000, 10000000};
                    r = r * p10[dl2] + r2;
                    i = 8 + dl2;
                }
                len = i;
                return (int64_t)r;
            }
        }

        // Not all 8 bytes are digits: ctz length + shift-align SWAR
        uint64_t dl = __builtin_ctzll(umask ^ cx30) >> 3;
        if (dl == 0) { len = 0; return 0; }
        u <<= 64 - (dl << 3);
        uint64_t r = parse8SWAR_u64(u);
        len = dl;
        return (int64_t)r;
    }

    // int64 to string, write to oBuffer (no Integer overhead)
    // OPT: 10000-base decomposition + outTable lookup, division count 18->5
    static inline void writeI64(int64_t val) {
        if (val == 0) { *oCursor++ = '0'; return; }
        uint64_t uv;
        bool neg = val < 0;
        if (neg) {
            *oCursor++ = '-';
            uv = uint64_t(-(val + 1)) + 1;
        } else {
            uv = uint64_t(val);
        }
        // 10000-base decomposition: low -> high, max 5 groups (18 digits = 4+4+4+4+2)
        uint32_t limbs[5];
        int n = 0;
        while (uv >= 10000) {
            limbs[n++] = uint32_t(uv % 10000);
            uv /= 10000;
        }
        limbs[n++] = uint32_t(uv); // highest group (1-4 digits)
        // Output highest group (no leading zeros)
        uint32_t high = limbs[n - 1];
        if (high < 10) {
            *oCursor++ = char('0' + high);
        } else if (high < 100) {
            *oCursor++ = char('0' + high / 10);
            *oCursor++ = char('0' + high % 10);
        } else if (high < 1000) {
            *oCursor++ = char('0' + high / 100);
            *oCursor++ = char('0' + high / 10 % 10);
            *oCursor++ = char('0' + high % 10);
        } else {
            std::memcpy(oCursor, &hint::outTable.t[high], 4);
            oCursor += 4;
        }
        // Output remaining groups (zero-padded, 4 bytes/table lookup)
        for (int j = n - 2; j >= 0; j--) {
            std::memcpy(oCursor, &hint::outTable.t[limbs[j]], 4);
            oCursor += 4;
        }
    }
}
int main() {
#ifdef PROFILE_DIV
    setvbuf(stderr, NULL, _IONBF, 0);
#endif
    { TP_DECL(rd); initInput(); TP_ADD(rd); }
    size_t t = 0;
    while (iCursor < iEnd && *iCursor >= '0' && *iCursor <= '9') {
        t = t * 10 + size_t(*iCursor++ - '0');
    }
    if (iCursor < iEnd && *iCursor < 0x21) iCursor++;
    hint::Integer a, b, q, r;
    int64_t va, vb;
    while (t--) {
        const char *sa = iCursor;
        size_t la;
        if (sa < iEnd && *sa == '-') {
            la = swarTokenLen(iCursor);
            iCursor += la;
        } else {
            va = parsePositiveUntilNondigit(sa, la);
            iCursor = sa + la;
        }
        if (iCursor < iEnd && *iCursor < 0x21) iCursor++;
        const char *sb = iCursor;
        size_t lb;
        if (sb < iEnd && *sb == '-') {
            lb = swarTokenLen(iCursor);
            iCursor += lb;
        } else {
            vb = parsePositiveUntilNondigit(sb, lb);
            iCursor = sb + lb;
        }
        if (iCursor < iEnd && *iCursor < 0x21) iCursor++;
        // Fast path: both positive <= 18 digits, b != 0
        if (la > 0 && la <= 18 && lb > 0 && lb <= 18 && sa[0] != '-' && sb[0] != '-' && vb != 0) {
            writeI64(va / vb);
            *oCursor++ = ' ';
            writeI64(va % vb);
        } else if (tryParseI64Unchecked(sa, la, va) && tryParseI64Unchecked(sb, lb, vb) && vb != 0) {
            // Slow int64 path: supports negatives, any int64 range
            writeI64(va / vb);
            *oCursor++ = ' ';
            writeI64(va % vb);
        } else {
            // BigInt path
            { TP_DECL(parse);
              a.fromCharRange(sa, sa + la);
              b.fromCharRange(sb, sb + lb);
              TP_ADD(parse); }
            { TP_DECL(div);
              a.absDivRem(b, q, r);
              TP_ADD(div); }
            { TP_DECL(print);
              writeHint(q);
              *oCursor++ = ' ';
              writeHint(r);
              TP_ADD(print); }
        }
        *oCursor++ = '\n';
    }
    { TP_DECL(wr); flushOutput(); TP_ADD(wr); }
    return 0;
}
