// Library Checker: Addition of Big Integers
// https://judge.yosupo.jp/problem/addition_of_big_integers
// 输入: T / A B (T 组，A B 可带负号)
// 输出: 每组一行 A+B
// I/O: mmap 输入 + 32MB oBuffer 输出 + SWAR 快读（复刻原作者 origin_add）
// 提交方式: 将 masonxiong_opt.cpp 全部内容 + 以下 main 函数合并粘贴
// 编译选项: -march=native (LC 默认已开启)

#include <iostream>
#include <cstdio>

// [PASTE masonxiong_opt.cpp HERE]

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

    // SWAR 一次检测 8 字节找空白，绕过 std::string 中间层，直接 construct 到 UnsignedInteger
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

    // 把 UnsignedInteger 的十进制表示直接写入 oBuffer（展开 operator const char*，避免 thread_local 检查 + strlen）
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
    // 读 T（普通循环，OPT 的 InputHelper 没把非数字位初始化为 -1，不能用原作者的 ~I 技巧）
    while (*iCursor >= '0' && *iCursor <= '9') t = t * 10 + (*iCursor++ - '0');
    ++iCursor; // skip newline
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
