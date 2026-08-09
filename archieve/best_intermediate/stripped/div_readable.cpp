







#define HINT_OP_DIV
#define ENABLE_FFT5 1
#define NDEBUG






#pragma GCC target("avx2,bmi,bmi2,popcnt,lzcnt")








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
#include <immintrin.h>












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
    static constexpr size_t HP = size_t(2) << 20;             
    static constexpr size_t ARENA_SIZE = size_t(512) << 20;   
    static constexpr unsigned KMIN = 6;                       
    static constexpr unsigned KMAX = 30;                      

    static char *g_raw = nullptr;   
    static char *g_base = nullptr;  
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
        
        madvise(g_base, static_cast<size_t>(g_end - g_base) & ~(HP - 1), MADV_HUGEPAGE);
    }

    static inline bool owns(void *p) noexcept
    {
        return g_base && static_cast<char *>(p) >= g_base && static_cast<char *>(p) < g_end;
    }

    
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
            return;  
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
} 

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
#endif


#include <chrono>
#include <cstdio>
#ifdef FFTHIST
#include <map>
#include <cmath>
#include <cstdlib>
#endif
#ifdef CEILHIST


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






#ifndef FFT_FIXED_MAX
#define FFT_FIXED_MAX 32
#endif



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

    
    
    
    
    constexpr size_t FFT3_MIN = 192;
    
    constexpr size_t FFT5_MIN = 80;
    constexpr size_t FFT5_MAX = 256;  
    
    constexpr size_t fft_ceil(size_t n)
    {
        size_t p = int_ceil2(n);
#ifdef NO_FFT3
        return p; 
#else
        size_t best = p;
        size_t h = p / 4 * 3; 
        if (h >= FFT3_MIN && h >= n)
            best = h;
#ifdef ENABLE_FFT5
        
        size_t h5 = p / 8 * 5;
        if (h5 >= FFT5_MIN && h5 >= n && h5 < best && h5 <= FFT5_MAX)
            best = h5;
#endif
        return best;
#endif
    }
    
    constexpr bool is_fft3(size_t float_len)
    {
        return (float_len % 3) == 0;
    }
    constexpr bool is_fft5(size_t float_len)
    {
#ifdef ENABLE_FFT5
        
        return (float_len % 5) == 0 && (float_len % 3) != 0 && float_len >= FFT5_MIN && float_len <= FFT5_MAX;
#else
        (void)float_len;
        return false;
#endif
    }

    
    
    
    
    
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

    
    
    
    
    
    
    
    
    
    
    
    inline double muBlockCost(size_t qn_mu, size_t len2, size_t inc)
    {
        if (inc > len2 || inc < 64) return 1e300;
        
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
        const bool cyc = (gate < inc + len2);  
        const size_t Fc = cyc ? Fc_cand : Fl;
        const double wi = Wf(Fi), wc = Wf(Fc);
        return 6.8 * wi + wi + wc + (double)nblk * (2 * wi + 2 * wc);
    }

