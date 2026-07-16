#include "mason_compat.hpp"
#include "masonxiong.cpp"
#include <chrono>
#include <iostream>
#include <random>
#include <string>

using Clock = std::chrono::high_resolution_clock;

// Prevent the optimizer from eliding the computed result.
static volatile const void* volatile g_sink = nullptr;
template <typename T>
inline void escape(T const& v) { g_sink = static_cast<const void*>(&v); }

static std::string make_number(int digits, std::uint32_t seed) {
    std::mt19937 rng(seed);
    std::string s;
    s.reserve(digits);
    s += char('1' + rng() % 9);
    for (int i = 1; i < digits; ++i)
        s += char('0' + rng() % 10);
    return s;
}

template <typename F>
static long long ms(F&& f) {
    auto t0 = Clock::now();
    f();
    auto t1 = Clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
}

int main() {
    struct Case { int digits; const char* tag; };
    Case cases[] = { {1000, "1k"}, {10000, "10k"}, {100000, "100k"}, {1000000, "1M"} };

    for (auto c : cases) {
        std::string sa = make_number(c.digits, 1);
        std::string sb = make_number(c.digits, 2);

        UnsignedInteger a(sa), b(sb);
        UnsignedInteger big = make_number(c.digits * 2 + 1, 3); // for division

        std::cout << "[" << c.tag << "] digits=" << c.digits << "\n";
        std::cout << "  add  " << ms([&]{ UnsignedInteger r = a + b; escape(r); }) << " ms\n";
        std::cout << "  sub  " << ms([&]{ UnsignedInteger r = a - (b <= a ? b : a); escape(r); }) << " ms\n";
        std::cout << "  mul  " << ms([&]{ UnsignedInteger r = a * b; escape(r); }) << " ms\n";
        std::cout << "  div  " << ms([&]{ UnsignedInteger r = big / a; escape(r); }) << " ms\n";
        std::cout << std::flush;
    }

    // sanity check
    UnsignedInteger a("12345678901234567890");
    UnsignedInteger b("98765432109876543210");
    std::cout << "sanity 12345678901234567890 * 98765432109876543210 =\n"
              << (a * b) << "\n";
    return 0;
}
