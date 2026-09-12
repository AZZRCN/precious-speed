# -*- coding: utf-8 -*-
"""
D27 = D25 + Zen3 大页 Arena 分配器 (LC 特调)

LC 判题机 (官方 help 确认):
    GCP c2d-highcpu-8 / AMD EPYC 7B13 (Zen3 "Milan"), 限 1 核, 1 GiB
    L1d 32KB/8way  L2 512KB/8way  L3 32MB/16way(victim)
    L1 DTLB 64 项 -> 4KB 页覆盖仅 256KB;  L2 TLB 2048 项 -> 覆盖 8MB
    DIV 峰值工作集 33MB => 连 L2 TLB 都装不下
    且 Zen3 硬件预取器不跨 4KB 页边界 -> FFT 长距顺序遍历被反复打断

实测 (VM, /usr/bin/time):
    amb_02  minflt=6490  rss=33.1MB
    lri_00  minflt=4718  rss=28.1MB

对策: 所有堆分配统一走一块 2MB 对齐 + MADV_HUGEPAGE 的 arena
    缺页 6490 -> ~20 ; TLB 覆盖 8MB -> 128MB ; 预取器可在 2MB 内连续工作

关: -DHP_ARENA_OFF   统计: -DHP_STATS
"""
import io
import os
import shutil
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'best', 'div_D25.cpp')
DST = os.path.join(ROOT, 'best', 'div_D27.cpp')

ARENA = r'''
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
'''

OLD_COMMENT = '''// LC 评测机: GCP c2-standard-4 (Cascade Lake, AVX2+FMA+BMI2)'''
NEW_COMMENT = '''// LC 评测机 (judge.yosupo.jp/help 官方确认): GCP c2d-highcpu-8
//   AMD EPYC 7B13 = Zen3 "Milan", 限 1 核, 1 GiB 内存
//   AVX2+BMI2 可用; 无 AVX-512 (Zen3 不支持)'''

ANCHOR = '#include <chrono>'

# ---- C: AlignedAlloc32 接管 ----------------------------------------------
# 这是真正的大头: FFT 全部工作数组走 std::vector<T, AlignedAlloc32<T>>,
# 其 allocate 用 posix_memalign / deallocate 用 free, 完全绕过 operator new.
# strace 实测: 运行期 38 次 mmap (1MB/1.5MB/2MB/3MB) + 14 次 munmap 全部由它产生,
# 全是 4KB 页, 且反复 mmap/munmap -> 每轮重新缺页.
ALLOC_OLD = '''            if (posix_memalign(&p, 32, n * sizeof(T)) != 0)
                throw std::bad_alloc();'''
ALLOC_NEW = '''#if defined(__linux__) && !defined(HP_ARENA_OFF)
            // D27: 走大页 arena (2MB 页 + freelist 复用, 消除 mmap/munmap syscall)
            p = ::hparena::take(n * sizeof(T), 32);
            if (p)
                return static_cast<T *>(p);
#endif
            if (posix_memalign(&p, 32, n * sizeof(T)) != 0)
                throw std::bad_alloc();'''

DEALLOC_OLD = '''        void deallocate(T *p, size_t)
        {
#ifdef _WIN32
            _aligned_free(p);
#else
            free(p);
#endif
        }'''
DEALLOC_NEW = '''        void deallocate(T *p, size_t)
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
        }'''


def main():
    shutil.copyfile(SRC, DST)
    s = io.open(DST, encoding='utf-8', errors='replace').read()
    n0 = len(s)

    # 1) 修正过时的架构注释
    if OLD_COMMENT in s:
        s = s.replace(OLD_COMMENT, NEW_COMMENT, 1)
        print('[A] arch comment fixed')
    else:
        print('[A] WARN: arch comment not found')

    # 2) 插入 arena (在 <chrono> 之前, 此时基础 include 已齐备)
    i = s.find(ANCHOR)
    if i < 0:
        print('[B] FATAL: anchor not found')
        sys.exit(1)
    s = s[:i] + ARENA + '\n' + s[i:]
    print('[B] arena inserted @%d' % i)

    # 3) 接管 AlignedAlloc32 (FFT 工作数组的真正分配器)
    if s.count(ALLOC_OLD) != 1:
        print('[C] FATAL: allocate anchor count=%d' % s.count(ALLOC_OLD))
        sys.exit(1)
    s = s.replace(ALLOC_OLD, ALLOC_NEW, 1)
    print('[C] AlignedAlloc32::allocate -> arena')

    if s.count(DEALLOC_OLD) != 1:
        print('[D] FATAL: deallocate anchor count=%d' % s.count(DEALLOC_OLD))
        sys.exit(1)
    s = s.replace(DEALLOC_OLD, DEALLOC_NEW, 1)
    print('[D] AlignedAlloc32::deallocate -> arena')

    io.open(DST, 'w', encoding='utf-8').write(s)
    print('written %s : %d -> %d bytes' % (DST, n0, len(s)))


if __name__ == '__main__':
    main()
