// fuzz_div.cpp - Fuzz test moptm division against masonxiong_opt
// Compile: g++ -O2 -mavx2 -mfma -funroll-loops -o fuzz_div.exe fuzz_div.cpp
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <random>

// Include both implementations in separate namespaces
namespace moptm_ns {
    #define MOPTM_NO_IO
    #include "moptm.cpp"
}
namespace mason_ns {
    #define main mason_main_unused
    #include "masonxiong_opt.cpp"
    #undef main
}

// Need to bridge between namespaces - generate random numbers as strings
static void gen_num(std::mt19937& rng, int digits, char* buf) {
    buf[0] = char('1' + rng() % 9);
    for (int i = 1; i < digits; i++)
        buf[i] = char('0' + rng() % 10);
    buf[digits] = 0;
}

int main(int argc, char* argv[]) {
    int seed = argc > 1 ? atoi(argv[1]) : 42;
    int rounds = argc > 2 ? atoi(argv[2]) : 1000;
    std::mt19937 rng(seed);

    int fails = 0;
    for (int r = 0; r < rounds; r++) {
        // Generate random lengths - focus on blocks >= 3 (Core2 path)
        int na, nb;
        int path = rng() % 5;
        switch (path) {
            case 0: // Core2 blocks=3
                nb = 50 + rng() % 200;
                na = nb * 3 + rng() % nb;
                break;
            case 1: // Core2 blocks=4-8
                nb = 50 + rng() % 100;
                na = nb * (4 + rng() % 5) + rng() % nb;
                break;
            case 2: // Core1 (blocks=1-2)
                nb = 100 + rng() % 500;
                na = nb + rng() % (nb * 2);
                break;
            case 3: // Small (bruteforce)
                nb = 1 + rng() % 20;
                na = nb + rng() % 100;
                break;
            case 4: // Edge: nb=1
                nb = 1;
                na = 1 + rng() % 50;
                break;
        }

        char* a_str = new char[na + 1];
        char* b_str = new char[nb + 1];
        gen_num(rng, na, a_str);
        gen_num(rng, nb, b_str);

        // Compute with moptm
        moptm_ns::UnsignedInteger a_moptm(a_str), b_moptm(b_str);
        auto [q_moptm, r_moptm] = a_moptm.divisionAndModulus(b_moptm);

        // Compute with masonxiong_opt
        mason_ns::UnsignedInteger a_mason(a_str), b_mason(b_str);
        auto [q_mason, r_mason] = a_mason.divisionAndModulus(b_mason);

        // Compare
        std::string qm = (std::string)q_moptm;
        std::string qs = (std::string)q_mason;
        std::string rm = (std::string)r_moptm;
        std::string rs = (std::string)r_mason;

        if (qm != qs || rm != rs) {
            fails++;
            printf("FAIL round %d: na=%d nb=%d\n", r, na, nb);
            printf("  A: %s\n", na <= 100 ? a_str : "(long)");
            printf("  B: %s\n", nb <= 100 ? b_str : "(long)");
            // Show first divergence
            int qlen = std::min(qm.length(), qs.length());
            int qdiff = -1;
            for (int i = 0; i < qlen; i++) {
                if (qm[i] != qs[i]) { qdiff = i; break; }
            }
            if (qdiff >= 0) {
                printf("  Q diff at pos %d: moptm='%c' mason='%c'\n", qdiff, qm[qdiff], qs[qdiff]);
                printf("  Q moptm (around diff): ...%s\n", qm.substr(std::max(0,qdiff-10), 30).c_str());
                printf("  Q mason (around diff): ...%s\n", qs.substr(std::max(0,qdiff-10), 30).c_str());
            } else {
                printf("  Q length diff: moptm=%zu mason=%zu\n", qm.length(), qs.length());
            }
            printf("  R moptm: %s\n", rm.length() <= 100 ? rm.c_str() : "(long)");
            printf("  R mason: %s\n", rs.length() <= 100 ? rs.c_str() : "(long)");
            // Also verify: A = Q*B + R
            {
                auto check = q_moptm * b_moptm + r_moptm;
                std::string cs = (std::string)check;
                printf("  Verify moptm: A == Q*B+R ? %s\n", cs == std::string(a_str) ? "YES" : "NO");
            }
            if (fails >= 5) {
                printf("Too many failures, stopping.\n");
                break;
            }
        }

        delete[] a_str;
        delete[] b_str;
    }
    printf("Done: %d fails out of %d rounds\n", fails, rounds);
    return fails > 0 ? 1 : 0;
}