#ifdef DIV_INVDUMP

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
        
        if constexpr (sizeof(T) <= 4)
            return 31 - __builtin_clz(uint32_t(n));
        else
            return 63 - __builtin_clzll(uint64_t(n));
    }
    constexpr int hint_ctz(uint32_t x)
    {
        
        if (x == 0)
            return 32;
        return __builtin_ctz(x);
    }

    constexpr int hint_ctz(uint64_t x)
    {
        
        if (x == 0)
            return 64;
        return __builtin_ctzll(x);
    }


    constexpr int hint_clz(uint32_t x)
    {
        
        if (x == 0)
            return 32;
        return __builtin_clz(x);
    }

    constexpr int hint_clz(uint64_t x)
    {
        
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
            
            
            HINT_AI C4d c4mul(const C4d &a, const C4d &b)
            {
                return C4d{a.re * b.re - a.im * b.im, a.im * b.re + a.re * b.im};
            }
            HINT_AI C4d c4mulConj(const C4d &a, const C4d &b)
            {
                return C4d{a.re * b.re + a.im * b.im, a.im * b.re - a.re * b.im};
            }
            
            
            
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
                AlignedVec32<Float> table;  
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
                            
                            difC4<RIRI_IN ? 2 : 1>(inout, float_len);
                            return;
                        }
                    }
                    if (float_len <= FFT_FIXED_MAX)
                    {
                        difDispatch<RIRI_IN>(inout, float_len);
                        return;
                    }
                    
                    
                    
                    const size_t fft_len = float_len / 2, c2_len = fft_len / 2;
                    const size_t stride1 = c2_len / 4, stride2 = stride1 * 2, stride3 = stride1 * 3;
                    
                    
                    
                    auto tp1 = reinterpret_cast<const C2 *>(__builtin_assume_aligned(table1.getBegin(fft_len), 32));
                    auto tp3 = reinterpret_cast<const C2 *>(__builtin_assume_aligned(table3.getBegin(fft_len), 32));
                    auto it = reinterpret_cast<C2 *>(__builtin_assume_aligned(inout, 32));
                    for (auto end = it + stride1; it < end; it++, tp1++, tp3++)
                    {
                        
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
                            
                            iditC4<RIRI_OUT ? 2 : 1>(inout, float_len);
                            return;
                        }
                    }
                    if (float_len <= FFT_FIXED_MAX)
                    {
                        iditDispatch<RIRI_OUT>(inout, float_len);
                        return;
                    }
                    
                    size_t stride = float_len / 4;
                    idit<false>(reinterpret_cast<Float *>(__builtin_assume_aligned(inout, 32)), stride * 2);
                    idit<false>(reinterpret_cast<Float *>(__builtin_assume_aligned(inout + stride * 2, 32)), stride);
                    idit<false>(reinterpret_cast<Float *>(__builtin_assume_aligned(inout + stride * 3, 32)), stride);
                    const size_t fft_len = float_len / 2, c2_len = fft_len / 2;
                    const size_t stride1 = c2_len / 4, stride2 = stride1 * 2, stride3 = stride1 * 3;
                    
                    auto tp1 = reinterpret_cast<const C2 *>(__builtin_assume_aligned(table1.getBegin(fft_len), 32));
                    auto tp3 = reinterpret_cast<const C2 *>(__builtin_assume_aligned(table3.getBegin(fft_len), 32));
                    auto it = reinterpret_cast<C2 *>(__builtin_assume_aligned(inout, 32));
                    for (auto end = it + stride1; it < end; it++, tp1++, tp3++)
                    {
                        
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

                
                
                
                template <int IN_MODE>
                void difC4(Float inout[], size_t float_len)
                {
                    if constexpr (std::is_same_v<Float, double>)
                    {
                        if (float_len <= FFT_C4_MIN)
                        {
                            
                            packC4(inout, float_len); 
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
                            packC4(inout, float_len); 
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

            
            
            HINT_AI C4d c4rev(const C4d &c)
            {
                return C4d{_mm256_permute4x64_pd(c.re, 0x1B), _mm256_permute4x64_pd(c.im, 0x1B)};
            }
            
            HINT_AI C4d c4loadC2Rev(const double *p)
            {
                return c4rev(c4loadC2(p));
            }
            
            HINT_AI C4d c4twiddle(const __m256d &w0, const __m256d &w1)
            {
                return C4d{_mm256_permute2f128_pd(w0, w1, 0x20), _mm256_permute2f128_pd(w0, w1, 0x31)};
            }
            
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
                        if (begin >= 32) 
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

            
            template <typename Float>
            inline FFT<Float> &getSharedFFT()
            {
                static FFT<Float> fft;
                return fft;
            }

            
            
            
            
            
            
            
            
            
            
            constexpr Float64 SQRT3_DIV2 = 0.866025403784438646763723170752936;

            template <typename Float, int FACTOR>
            struct FFTTable3
            {
                using C2 = Complex2<Float>;
                
                
                FFTTable3() : cur_m(2), table(8)
                {
                    Float theta = Float(-HINT_2PI) * FACTOR / Float(6); 
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
                        const Float *last = &table[m]; 
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

            
            template <typename Float, int FACTOR>
            struct FFTTable5
            {
                using C2 = Complex2<Float>;
                FFTTable5() : cur_m(2), table(8)
                {
                    Float theta = Float(-HINT_2PI) * FACTOR / Float(10); 
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
                        const Float *last = &table[m];
                        Float theta = Float(-HINT_2PI) * FACTOR / Float(5 * m);
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
            inline FFTTable5<Float, 1> &getTable5a()
            {
                static FFTTable5<Float, 1> t;
                return t;
            }
            template <typename Float>
            inline FFTTable5<Float, 2> &getTable5b()
            {
                static FFTTable5<Float, 2> t;
                return t;
            }
            template <typename Float>
            inline FFTTable5<Float, 3> &getTable5c()
            {
                static FFTTable5<Float, 3> t;
                return t;
            }
            template <typename Float>
            inline FFTTable5<Float, 4> &getTable5d()
            {
                static FFTTable5<Float, 4> t;
                return t;
            }

            
            template <typename Float>
            inline const Complex2<Float> *smallOmega8()
            {
                using C2 = Complex2<Float>;
                auto w = [](int e)
                { return std::polar<Float>(1, Float(-HINT_2PI) * e / 16); };
                
                static const C2 tab[4] = {
                    C2(w(0).real(), w(4).real(), w(0).imag(), w(4).imag()),
                    C2(w(2).real(), w(6).real(), w(2).imag(), w(6).imag()),
                    C2(w(1).real(), w(5).real(), w(1).imag(), w(5).imag()),
                    C2(w(3).real(), w(7).real(), w(3).imag(), w(7).imag())};
                return tab;
            }

            
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
                    
                    C2 s = a1 + a2, d = a1 - a2;
                    C2 r0 = a0 + s;
                    C2 h(a0.real - s.real * H, a0.imag - s.imag * H);
                    C2 g(d.imag * (-S), d.real * S); 
                    r0.store(b0);
                    (h - g).mul(p1[0]).store(b1);
                    (h + g).mul(p2[0]).store(b2);
                }
            }
            
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


            
            
            
            
            
            
            
            
            constexpr Float64 R5_C1 = 0.309016994374947424102293417183;
            constexpr Float64 R5_S1 = 0.951056516295153572116439333379;
            constexpr Float64 R5_C2 = -0.809016994374947424102293417183;
            constexpr Float64 R5_S2 = 0.587785252292473129078558064009;

            template <bool RIRI_IN, typename Float>
            inline void dif5Stage(Float *inout, size_t m)
            {
                using C2 = Complex2<Float>;
                const Float c1 = Float(R5_C1), s1 = Float(R5_S1);
                const Float c2 = Float(R5_C2), s2 = Float(R5_S2);
                auto &t1t = getTable5a<Float>();
                auto &t2t = getTable5b<Float>();
                auto &t3t = getTable5c<Float>();
                auto &t4t = getTable5d<Float>();
                t1t.expand(m), t2t.expand(m), t3t.expand(m), t4t.expand(m);
                auto p1 = reinterpret_cast<const C2 *>(t1t.getBegin(m));
                auto p2 = reinterpret_cast<const C2 *>(t2t.getBegin(m));
                auto p3 = reinterpret_cast<const C2 *>(t3t.getBegin(m));
                auto p4 = reinterpret_cast<const C2 *>(t4t.getBegin(m));
                Float *b0 = inout, *b1 = inout + m * 2, *b2 = inout + m * 4;
                Float *b3 = inout + m * 6, *b4 = inout + m * 8;
                for (size_t c = m / 2; c > 0; c--, b0 += 4, b1 += 4, b2 += 4, b3 += 4, b4 += 4,
                            p1++, p2++, p3++, p4++)
                {
                    C2 a0, a1, a2, a3, a4;
                    a0.load(b0), a1.load(b1), a2.load(b2), a3.load(b3), a4.load(b4);
                    if (RIRI_IN)
                    {
                        a0.permute(), a1.permute(), a2.permute(), a3.permute(), a4.permute();
                    }
                    C2 t1 = a1 + a4, t2 = a2 + a3, t3 = a1 - a4, t4 = a2 - a3;
                    C2 u1(a0.real + t1.real * c1 + t2.real * c2,
                          a0.imag + t1.imag * c1 + t2.imag * c2);
                    C2 u2(a0.real + t1.real * c2 + t2.real * c1,
                          a0.imag + t1.imag * c2 + t2.imag * c1);
                    C2 v1(t3.real * s1 + t4.real * s2, t3.imag * s1 + t4.imag * s2);
                    C2 v2(t3.real * s2 - t4.real * s1, t3.imag * s2 - t4.imag * s1);
                    C2 A0 = a0 + t1 + t2;
                    C2 A1(u1.real + v1.imag, u1.imag - v1.real);
                    C2 A4(u1.real - v1.imag, u1.imag + v1.real);
                    C2 A2(u2.real + v2.imag, u2.imag - v2.real);
                    C2 A3(u2.real - v2.imag, u2.imag + v2.real);
                    A0.store(b0);
                    A1.mul(p1[0]).store(b1);
                    A2.mul(p2[0]).store(b2);
                    A3.mul(p3[0]).store(b3);
                    A4.mul(p4[0]).store(b4);
                }
            }
            
            template <bool RIRI_OUT, typename Float>
            inline void idit5Stage(Float *inout, size_t m)
            {
                using C2 = Complex2<Float>;
                const Float c1 = Float(R5_C1), s1 = Float(R5_S1);
                const Float c2 = Float(R5_C2), s2 = Float(R5_S2);
                auto &t1t = getTable5a<Float>();
                auto &t2t = getTable5b<Float>();
                auto &t3t = getTable5c<Float>();
                auto &t4t = getTable5d<Float>();
                t1t.expand(m), t2t.expand(m), t3t.expand(m), t4t.expand(m);
                auto p1 = reinterpret_cast<const C2 *>(t1t.getBegin(m));
                auto p2 = reinterpret_cast<const C2 *>(t2t.getBegin(m));
                auto p3 = reinterpret_cast<const C2 *>(t3t.getBegin(m));
                auto p4 = reinterpret_cast<const C2 *>(t4t.getBegin(m));
                Float *b0 = inout, *b1 = inout + m * 2, *b2 = inout + m * 4;
                Float *b3 = inout + m * 6, *b4 = inout + m * 8;
                for (size_t c = m / 2; c > 0; c--, b0 += 4, b1 += 4, b2 += 4, b3 += 4, b4 += 4,
                            p1++, p2++, p3++, p4++)
                {
                    C2 y0, y1, y2, y3, y4;
                    y0.load(b0), y1.load(b1), y2.load(b2), y3.load(b3), y4.load(b4);
                    C2 x1 = y1.mulConj(p1[0]), x2 = y2.mulConj(p2[0]);
                    C2 x3 = y3.mulConj(p3[0]), x4 = y4.mulConj(p4[0]);
                    C2 t1 = x1 + x4, t2 = x2 + x3, t3 = x1 - x4, t4 = x2 - x3;
                    C2 u1(y0.real + t1.real * c1 + t2.real * c2,
                          y0.imag + t1.imag * c1 + t2.imag * c2);
                    C2 u2(y0.real + t1.real * c2 + t2.real * c1,
                          y0.imag + t1.imag * c2 + t2.imag * c1);
                    C2 v1(t3.real * s1 + t4.real * s2, t3.imag * s1 + t4.imag * s2);
                    C2 v2(t3.real * s2 - t4.real * s1, t3.imag * s2 - t4.imag * s1);
                    C2 a0 = y0 + t1 + t2;
                    C2 a1(u1.real - v1.imag, u1.imag + v1.real);
                    C2 a4(u1.real + v1.imag, u1.imag - v1.real);
                    C2 a2(u2.real - v2.imag, u2.imag + v2.real);
                    C2 a3(u2.real + v2.imag, u2.imag - v2.real);
                    if (RIRI_OUT)
                    {
                        a0.permute(), a1.permute(), a2.permute(), a3.permute(), a4.permute();
                    }
                    a0.store(b0), a1.store(b1), a2.store(b2), a3.store(b3), a4.store(b4);
                }
            }

            
            
            
            template <typename Float>
            inline void dot_cross_blocks(Float *o1, Float *o2, const Float *n1, const Float *n2,
                                         size_t blk, const Complex2<Float> &rot,
                                         const Float2<Float> &invx, Float inv4,
                                         BinRevTableC2HP<Float> &table)
            {
                using C2 = Complex2<Float>;
                {
                    const C2 *sm = smallOmega8<Float>();
                    for (size_t t = 0; t < 4; t++)
                    {
                        const size_t f = 4 * t;
                        dot_rfftX2(o1 + f, o2 + blk - 4 - f, n1 + f, n2 + blk - 4 - f,
                                   sm[t].mul(rot), invx);
                    }
                }
                for (size_t begin = 16; begin < blk; begin *= 2)
                {
                    table.reset(begin / 2);
                    if constexpr (std::is_same_v<Float, double>)
                    {
                        const __m256d invv = _mm256_set1_pd(inv4);
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

            
            
            
            
            template <typename Float>
            inline void real_dot_binrev5(Float in_out[], const Float in[], size_t float_len)
            {
                using F2 = Float2<Float>;
                using C2 = Complex2<Float>;
                const size_t m = float_len / 10, blk = m * 2;
                const Float inv = Float(1) / Float(float_len);
                const Float inv4 = Float(0.25) / Float(float_len);
                const F2 invx = F2::from1(inv4);
                static thread_local BinRevTableC2HP<Float> table(31, 32);
                
                real_dot_binrev<2>(in_out, in, 16, inv);
                for (size_t begin = 16; begin < blk; begin *= 2)
                {
                    table.reset(begin / 2);
                    if constexpr (std::is_same_v<Float, double>)
                    {
                        if (begin >= 32)
                        {
                            const __m256d invv = _mm256_set1_pd(inv4);
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
                
                const Float ang = Float(-HINT_2PI) / Float(float_len);
                const C2 rot1(Float(std::cos(ang)), Float(std::sin(ang)));
                const C2 rot2(Float(std::cos(2 * ang)), Float(std::sin(2 * ang)));
                dot_cross_blocks(in_out + blk, in_out + blk * 4, in + blk, in + blk * 4,
                                 blk, rot1, invx, inv4, table);
                dot_cross_blocks(in_out + blk * 2, in_out + blk * 3, in + blk * 2, in + blk * 3,
                                 blk, rot2, invx, inv4, table);
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
                
                
                for (size_t begin = 16; begin < blk; begin *= 2)
                {
                    table.reset(begin / 2);
                    if constexpr (std::is_same_v<Float, double>)
                    {
                        
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

            
            template <typename Float>
            inline void rdif(Float *p, size_t float_len)
            {
                FFTHIST_TICK(float_len);
                auto &fft = getSharedFFT<Float>();
                if (is_fft5(float_len))
                {
                    const size_t m5 = float_len / 10, blk5 = m5 * 2;
                    fft.expand(blk5);
                    dif5Stage<true>(p, m5);
                    for (size_t i = 0; i < 5; i++)
                    {
                        fft.template dif<false>(p + blk5 * i, blk5);
                    }
                    return;
                }
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
                if (is_fft5(float_len))
                {
                    const size_t m5 = float_len / 10, blk5 = m5 * 2;
                    fft.expand(blk5);
                    for (size_t i = 0; i < 5; i++)
                    {
                        fft.template idit<false>(p + blk5 * i, blk5);
                    }
                    idit5Stage<true>(p, m5);
                    return;
                }
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
                if (is_fft5(float_len))
                {
                    real_dot_binrev5(in_out, in, float_len);
                }
                else if (!is_fft3(float_len))
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
                assert(is_2pow(float_len) || is_fft3(float_len) || is_fft5(float_len));
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
    
    
    
    
    
    
    
    
    
    inline void str16to4limbs(const char *p, uint16_t *limbs4)
    {
        __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i *>(p));
        v = _mm_sub_epi8(v, _mm_set1_epi8('0'));
        __m128i zero = _mm_setzero_si128();
        
        __m128i lo = _mm_unpacklo_epi8(v, zero);  
        __m128i hi = _mm_unpackhi_epi8(v, zero);  
        
        __m128i mul2 = _mm_set_epi16(1, 10, 1, 10, 1, 10, 1, 10);  
        __m128i lo_s = _mm_mullo_epi16(lo, mul2);
        __m128i hi_s = _mm_mullo_epi16(hi, mul2);
        __m128i ones = _mm_set1_epi16(1);
        __m128i lo_d = _mm_madd_epi16(lo_s, ones);  
        __m128i hi_d = _mm_madd_epi16(hi_s, ones);  
        
        __m128i packed = _mm_packs_epi32(lo_d, hi_d);
        
        __m128i mul4 = _mm_set1_epi32(0x00010064);  
        __m128i v4 = _mm_madd_epi16(packed, mul4);  
        
        v4 = _mm_shuffle_epi32(v4, _MM_SHUFFLE(0, 1, 2, 3));
        
        __m128i result = _mm_packs_epi32(v4, v4);
        _mm_storel_epi64(reinterpret_cast<__m128i *>(limbs4), result);
    }
    
    
    inline void copyU16ToF64(const uint16_t *src, double *dst, size_t n)
    {
        size_t j = 0;
#if defined(__AVX2__)
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
    }
    
    
    inline void copyU16ToF64AndFill(const uint16_t *src, double *dst, size_t n, size_t total)
    {
        size_t j = 0;
#if defined(__AVX2__)
        
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
        
        while (length > 0 && HINT_UNLIKELY(array[length - 1] == 0))
        {
            length--;
        }
        return length;
    }

    
    
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
        
        
        
        
        
        
        
        
        
        static constexpr uint64_t BARRETT_M = 0x68DB8BAC710CCULL;
        static HINT_AI uint64_t divBASE(uint64_t s)
        {
#if defined(__BMI2__) && defined(__x86_64__) && defined(__GNUC__)
            
            
            
            uint64_t _lo, _hi;
            __asm__("mulx %[s], %[lo], %[hi]"
                    : [lo] "=r"(_lo), [hi] "=r"(_hi)
                    : "d"(BARRETT_M), [s] "r"(s));
            (void)_lo;
            return _hi;
#else
            return (uint64_t)((unsigned __int128)s * BARRETT_M >> 64);
#endif
        }
        
        
        
        
        
        
        
        
        
        
        
        
        static HINT_AI uint64_t cvtRoundU64(const double *p)
        {
            return (uint64_t)_mm_cvtsd_si64(_mm_load_sd(p));
        }
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

        
        size_t writeTo(char* out) const
        {
            
            
            
            
            
            
            
            
            
            
            
            
            
            
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
            
            size_t i = data.size() - 1;
            
            
            
            
            
#if defined(__AVX2__)
            while (i >= 8)
            {
                
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
            
            while (i > 0)
            {
                i--;
                std::memcpy(p, &outTable.t[data[i]], 4);
                p += 4;
            }
            return size_t(p - out);
        }
        
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
            
            
            
            while (end - p_begin >= 16)
            {
                end -= 16;
                str16to4limbs(end, data.data() + i);
                i += 4;
            }
            
            while (end - p_begin >= 8)
            {
                end -= 8;
                data[i] = str4toi(end + 4);     
                data[i + 1] = str4toi(end);     
                i += 2;
            }
            
            while (end - p_begin >= 4)
            {
                end -= 4;
                data[i] = str4toi(end);
                i++;
            }
            
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
        
        
        
#pragma GCC push_options
#pragma GCC target("avx2,fma,bmi,bmi2")
        static bool absAdd_avx2(View in1, View in2, Span out)
        {
            if (in1.size < in2.size)
            {
                std::swap(in1, in2);
            }
            size_t i = 0;
            
            
            
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
                
                __m256i t = _mm256_add_epi16(a, b);
                __m256i gm = _mm256_cmpgt_epi16(t, vbasem1); 
                __m256i pm = _mm256_cmpeq_epi16(t, vbasem1); 
                uint32_t G = _pext_u32(static_cast<uint32_t>(_mm256_movemask_epi8(gm)), 0x55555555u);
                uint32_t P = _pext_u32(static_cast<uint32_t>(_mm256_movemask_epi8(pm)), 0x55555555u);
                uint32_t A = G, B = G | P;
                uint32_t s = A + B + carry32;
                uint32_t cin = s ^ A ^ B; 
                uint32_t cout = cin >> 1; 
                carry32 = (s >> 16) & 1u;
                __m256i vcin = _mm256_cmpeq_epi16(
                    _mm256_and_si256(_mm256_set1_epi16(static_cast<short>(cin)), vbit), vbit);
                __m256i vcout = _mm256_cmpeq_epi16(
                    _mm256_and_si256(_mm256_set1_epi16(static_cast<short>(cout)), vbit), vbit);
                
                __m256i r = _mm256_sub_epi16(
                    _mm256_add_epi16(t, _mm256_srli_epi16(vcin, 15)),
                    _mm256_and_si256(vcout, vbase));
                _mm256_storeu_si256(reinterpret_cast<__m256i *>(out.ptr + i), r);
            }
            Limb carry = static_cast<Limb>(carry32);
            
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
                
                __m256i t = _mm256_sub_epi16(_mm256_add_epi16(a, vbase), b);
                __m256i gm = _mm256_cmpgt_epi16(vbase, t); 
                __m256i pm = _mm256_cmpeq_epi16(t, vbase); 
                uint32_t G = _pext_u32(static_cast<uint32_t>(_mm256_movemask_epi8(gm)), 0x55555555u);
                uint32_t P = _pext_u32(static_cast<uint32_t>(_mm256_movemask_epi8(pm)), 0x55555555u);
                uint32_t A = G, B = G | P;
                uint32_t s = A + B + borrow;
                uint32_t cin = s ^ A ^ B; 
                uint32_t cout = cin >> 1; 
                borrow = (s >> 16) & 1u;
                __m256i vcin = _mm256_cmpeq_epi16(
                    _mm256_and_si256(_mm256_set1_epi16(static_cast<short>(cin)), vbit), vbit);
                __m256i vcout = _mm256_cmpeq_epi16(
                    _mm256_and_si256(_mm256_set1_epi16(static_cast<short>(cout)), vbit), vbit);
                
                __m256i r = _mm256_sub_epi16(
                    _mm256_sub_epi16(t, _mm256_srli_epi16(vcin, 15)),
                    _mm256_andnot_si256(vcout, vbase));
                _mm256_storeu_si256(reinterpret_cast<__m256i *>(out.ptr + i), r);
            }
            Limb bw = static_cast<Limb>(borrow);
            
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
                    
                    if (in1.ptr == out.ptr) return false;
                    
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
            if (buf.capacity() < buf_size)
                buf.reserve(buf_size);
            Limb *bp = buf.data();
            
            bp[in2.size] = absMul1(in2, in1[0], Span(bp, in2.size));
            
            for (size_t i = 1; i < in1.size; i++)
            {
                bp[i + in2.size] = absAddMul1(in2, in1[i], Span(bp + i, in2.size));
            }
            std::copy_n(bp, buf_size, out.begin());
        }
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        static uint64_t carryPropSeg(const double *v, Limb *out, size_t n)
        {
            constexpr size_t MIN_PAR = 2048;   
            uint64_t carry = 0;
            size_t i = 0;
            if (n >= MIN_PAR)
            {
                const size_t seg = (n >> 2) & ~size_t(7);
                const size_t b1 = seg, b2 = seg * 2, b3 = seg * 3;
                uint64_t c0 = 0, c1 = 0, c2 = 0, c3 = 0;
                for (size_t k = 0; k < seg; ++k)
                {
                    
                    
                    
                    uint64_t s0 = c0 + cvtRoundU64(v + k);
                    uint64_t s1 = c1 + cvtRoundU64(v + b1 + k);
                    uint64_t s2 = c2 + cvtRoundU64(v + b2 + k);
                    uint64_t s3 = c3 + cvtRoundU64(v + b3 + k);
                    uint64_t q0 = divBASE(s0);
                    uint64_t q1 = divBASE(s1);
                    uint64_t q2 = divBASE(s2);
                    uint64_t q3 = divBASE(s3);
                    out[k]      = Limb(s0 - q0 * BASE);
                    out[b1 + k] = Limb(s1 - q1 * BASE);
                    out[b2 + k] = Limb(s2 - q2 * BASE);
                    out[b3 + k] = Limb(s3 - q3 * BASE);
                    c0 = q0; c1 = q1; c2 = q2; c3 = q3;
                }
                
                carry = c3;
                for (i = b3 + seg; i < n; ++i)
                {
                    carry += cvtRoundU64(v + i);
                    uint64_t q = divBASE(carry);
                    out[i] = Limb(carry - q * BASE);
                    carry = q;
                }
                
                uint64_t ov = c0;
                for (size_t p = b1; ov > 0 && p < b2; ++p)
                {
                    uint64_t s = uint64_t(out[p]) + ov;
                    uint64_t q = divBASE(s);
                    out[p] = Limb(s - q * BASE);
                    ov = q;
                }
                ov += c1;
                for (size_t p = b2; ov > 0 && p < b3; ++p)
                {
                    uint64_t s = uint64_t(out[p]) + ov;
                    uint64_t q = divBASE(s);
                    out[p] = Limb(s - q * BASE);
                    ov = q;
                }
                ov += c2;
                for (size_t p = b3; ov > 0 && p < n; ++p)
                {
                    uint64_t s = uint64_t(out[p]) + ov;
                    uint64_t q = divBASE(s);
                    out[p] = Limb(s - q * BASE);
                    ov = q;
                }
                return carry + ov;
            }
            for (; i + 7 < n; i += 8)
            {
                HINT_PREFETCH(v + i + 16, 0, 0);
                HINT_PREFETCH(v + i + 24, 0, 0);
                uint64_t s0 = carry + cvtRoundU64(v + i);
                uint64_t q0 = divBASE(s0);
                uint64_t s1 = q0 + cvtRoundU64(v + i + 1);
                uint64_t q1 = divBASE(s1);
                uint64_t s2 = q1 + cvtRoundU64(v + i + 2);
                uint64_t q2 = divBASE(s2);
                uint64_t s3 = q2 + cvtRoundU64(v + i + 3);
                uint64_t q3 = divBASE(s3);
                uint64_t s4 = q3 + cvtRoundU64(v + i + 4);
                uint64_t q4 = divBASE(s4);
                uint64_t s5 = q4 + cvtRoundU64(v + i + 5);
                uint64_t q5 = divBASE(s5);
                uint64_t s6 = q5 + cvtRoundU64(v + i + 6);
                uint64_t q6 = divBASE(s6);
                uint64_t s7 = q6 + cvtRoundU64(v + i + 7);
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
            for (; i < n; ++i)
            {
                carry += cvtRoundU64(v + i);
                uint64_t q = divBASE(carry);
                out[i] = Limb(carry - q * BASE);
                carry = q;
            }
            return carry;
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
            if (tv1.capacity() < float_len) tv1.reserve(float_len);
            if (tv2.capacity() < float_len) tv2.reserve(float_len);
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
            uint64_t carry = carryPropSeg(v1, out.ptr, conv_len);
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
            if (tv.capacity() < float_len) tv.reserve(float_len);
            double *v = tv.data();
            copyU16ToF64AndFill(in.ptr, v, len, float_len);
            transform::fft::real_conv(v, v, float_len);
            uint64_t carry = carryPropSeg(v, out.ptr, conv_len);
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





#ifndef MUL_BASIC_THRESHOLD
#define MUL_BASIC_THRESHOLD FFT_MUL_THRESHOLD
#endif
#ifndef SQR_BASIC_THRESHOLD
#define SQR_BASIC_THRESHOLD FFT_SQR_THRESHOLD
#endif
        static void absSqr(View in, Span out)
        {
            assert(out.size >= in.size * 2);
            if (in.size <= SQR_BASIC_THRESHOLD)
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
            if (b_dft.capacity() < float_len) b_dft.reserve(float_len);
            prepareDFT(small, b_dft.data(), float_len);
            
            
            thread_local std::vector<Limb> large_copy;
            if (large_copy.capacity() < len_big) large_copy.reserve(len_big);
            std::copy_n(large.ptr, len_big, large_copy.data());
            
            std::fill_n(out.ptr, out.size, Limb(0));
            
            thread_local std::vector<Limb> tbuf;
            size_t tbuf_max = chunk + chunk;
            if (tbuf.capacity() < tbuf_max) tbuf.reserve(tbuf_max);
            
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
            if (sml <= MUL_BASIC_THRESHOLD)
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
            assert(is_2pow(float_len) || is_fft3(float_len) || is_fft5(float_len));
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
            if (tv.capacity() < float_len) tv.reserve(float_len);
            double *v = tv.data();
            copyU16ToF64AndFill(a.ptr, v, a_len, float_len);
            transform::fft::rdif(v, float_len);
            transform::fft::rdot(v, b_dft, float_len);
            transform::fft::ridit(v, float_len);
            uint64_t carry = carryPropSeg(v, out.ptr, conv_len);
            out[conv_len] = Limb(carry);

            if (out.size > conv_len + 1)
            {
                std::fill_n(out.ptr + conv_len + 1, out.size - conv_len - 1, Limb(0));
            }
        }

        
        
        
        
        
        
        static void fftMulModBm1(View a, View b, size_t m, Span out)
        {
            assert(is_2pow(m) || is_fft3(m) || is_fft5(m));
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
            if (tv.capacity() < 2 * m) tv.reserve(2 * m);
            double *va = tv.data();
            double *vb = tv.data() + m;

            copyU16ToF64AndFill(a.ptr, va, a_len, m);
            copyU16ToF64AndFill(b.ptr, vb, b_len, m);

            transform::fft::rdif(va, m);
            transform::fft::rdif(vb, m);
            transform::fft::rdot(va, vb, m);
            transform::fft::ridit(va, m);

            
            uint64_t carry = carryPropSeg(va, out.ptr, m);

            
            
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

        
        
        static void fftMulModBm1Pre(View a, const double *b_dft, size_t b_len, size_t m, Span out)
        {
            assert(is_2pow(m) || is_fft3(m) || is_fft5(m));
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
            if (tv.capacity() < m) tv.reserve(m);
            double *v = tv.data();

            copyU16ToF64AndFill(a.ptr, v, a_len, m);

            transform::fft::rdif(v, m);
            transform::fft::rdot(v, b_dft, m);
            transform::fft::ridit(v, m);

            
            uint64_t carry = carryPropSeg(v, out.ptr, m);
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
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        static Limb absMul1(View in, Limb x, Span out)
        {
            static_assert(sizeof(Limb) == 2 && BASE == 10000,
                          "absMul1 AVX2 path assumes uint16 limbs in base 1e4");
            size_t i = 0;
            uint32_t carry_q = 0;   
            uint32_t carry_rip = 0; 
            if (in.size >= 16)
            {
                const __m256i vx = _mm256_set1_epi32(int(uint32_t(x)));
                const __m256i vmagic = _mm256_set1_epi32(109951163); 
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
                    
                    __m256i p0 = _mm256_mullo_epi32(a0, vx);
                    __m256i p1 = _mm256_mullo_epi32(a1, vx);
                    
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
                    
                    __m256i q16 = _mm256_permute4x64_epi64(
                        _mm256_packus_epi32(q0, q1), 0xD8);
                    __m256i r16 = _mm256_permute4x64_epi64(
                        _mm256_packus_epi32(r0, r1), 0xD8);
                    
                    __m256i qsh = _mm256_alignr_epi8(
                        q16, _mm256_permute2x128_si256(q16, q16, 0x08), 14);
                    qsh = _mm256_insert_epi16(qsh, int(carry_q), 0);
                    carry_q = uint32_t(uint16_t(_mm256_extract_epi16(q16, 15)));
                    __m256i t = _mm256_add_epi16(r16, qsh); 
                    __m256i gm = _mm256_cmpgt_epi16(t, vbm1); 
                    __m256i pm = _mm256_cmpeq_epi16(t, vbm1); 
                    uint32_t G = _pext_u32(uint32_t(_mm256_movemask_epi8(gm)), 0x55555555u);
                    uint32_t P = _pext_u32(uint32_t(_mm256_movemask_epi8(pm)), 0x55555555u);
                    uint32_t A = G, B = G | P;
                    uint32_t sw = A + B + carry_rip;
                    uint32_t cin = sw ^ A ^ B; 
                    uint32_t cout = cin >> 1;  
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
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        static Limb absAddMul1(View in, Limb x, Span acc)
        {
            static_assert(sizeof(Limb) == 2 && BASE == 10000,
                          "absAddMul1 AVX2 path assumes uint16 limbs in base 1e4");
            size_t i = 0;
            uint32_t carry = 0; 
            if (in.size >= 16)
            {
                const __m256i vx = _mm256_set1_epi32(int(uint32_t(x)));
                const __m256i vmagic = _mm256_set1_epi32(109951163); 
                const __m256i vbase32 = _mm256_set1_epi32(int(BASE));
                const __m256i vbase16 = _mm256_set1_epi16(short(BASE));
                const __m256i vbm1 = _mm256_set1_epi16(short(BASE - 1));
                const __m256i v2bm1 = _mm256_set1_epi16(short(2 * BASE - 1));
                const __m256i vzero = _mm256_setzero_si256();
                for (; i + 15 < in.size; i += 16)
                {
                    __m256i a16 = _mm256_loadu_si256(
                        reinterpret_cast<const __m256i *>(in.ptr + i));
                    __m256i a0 = _mm256_cvtepu16_epi32(_mm256_castsi256_si128(a16));
                    __m256i a1 = _mm256_cvtepu16_epi32(_mm256_extracti128_si256(a16, 1));
                    __m256i p0 = _mm256_mullo_epi32(a0, vx); 
                    __m256i p1 = _mm256_mullo_epi32(a1, vx);
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
                    __m256i q16 = _mm256_permute4x64_epi64(
                        _mm256_packus_epi32(q0, q1), 0xD8);
                    __m256i r16 = _mm256_permute4x64_epi64(
                        _mm256_packus_epi32(r0, r1), 0xD8);
                    
                    
                    __m256i qsh = _mm256_alignr_epi8(
                        q16, _mm256_permute2x128_si256(q16, q16, 0x08), 14);
                    qsh = _mm256_or_si256(
                        qsh, _mm256_castsi128_si256(_mm_cvtsi32_si128(int(carry))));
                    uint32_t q_last = uint32_t(uint16_t(_mm256_extract_epi16(q16, 15)));
                    __m256i b16 = _mm256_loadu_si256(
                        reinterpret_cast<const __m256i *>(acc.ptr + i));
                    __m256i t = _mm256_add_epi16(_mm256_add_epi16(r16, qsh), b16);
                    __m256i ge1 = _mm256_cmpgt_epi16(t, vbm1);  
                    __m256i ge2 = _mm256_cmpgt_epi16(t, v2bm1); 
                    
                    __m256i g = _mm256_sub_epi16(_mm256_sub_epi16(vzero, ge1), ge2);
                    __m256i m = _mm256_sub_epi16(t, _mm256_mullo_epi16(g, vbase16));
                    
                    __m256i gsh = _mm256_alignr_epi8(
                        g, _mm256_permute2x128_si256(g, g, 0x08), 14);
                    __m256i u = _mm256_add_epi16(m, gsh); 
                    __m256i chk = _mm256_cmpgt_epi16(u, vbm1);
                    if (_mm256_testz_si256(chk, chk))
                    {
                        uint32_t g_last =
                            uint32_t(uint16_t(_mm256_extract_epi16(g, 15)));
                        _mm256_storeu_si256(
                            reinterpret_cast<__m256i *>(acc.ptr + i), u);
                        carry = q_last + g_last; 
                    }
                    else
                    {
                        
                        uint32_t c = carry;
                        for (size_t j = i; j < i + 16; j++)
                        {
                            uint32_t v = uint32_t(in[j]) * uint32_t(x)
                                       + uint32_t(acc[j]) + c;
                            acc[j] = Limb(v % BASE);
                            c = v / BASE;
                        }
                        carry = c;
                    }
                }
            }
            for (; i < in.size; i++)
            {
                uint32_t v = uint32_t(in[i]) * uint32_t(x) + uint32_t(acc[i]) + carry;
                acc[i] = Limb(v % BASE);
                carry = v / BASE;
            }
            return Limb(carry);
        }
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        template <bool PADDED>
        static bool absSubMul1(Span acc, View in, Limb x)
        {
            static_assert(sizeof(Limb) == 2 && BASE == 10000,
                          "absSubMul1 AVX2 path assumes uint16 limbs in base 1e4");
            assert(acc.size >= in.size + 1);
            const size_t n = in.size;
            size_t i = 0;
            uint32_t carry_q = 0;   
            uint32_t carry_rip = 0; 
            uint32_t borrow = 0;    
            if (n >= 16)
            {
                const __m256i vx = _mm256_set1_epi32(int(uint32_t(x)));
                const __m256i vmagic = _mm256_set1_epi32(109951163); 
                const __m256i vbase32 = _mm256_set1_epi32(int(BASE));
                const __m256i vbase16 = _mm256_set1_epi16(short(BASE));
                const __m256i vbm1 = _mm256_set1_epi16(short(BASE - 1));
                const __m256i vbit = _mm256_setr_epi16(
                    1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192,
                    short(16384), short(32768));
                for (; i + 15 < n; i += 16)
                {
                    
                    __m256i a16 = _mm256_loadu_si256(
                        reinterpret_cast<const __m256i *>(in.ptr + i));
                    __m256i a0 = _mm256_cvtepu16_epi32(_mm256_castsi256_si128(a16));
                    __m256i a1 = _mm256_cvtepu16_epi32(_mm256_extracti128_si256(a16, 1));
                    __m256i p0 = _mm256_mullo_epi32(a0, vx);
                    __m256i p1 = _mm256_mullo_epi32(a1, vx);
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
                    __m256i q16 = _mm256_permute4x64_epi64(
                        _mm256_packus_epi32(q0, q1), 0xD8);
                    __m256i r16 = _mm256_permute4x64_epi64(
                        _mm256_packus_epi32(r0, r1), 0xD8);
                    __m256i qsh = _mm256_alignr_epi8(
                        q16, _mm256_permute2x128_si256(q16, q16, 0x08), 14);
                    qsh = _mm256_insert_epi16(qsh, int(carry_q), 0);
                    carry_q = uint32_t(uint16_t(_mm256_extract_epi16(q16, 15)));
                    __m256i t = _mm256_add_epi16(r16, qsh); 
                    __m256i gm = _mm256_cmpgt_epi16(t, vbm1);
                    __m256i pm = _mm256_cmpeq_epi16(t, vbm1);
                    uint32_t G = _pext_u32(uint32_t(_mm256_movemask_epi8(gm)), 0x55555555u);
                    uint32_t P = _pext_u32(uint32_t(_mm256_movemask_epi8(pm)), 0x55555555u);
                    uint32_t A = G, B = G | P;
                    uint32_t sw = A + B + carry_rip;
                    uint32_t cin = sw ^ A ^ B;
                    uint32_t cout = cin >> 1;
                    carry_rip = (sw >> 16) & 1u;
                    __m256i vcin = _mm256_cmpeq_epi16(
                        _mm256_and_si256(_mm256_set1_epi16(short(cin)), vbit), vbit);
                    __m256i vcout = _mm256_cmpeq_epi16(
                        _mm256_and_si256(_mm256_set1_epi16(short(cout)), vbit), vbit);
                    __m256i res = _mm256_sub_epi16(
                        _mm256_add_epi16(t, _mm256_srli_epi16(vcin, 15)),
                        _mm256_and_si256(vcout, vbase16));
                    
                    __m256i av = _mm256_loadu_si256(
                        reinterpret_cast<const __m256i *>(acc.ptr + i));
                    __m256i t2 = _mm256_sub_epi16(_mm256_add_epi16(av, vbase16), res);
                    __m256i gm2 = _mm256_cmpgt_epi16(vbase16, t2); 
                    __m256i pm2 = _mm256_cmpeq_epi16(t2, vbase16); 
                    uint32_t G2 = _pext_u32(uint32_t(_mm256_movemask_epi8(gm2)), 0x55555555u);
                    uint32_t P2 = _pext_u32(uint32_t(_mm256_movemask_epi8(pm2)), 0x55555555u);
                    uint32_t A2 = G2, B2 = G2 | P2;
                    uint32_t s2 = A2 + B2 + borrow;
                    uint32_t cin2 = s2 ^ A2 ^ B2;
                    uint32_t cout2 = cin2 >> 1;
                    borrow = (s2 >> 16) & 1u;
                    __m256i vcin2 = _mm256_cmpeq_epi16(
                        _mm256_and_si256(_mm256_set1_epi16(short(cin2)), vbit), vbit);
                    __m256i vcout2 = _mm256_cmpeq_epi16(
                        _mm256_and_si256(_mm256_set1_epi16(short(cout2)), vbit), vbit);
                    __m256i r2 = _mm256_sub_epi16(
                        _mm256_sub_epi16(t2, _mm256_srli_epi16(vcin2, 15)),
                        _mm256_andnot_si256(vcout2, vbase16));
                    _mm256_storeu_si256(reinterpret_cast<__m256i *>(acc.ptr + i), r2);
                }
            }
            
            if constexpr (PADDED)
            {
                
                if (n >= 8)
                {
                    assert(acc.size == n + 1); 
                    const size_t rem = n - i; 
                    const __m256i vx = _mm256_set1_epi32(int(uint32_t(x)));
                    const __m256i vmagic = _mm256_set1_epi32(109951163);
                    const __m256i vbase32 = _mm256_set1_epi32(int(BASE));
                    const __m256i vbase16 = _mm256_set1_epi16(short(BASE));
                    const __m256i vbm1 = _mm256_set1_epi16(short(BASE - 1));
                    const __m256i vbit = _mm256_setr_epi16(
                        1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192,
                        short(16384), short(32768));
                    const __m256i vidx = _mm256_setr_epi16(
                        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
                    
                    const __m256i in_mask =
                        _mm256_cmpgt_epi16(_mm256_set1_epi16(short(rem)), vidx);
                    const __m256i keep =
                        _mm256_cmpgt_epi16(_mm256_set1_epi16(short(rem + 1)), vidx);

                    __m256i a16 = _mm256_and_si256(
                        _mm256_loadu_si256(reinterpret_cast<const __m256i *>(in.ptr + i)),
                        in_mask);
                    __m256i a0 = _mm256_cvtepu16_epi32(_mm256_castsi256_si128(a16));
                    __m256i a1 = _mm256_cvtepu16_epi32(_mm256_extracti128_si256(a16, 1));
                    __m256i p0 = _mm256_mullo_epi32(a0, vx);
                    __m256i p1 = _mm256_mullo_epi32(a1, vx);
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
                    __m256i q16 = _mm256_permute4x64_epi64(
                        _mm256_packus_epi32(q0, q1), 0xD8);
                    __m256i r16 = _mm256_permute4x64_epi64(
                        _mm256_packus_epi32(r0, r1), 0xD8);
                    __m256i qsh = _mm256_alignr_epi8(
                        q16, _mm256_permute2x128_si256(q16, q16, 0x08), 14);
                    qsh = _mm256_insert_epi16(qsh, int(carry_q), 0);
                    __m256i t = _mm256_add_epi16(r16, qsh);
                    __m256i gm = _mm256_cmpgt_epi16(t, vbm1);
                    __m256i pm = _mm256_cmpeq_epi16(t, vbm1);
                    uint32_t G = _pext_u32(uint32_t(_mm256_movemask_epi8(gm)), 0x55555555u);
                    uint32_t P = _pext_u32(uint32_t(_mm256_movemask_epi8(pm)), 0x55555555u);
                    uint32_t A = G, B = G | P;
                    uint32_t sw = A + B + carry_rip;
                    uint32_t cin = sw ^ A ^ B;
                    uint32_t cout = cin >> 1;
                    __m256i vcin = _mm256_cmpeq_epi16(
                        _mm256_and_si256(_mm256_set1_epi16(short(cin)), vbit), vbit);
                    __m256i vcout = _mm256_cmpeq_epi16(
                        _mm256_and_si256(_mm256_set1_epi16(short(cout)), vbit), vbit);
                    __m256i res = _mm256_sub_epi16(
                        _mm256_add_epi16(t, _mm256_srli_epi16(vcin, 15)),
                        _mm256_and_si256(vcout, vbase16));
                    
                    __m256i av = _mm256_loadu_si256(
                        reinterpret_cast<const __m256i *>(acc.ptr + i));
                    __m256i t2 = _mm256_sub_epi16(_mm256_add_epi16(av, vbase16), res);
                    __m256i gm2 = _mm256_cmpgt_epi16(vbase16, t2);
                    __m256i pm2 = _mm256_cmpeq_epi16(t2, vbase16);
                    uint32_t G2 = _pext_u32(uint32_t(_mm256_movemask_epi8(gm2)), 0x55555555u);
                    uint32_t P2 = _pext_u32(uint32_t(_mm256_movemask_epi8(pm2)), 0x55555555u);
                    uint32_t A2 = G2, B2 = G2 | P2;
                    uint32_t s2 = A2 + B2 + borrow;
                    uint32_t cin2 = s2 ^ A2 ^ B2;
                    uint32_t cout2 = cin2 >> 1;
                    
                    
                    borrow = (cin2 >> (rem + 1)) & 1u;
                    __m256i vcin2 = _mm256_cmpeq_epi16(
                        _mm256_and_si256(_mm256_set1_epi16(short(cin2)), vbit), vbit);
                    __m256i vcout2 = _mm256_cmpeq_epi16(
                        _mm256_and_si256(_mm256_set1_epi16(short(cout2)), vbit), vbit);
                    __m256i r2 = _mm256_sub_epi16(
                        _mm256_sub_epi16(t2, _mm256_srli_epi16(vcin2, 15)),
                        _mm256_andnot_si256(vcout2, vbase16));
                    _mm256_storeu_si256(reinterpret_cast<__m256i *>(acc.ptr + i),
                                        _mm256_blendv_epi8(av, r2, keep));
                    return borrow != 0;
                }
            }
            Limb mcarry = Limb(carry_q + carry_rip);
            Limb bw = Limb(borrow);
            for (; i < n; i++)
            {
                Limb2 prod = Limb2(in[i]) * x + mcarry;
                Limb m = Limb(prod % BASE);
                mcarry = Limb(prod / BASE);
                acc[i] = sub_half<Limb>(acc[i], Limb(m + bw), BASE, bw);
            }
            
            acc[i] = sub_half<Limb>(acc[i], Limb(mcarry + bw), BASE, bw);
            i++;
            
            for (; i < acc.size && bw; i++)
            {
                acc[i] = sub_half<Limb>(acc[i], bw, BASE, bw);
            }
            return bw != 0;
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
        
        
        template <bool PADDED = false>
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

            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            const uint64_t dh_magic = (uint64_t(1) << 42) / divisor_high + 1;
            const Limb divisor_high2 = (len2 >= 2) ? divisor[len2 - 2] : Limb(0);

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
                Span dividend_span(dividend + quot_idx);
                assert(dividend_span.size == len2 + 1);
                
                
                bool bf = absSubMul1<PADDED>(dividend_span, divisor, qhat);
                while (bf)
                {
                    qhat--;
                    
                    bf = !absAdd(dividend_span, divisor, dividend_span);
                }
                quotient[quot_idx] = qhat;
                dividend.size = len1;
            }
        }

        
        
        
        
        
        
        
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
            if (tprod.capacity() < prod_size) tprod.reserve(prod_size);
            if (tinv2.capacity() < inv2_size) tinv2.reserve(inv2_size);
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

        
        
        
        
        static void absInvNewtonGMP(View m, Span inv,
                                    const double *m_dft = nullptr, size_t m_dft_float_len = 0)
        {
            size_t k = m.size;  
            INVPROF_SCOPE(k);
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

#ifndef CYCLIC_MIN_K
#define CYCLIC_MIN_K 4096
#endif
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            size_t s = (k - 1) / 2;
#ifndef DISABLE_ADAPTIVE_SPLIT
            if (k >= CYCLIC_MIN_K)
            {
                
                const size_t p2_adj = fft_ceil_mn_q(k + 1);
                const size_t rn_min = k - s;                            
                const size_t rn_max = (k * 13) / 20;                    
                const size_t rn_cyc = (p2_adj > k) ? (p2_adj - k) : 0;  

                if (rn_cyc > rn_min)  
                {
                    if (rn_cyc <= rn_max)
                    {
                        
                        s = k - rn_cyc;
                    }
                    else
                    {
                        
                        const size_t c1 = fft_ceil_mn_q(2 * (rn_min + 1));           
                        const size_t c2 = fft_ceil_mn_q(2 * (rn_min + 1) + k - 1);   
                        const size_t lim1 = c1 / 2 - 1;                          
                        const size_t lim2 = (c2 - k - 1) / 2;                    
                        const size_t rn_free = std::min(std::min(lim1, lim2), rn_max);
                        if (rn_free > rn_min)
                        {
                            
                            
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
            
            absInvNewtonGMP(m + s, inv);
            size_t rn = k - s;        
            size_t inv0_len = rn + 1;
            Span inv0(inv.ptr, inv0_len);

            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            size_t mn = fft_ceil_mn(k + 1);
            
            
            
            
            
            
            
            
            
            
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
                
                
                
                

                
                
                thread_local std::vector<Limb> txp_mod;
                if (txp_mod.capacity() < mn + 2) txp_mod.reserve(mn + 2);
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

                
                
                
                
                
                
                
                {
                    size_t sub_pos = rn + k - mn;
                    assert(sub_pos < mn);
                    Limb borrow = 1;
                    size_t j = sub_pos;
                    size_t guard = 0; 
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
                    
                    
                    if (borrow > 0)
                    {
                        
                        
                        std::fill_n(xp_mod.ptr, mn, Limb(BASE - 1));
                        xp_mod[0] = Limb(BASE - 2);
                    }
                }

                
                
                
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
                    
                    
                    
                    is_positive = false;
                    goto gmp_newton_fallback;
                }

                
                
                
                
                
                {
                    
                    
                    std::copy_backward(inv.ptr, inv.ptr + rn + 1, inv.ptr + k + 1);

                    
                    thread_local std::vector<Limb> txp_full;
                    if (txp_full.capacity() < 2 * k + 2) txp_full.reserve(2 * k + 2);
                    std::fill_n(txp_full.data(), 2 * k + 2, Limb(0));

                    
                    std::copy(xp_mod.ptr, xp_mod.ptr + mn, txp_full.data());

                    Limb *xp = txp_full.data();
                    Limb cy = 0;  
                    assert(2 * k >= 3 * rn && "xp_high and mul_out must not overlap");

                  if (is_positive)
                  {
                    
                    
                    cy = xp[k];  
                    if (cy > 0)
                    {
                        
                        bool borrow = absSub(View(xp, k), m, Span(xp, k));
                        cy = 2;  
                        if (!borrow)
                        {
                            
                            borrow = absSub(View(xp, k), m, Span(xp, k));
                            assert(borrow && "ASSERT_CARRY: second sub must borrow");
                            cy = 3;
                        }
                    }
                    else
                    {
                        cy = 1;  
                    }

                    
                    if (absCompare(View(xp, k), m) > 0)
                    {
                        bool borrow = absSub(View(xp, k), m, Span(xp, k));
                        assert(!borrow && "ASSERT_NOCARRY");
                        cy++;
                    }

                    
                    
                    
                    
                    assert(2 * k >= 3 * rn && "xp_high and mul_out must not overlap");
                    Limb *xp_high = xp + 2 * k - rn;
                    Limb bf = (absCompare(View(xp, s), View(m.ptr, s)) > 0) ? 1 : 0;
                    for (size_t i = 0; i < rn; i++)
                    {
                        xp_high[i] = sub_half<Limb>(m.ptr[s + i], xp[s + i] + bf, BASE, bf);
                    }
                    assert(bf == 0 && "ASSERT_NOCARRY: xp_high sub overflow");

                    
                    
                    {
                        bool bf2 = absSub1(View(inv.ptr + s, rn), cy, Span(inv.ptr + s, rn));
                        assert(!bf2 && "MPN_DECR_U underflow");
                    }
                  }
                  else
                  {
                    
                    
                    
                    
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

                    
                    
                    INVPH_END(corr, k, "corr");
                    {
                        INVPH_DECL(muln);
                        View xp_high_view(xp + 2 * k - rn, rn);
                        View inv0_low_view(inv.ptr + s, rn);
                        Span mul_out(xp, 2 * rn);
                        absMul(xp_high_view, inv0_low_view, mul_out);
                        INVPH_END(muln, k, "mul_n");
                    }

                    
                    
                    size_t add_len = 2 * rn - k;
                    Limb carry = 0;
                    if (add_len > 0)
                    {
                        carry = absAdd(View(xp + rn, add_len),
                                       View(xp + 2 * k - rn, add_len),
                                       Span(xp + rn, add_len)) ? 1 : 0;
                    }

                    
                    
                    {
                        Limb *xph = xp + 2 * k - rn;
                        for (size_t i = 0; i < s; i++)
                        {
                            Limb2 sum = Limb2(xp[3 * rn - k + i]) + Limb2(xph[2 * rn - k + i]) + carry;
                            inv.ptr[i] = Limb(sum % BASE);
                            carry = Limb(sum / BASE);
                        }
                    }

                    
                    
                    if (carry > 0)
                    {
                        bool cf2 = absAdd1(View(inv.ptr + s, rn), carry, Span(inv.ptr + s, rn));
                        assert(!cf2 && "MPN_INCR_U overflow into integer bit");
                    }

                    
                    
                    
                    
                    
                    
                    
                    
                    
                    
                    
                    
                    
                    
                    
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
                            if (tinv_p1.capacity() < k + 2) tinv_p1.reserve(k + 2);
                            if (tchk.capacity() < 2 * k + 3) tchk.reserve(2 * k + 3);
                            std::copy(inv.ptr, inv.ptr + k + 1, tinv_p1.data());
                            bool cf_p1 = absAdd1(View(tinv_p1.data(), k + 1), 1,
                                                 Span(tinv_p1.data(), k + 1));
                            assert(!cf_p1 && "inv+1 overflow");
                            (void)cf_p1;
                            std::fill_n(tchk.data(), 2 * k + 2, Limb(0));
                            absMul(View(tinv_p1.data(), k + 1), m, Span(tchk.data(), 2 * k + 1));
                            
                            
                            
                            
                            
                            
                            
                            
                            bool need_inc = false;
                            if (tchk[2 * k] == 0)
                            {
                                need_inc = true;  
                            }
                            else if (tchk[2 * k] == 1)
                            {
                                
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
                
            }

        gmp_newton_fallback:
#ifdef INVPROF
            if (_ips.key.cyc == 1) _ips.key.cyc = 2;  
#endif
            
            
            
            
            thread_local std::vector<Limb> tprod, tinv2;
            size_t prod_size = inv0_len * 2 + k;
            size_t inv2_size = k + 1;
            if (tprod.capacity() < prod_size) tprod.reserve(prod_size);
            if (tinv2.capacity() < inv2_size) tinv2.reserve(inv2_size);
            std::fill_n(tinv2.data(), s, Limb(0));
            Span prod_span(tprod.data(), prod_size), inv2_span(tinv2.data(), inv2_size);
            bool cf = absAdd(inv0, inv0, inv2_span + s);
            assert(!cf);
            absSqr(inv0, prod_span);
            
            
            
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
            if (tqhat.capacity() < qhat_len) tqhat.reserve(qhat_len);
            if (tprod.capacity() < prod_len) tprod.reserve(prod_len);
            
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
            if (tqhat.capacity() < qhat_len) tqhat.reserve(qhat_len);
            if (tprod.capacity() < prod_len) tprod.reserve(prod_len);
            Span qhat_span(tqhat.data(), qhat_len), prod_span(tprod.data(), prod_len);
            fftMulPre(divid_high, inv_dft, inv_span.size, inv_float_len, qhat_span);
            qhat_span = qhat_span + (k + 1);
            fftMulPre(qhat_span, divisor_dft, divisor.size, divisor_float_len, prod_span);
            
            while (absCompare(prod_span, dividend) > 0)
            {
                absSub(prod_span, divisor, prod_span);
                absSub1(qhat_span, 1, qhat_span);
            }
            
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
             if (tqhat.capacity() < qhat_len) tqhat.reserve(qhat_len);
             if (tprod.capacity() < prod_len) tprod.reserve(prod_len);

             Span qhat_span(tqhat.data(), qhat_len), prod_span(tprod.data(), prod_len);
             absMul(inv_span, divid_high, qhat_span);
             qhat_span = qhat_span + (k + 1);
             absMul(divisor, qhat_span, prod_span);
             prod_span.size = count_true_length(prod_span.ptr, prod_span.size);

             
             int corrections = 0;
             while (absCompare(prod_span, dividend) > 0 && corrections < 5)
             {
                 absSub(prod_span, divisor, prod_span);
                 absSub1(qhat_span, 1, qhat_span);
                 corrections++;
             }
             absSub(dividend, prod_span, dividend);
             dividend.size = k;
             
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

                
                
                if (divisor_high.size >= FFT_MUL_THRESHOLD)
                {
                    thread_local AlignedVec32<double> inv_dft_buf_c1, div_dft_buf_c1;
                    size_t inv_fl = fft_ceil_lin(2 * divisor_high.size + 1);
                    size_t div_fl = fft_ceil_lin(2 * divisor_high.size);
                    if (inv_dft_buf_c1.capacity() < inv_fl) inv_dft_buf_c1.reserve(inv_fl);
                    if (div_dft_buf_c1.capacity() < div_fl) div_dft_buf_c1.reserve(div_fl);
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
            if (t_prod.capacity() < prod_size) t_prod.reserve(prod_size);
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

            
            
            
            
            
            
            
            thread_local std::vector<Limb> t_inv;
            const size_t inv_extra = (in + 2 <= len2) ? 2 : 0;
            size_t inv_size = in + 1 + inv_extra;
            if (t_inv.size() < inv_size) t_inv.resize(inv_size);
            Span inv_span_full(t_inv.data(), inv_size);      
            Span inv_span(t_inv.data() + inv_extra, in + 1); 

            
            
            
            thread_local AlignedVec32<double> inv_dft_buf, divisor_dft_buf;
            size_t inv_float_len = fft_ceil_lin(2 * in + 1);
            
            
            
            size_t divisor_float_len = fft_ceil_lin(len2 + in);
            if (inv_dft_buf.capacity() < inv_float_len) inv_dft_buf.reserve(inv_float_len);
            if (divisor_dft_buf.capacity() < divisor_float_len) divisor_dft_buf.reserve(divisor_float_len);
            
            
            
            bool use_cyclic = false;
#ifndef DISABLE_2NXN_CYCLIC
            
            
            
            
            thread_local AlignedVec32<double> divisor_dft_mod_buf;
            
            
            
            
            
            
            
            size_t cyclic_m = std::max(fft_ceil_cycm(len2 + 1), fft_ceil_cycm((len2 + in) / 2 + 1));
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
#ifdef GATE_POW2
            const size_t cyclic_m_gate = std::max(int_ceil2(len2 + 1),
                                                  int_ceil2((len2 + in) / 2 + 1));
#else
            const size_t cyclic_m_gate = cyclic_m;
#endif
            use_cyclic = (cyclic_m_gate < in + len2) && allow_cyclic;
#ifdef DIV_MUSHAPE
            {
                
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
            if (use_cyclic && divisor_dft_mod_buf.capacity() < cyclic_m) divisor_dft_mod_buf.reserve(cyclic_m);
#endif

            
            if (in == len2)
            {
#if !defined(DISABLE_2NXN_CYCLIC) && !defined(DISABLE_INLEN2_GMP)
                
                
                
                
                
                
                
                
                
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
                
                
                if (inv_extra > 0) {
                    
                    
                    
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

            
            
            
            thread_local std::vector<Limb> tqhat, tprod;
            
            size_t qhat_len_max = 2 * in + 2;
            size_t prod_len_max = len2 + in + 1;
#ifndef DISABLE_2NXN_CYCLIC
            
            prod_len_max = std::max(prod_len_max, cyclic_m);
#endif
            if (tqhat.capacity() < qhat_len_max) tqhat.reserve(qhat_len_max);
            if (tprod.capacity() < prod_len_max) tprod.reserve(prod_len_max);
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

                
                
                Span window(dividend.ptr + (qn_remaining - this_in), len2 + this_in);
                Span quot_block(quotient.ptr + (qn_remaining - this_in), this_in);

                
                Span divid_high = window + (len2 - 1);

                
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

                
                
                Span qhat_span = qhat_full + (in + 1);

                
                
                
                
                
                
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

                
                size_t prod_len = len2 + qhat_span.size;
                Span prod_span(tprod.data(), prod_len);
#ifdef PROFILE_DIV
                auto _blk_t2 = std::chrono::high_resolution_clock::now();
#endif
#ifndef DISABLE_2NXN_CYCLIC
                
                
                
                
                if (use_cyclic)
                {
                    Span prod_mod_span(tprod.data(), cyclic_m);
                    fftMulModBm1Pre(qhat_span, divisor_dft_mod_buf.data(), len2, cyclic_m, prod_mod_span);
                    
                    
                    if (len2 + this_in > cyclic_m)
                    {
                        size_t wn = len2 + this_in - cyclic_m;
                        
                        
                        
                        
                        Span prod_low_wn(prod_mod_span.ptr, wn);
                        View rp_high_wn(window.ptr + (len2 + this_in - wn), wn);
                        bool borrow = absSub(prod_low_wn, rp_high_wn, prod_low_wn);
                        
                        
                        
                        Span prod_rest(prod_mod_span.ptr + wn, cyclic_m - wn);
                        if (borrow) borrow = absSub1(prod_rest, 1, prod_rest);
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
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
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
#ifndef UNWRAP_MODE
#define UNWRAP_MODE 0
#endif
#ifdef UNWRAP_PROBE
                        {
                            
                            
                            int _true = 99;  
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
                            
                            size_t _cmp_len = cyclic_m - len2;
                            bool _cx = (absCompare(View(window.ptr + len2, _cmp_len),
                                                   View(tprod.data() + len2, _cmp_len)) < 0);
                            int _gmp_incr = int(_cx) - int(borrow);
                            
                            if (_err != 0 || _true != 0 || _gmp_incr != 0)
                                std::fprintf(stderr,
                                             "[unwrap] len2=%zu this_in=%zu m=%zu wn=%zu "
                                             "gmp_incr=%d err=%d TRUE=%d\n",
                                             len2, this_in, cyclic_m, wn, _gmp_incr,
                                             int(_err), _true);
                        }
#endif
                        
                        if (_err > 0)
                            absSub1(prod_mod_span, Limb(_err), prod_mod_span);
                        else if (_err < 0)
                            absAdd1(prod_mod_span, Limb(-_err), prod_mod_span);
                    }
                    
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
                
                
                
                
                
                
                
                int32_t r = int32_t(window[len2]) - int32_t(tprod[len2]);

                
                
                
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

                
                r -= int32_t(cy);

                
                
                
                int corr_cnt = 0;
                while (r != 0 && corr_cnt < 10) {
                    if (r > 0) {
                        
                        absAdd1(qhat_span, 1, qhat_span);
                        bool b = absSub(Span(tprod.data(), len2), divisor, Span(tprod.data(), len2));
                        r -= int32_t(b);  
                    } else {
                        
                        absSub1(qhat_span, 1, qhat_span);
                        bool carry = absAdd(Span(tprod.data(), len2), divisor, Span(tprod.data(), len2));
                        r += int32_t(carry);  
                    }
                    corr_cnt++;
                }

                
                if (absCompare(Span(tprod.data(), len2), divisor) >= 0) {
                    absSub(Span(tprod.data(), len2), divisor, Span(tprod.data(), len2));
                    absAdd1(qhat_span, 1, qhat_span);
                }
                }
                else
#endif
                {
                
                int corr_down = 0;
                while (corr_down < 10) {
                    
                    bool prod_gt = false;
                    
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
                
                {
                    Span tp_extra(tprod.data() + len2, this_in);
                    View wnd_extra(window.ptr + len2, this_in);
                    bool cy_high = absSub(wnd_extra, tp_extra, tp_extra);
                    if (cy) {
                        bool cy3 = absSub1(tp_extra, 1, tp_extra);
                        cy_high = cy_high || cy3;
                    }
                    
                }

                
                
                
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

                
                std::copy(tprod.data(), tprod.data() + len2, window.ptr);
                window.size = len2;
#ifdef PROFILE_DIV
                auto _blk_t5 = std::chrono::high_resolution_clock::now();
                PROF_PRINT("  [prof]   block %zu: correct+sub: %.3f ms\n",
                        (qn - qn_remaining) / in,
                        std::chrono::duration<double, std::milli>(_blk_t5 - _blk_t4).count());
#endif

                
                
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

             
             thread_local AlignedVec32<double> inv_dft_buf, divisor_dft_buf;
             size_t inv_float_len = fft_ceil_lin(len2 * 2 + 1);
             size_t divisor_float_len = fft_ceil_lin(len2 * 2);
             bool has_divisor_dft = false;
              
              if (blocks >= 2)  
              {
                  if (divisor_dft_buf.capacity() < divisor_float_len) divisor_dft_buf.reserve(divisor_float_len);
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

            
            if (blocks >= 2)  
            {
                if (inv_dft_buf.capacity() < inv_float_len) inv_dft_buf.reserve(inv_float_len);
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
                    
                    
                    
                    
                    
                    
                    if (len2 >= 16)
                    {
                        dividend_norm.data.resize(len1 + 16, 0);
                        divisor_norm.data.resize(len2 + 16, 0);
                        
                        dividend_span = Span(dividend_norm.data.data(), len1);
                        divisor_span = Span(divisor_norm.data.data(), len2);
                        absDivBasicCore<true>(dividend_span, divisor_span, quot_span);
                        dividend_norm.data.resize(len1);
                    }
                    else
                    {
                        absDivBasicCore<false>(dividend_span, divisor_span, quot_span);
                    }
                }
                else if (len1 < len2 * 2)
                {
                    absDivNewtonCore1(dividend_span, divisor_span, quot_span);
                }
                else
                {
                    
                    size_t qn_mu = len1 - len2;
                    size_t mu_in;
                    if (qn_mu > len2)
                    {
                        
                        size_t nb = (qn_mu - 1) / len2 + 1;
#ifdef DIV_MU_NB_DELTA
                        
                        nb += (size_t)(DIV_MU_NB_DELTA);
#else
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
#ifndef DIV_MU_NB_MODEL_OFF
                        {
                            const size_t in_nat = (qn_mu - 1) / nb + 1;
                            if (in_nat >= 16384)
                            {
                                
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
                        
                        
                        
                        
                        
                        size_t in2 = (qn_mu - 1) / 2 + 1;
                        size_t in4 = (qn_mu - 1) / 4 + 1;
                        if (in2 > len2) in2 = len2;
                        if (in4 > len2) in4 = len2;
#ifdef DIV_MU_IN_QUARTER
                        mu_in = in4;  
#elif defined(DIV_MU_IN_HALF)
                        mu_in = in2;  
#else
                        size_t div_fl2 = fft_ceil_lin_q(len2 + in2);
                        size_t div_fl4 = fft_ceil_lin_q(len2 + in4);
                        mu_in = (div_fl4 < div_fl2) ? in4 : in2;
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
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


#include <cstdio>
#include <cstring>
#ifdef __linux__
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#endif
namespace {
#if defined(HINT_OP_DIV)
    static constexpr size_t OBUF_SIZE = 32 << 20;  
#else
    static constexpr size_t OBUF_SIZE = 8 << 20;   
#endif
    
    
    static char oBufferFallback[OBUF_SIZE];
    static char* oBuffer = oBufferFallback;
    static char* oCursor = oBufferFallback;

    void writeHint(const hint::Integer& val) {
        oCursor += val.writeTo(oCursor);
    }

    void flushOutput() {
        
        
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

    
    
    static char iBuffer[8 << 20];  
    static const char *iCursor = iBuffer, *iEnd = iBuffer;

    static void initInput() {
        
        
        
        
        _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
        _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#ifdef __linux__
        
        
        if (oBuffer == oBufferFallback) {
            void *m = mmap(nullptr, OBUF_SIZE, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (m != MAP_FAILED) {
                madvise(m, OBUF_SIZE, MADV_HUGEPAGE);
                oBuffer = static_cast<char *>(m);
                oCursor = oBuffer;
            }
        }
        
        struct stat status;
        if (fstat(STDIN_FILENO, &status) == 0 && status.st_size > 0) {
            void *p = mmap(nullptr, status.st_size, PROT_READ, MAP_PRIVATE, STDIN_FILENO, 0);
            if (p != MAP_FAILED) {
                
                madvise(p, status.st_size, MADV_SEQUENTIAL | MADV_WILLNEED);
                iCursor = reinterpret_cast<const char *>(p);
                iEnd = iCursor + status.st_size;
                return;
            }
        }
#endif
        size_t n = std::fread(iBuffer, 1, sizeof(iBuffer) - 1, stdin);
        iBuffer[n] = '\n';  
        iCursor = iBuffer;
        iEnd = iBuffer + n;
    }

    
    
    
    
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
        
        while (p + offset < iEnd && p[offset] >= 0x21) {
            offset++;
        }
        return offset;
    }

    
    
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

    
    
    static inline uint64_t parse8SWAR_u64(uint64_t u) {
        u = (u & 0x0F0F0F0F0F0F0F0FULL) * 2561ULL;
        u = ((u >> 8) & 0x00FF00FF00FF00FFULL) * 6553601ULL;
        u = ((u >> 16) & 0x0000FFFF0000FFFFULL) * 42949672960001ULL;
        return u >> 32;
    }

    
    
    
    
    
    
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

        
        uint64_t dl = __builtin_ctzll(umask ^ cx30) >> 3;
        if (dl == 0) { len = 0; return 0; }
        u <<= 64 - (dl << 3);
        uint64_t r = parse8SWAR_u64(u);
        len = dl;
        return (int64_t)r;
    }

    
    
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
        
        uint32_t limbs[5];
        int n = 0;
        while (uv >= 10000) {
            limbs[n++] = uint32_t(uv % 10000);
            uv /= 10000;
        }
        limbs[n++] = uint32_t(uv); 
        
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
        
        if (la > 0 && la <= 18 && lb > 0 && lb <= 18 && sa[0] != '-' && sb[0] != '-' && vb != 0) {
            writeI64(va / vb);
            *oCursor++ = ' ';
            writeI64(va % vb);
        } else if (tryParseI64Unchecked(sa, la, va) && tryParseI64Unchecked(sb, lb, vb) && vb != 0) {
            
            writeI64(va / vb);
            *oCursor++ = ' ';
            writeI64(va % vb);
        } else {
            
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
