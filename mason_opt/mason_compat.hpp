#ifndef MASON_COMPAT_HPP
#define MASON_COMPAT_HPP

// MSVC lacks __builtin_ctz; provide a shim so masonxiong.cpp compiles unchanged.
#ifdef _MSC_VER
#include <intrin.h>
static __forceinline unsigned long __builtin_ctz(unsigned int x) {
    unsigned long r = 0;
    _BitScanForward(&r, x);
    return r;
}
#endif

#endif
