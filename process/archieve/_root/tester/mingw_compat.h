#pragma once
#include <malloc.h>
#include <errno.h>
// MinGW 缺少 posix_memalign, 用 _aligned_malloc 替代
// posix_memalign(void **p, size_t align, size_t size) -> int (0=ok)
// _aligned_malloc(size_t size, size_t align) -> void*
inline int posix_memalign(void **p, size_t align, size_t size) {
    *p = _aligned_malloc(size, align);
    return *p ? 0 : ENOMEM;
}
