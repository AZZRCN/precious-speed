// lc_add_moptm.cpp - Addition submission for LC, based on moptm.cpp
// https://judge.yosupo.jp/problem/addition_of_big_integers
// Input: T / A B (T cases, A B may have leading '-')
// Output: A+B per line
// Local test: g++ -O2 -mavx2 -mfma -funroll-loops -o lc_add_moptm.exe lc_add_moptm.cpp
// Submit: merge moptm.cpp (without the #ifndef MOPTM_NO_IO block) + the I/O and main below
#define MOPTM_NO_IO
#include "moptm.cpp"

#ifdef __linux__
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#endif

namespace {
    static char oBuffer[32 << 20], *oCursor = oBuffer;

    static const char* iCursor = []() -> const char* {
#ifdef __linux__
        struct stat status;
        fstat(STDIN_FILENO, &status);
        return reinterpret_cast<const char*>(mmap(nullptr, status.st_size, PROT_READ, MAP_PRIVATE, STDIN_FILENO, 0));
#else
        static char iBuffer[16 << 20];
        std::fread(iBuffer, sizeof(char), sizeof(iBuffer) / sizeof(char), stdin);
        return iBuffer;
#endif
    }();

    void readFromCursor(UnsignedInteger& value) {
        const char* start = iCursor;
        while (true) {
            std::uint64_t data;
            std::memcpy(&data, iCursor, sizeof(data));
            if ((data = (~data) & (data - 0x2121212121212121) & 0x8080808080808080)) {
                const std::uint32_t stringLength = std::uint32_t((iCursor += __builtin_ctzll(data) / int(sizeof(std::uint64_t))) - start);
                if (value.capacity < (value.length = (stringLength + 7) >> 3))
                    detail::DigitAllocator::deallocate(value.digits, value.capacity), value.digits = detail::DigitAllocator::allocate(value.capacity = value.length);
                value.construct(start, stringLength), ++iCursor;
                return;
            }
            iCursor += sizeof(std::uint64_t);
        }
    }

    void writeUnsigned(const UnsignedInteger& a) {
        char* rCursor = oCursor + 8;
        std::uint32_t* i = a.digits + a.length, numberLength = 0;
        for (std::uint32_t number = *--i; *--rCursor = char(48 | number % 10), ++numberLength, number /= 10;);
        for (std::memmove(oCursor, rCursor, numberLength), oCursor += numberLength; i-- != a.digits; std::memcpy(oCursor, detail::O(*i / 10000), 4), oCursor += 4, std::memcpy(oCursor, detail::O(*i % 10000), 4), oCursor += 4);
    }
}

int main() {
    static std::uint32_t t;
    static SignedInteger a, b;
    while (*iCursor >= '0' && *iCursor <= '9') t = t * 10 + (*iCursor++ - '0');
    ++iCursor;
    do {
        iCursor += (a.sign = *iCursor == '-'), readFromCursor(a.absolute);
        iCursor += (b.sign = *iCursor == '-'), readFromCursor(b.absolute);
        a += b;
        if (a.sign && bool(a.absolute)) *oCursor++ = '-';
        writeUnsigned(a.absolute);
        *oCursor++ = '\n';
    } while (--t);
    std::fwrite(oBuffer, sizeof(char), oCursor - oBuffer, stdout);
    return 0;
}
