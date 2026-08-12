#ifndef _WIN_MEMALIGN_SHIM_H
#define _WIN_MEMALIGN_SHIM_H
// Shim for MinGW: provides posix_memalign via _aligned_malloc
// and redirects free() to _aligned_free() for AlignedAlloc32::deallocate.
// Usage: g++ -include win_memalign_shim.h ...
#include <malloc.h>
static inline int __shim_posix_memalign(void** p, size_t alignment, size_t size)
{
    void* q = _aligned_malloc(size, alignment);
    if (!q) return 12; // ENOMEM
    *p = q;
    return 0;
}
#define posix_memalign __shim_posix_memalign
// best/div.cpp uses free(p) only in AlignedAlloc32::deallocate (L101),
// which releases posix_memalign-allocated memory. _aligned_malloc MUST be
// paired with _aligned_free, so redirect free -> _aligned_free here.
#define free(p) _aligned_free(p)
#endif
