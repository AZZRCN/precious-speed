// fuzz_gen.cpp - Generate random division test cases
// Compile: g++ -O2 -o fuzz_gen.exe fuzz_gen.cpp
// Run: fuzz_gen.exe <seed> <rounds> <outfile>
#include <cstdio>
#include <cstdlib>
#include <random>

int main(int argc, char* argv[]) {
    int seed = argc > 1 ? atoi(argv[1]) : 42;
    int rounds = argc > 2 ? atoi(argv[2]) : 1000;
    FILE* out = argc > 3 ? fopen(argv[3], "w") : stdout;
    if (!out) { fprintf(stderr, "cannot open output\n"); return 1; }

    std::mt19937 rng(seed);
    fprintf(out, "%d\n", rounds);
    for (int r = 0; r < rounds; r++) {
        int na, nb;
        int path = rng() % 6;
        switch (path) {
            case 0: // Core2 blocks=3
                nb = 20 + rng() % 100;
                na = nb * 3 + rng() % nb;
                break;
            case 1: // Core2 blocks=4-8
                nb = 20 + rng() % 50;
                na = nb * (4 + rng() % 5) + rng() % nb;
                break;
            case 2: // Core1 (blocks=1-2)
                nb = 50 + rng() % 300;
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
            case 5: // Large Core2
                nb = 100 + rng() % 200;
                na = nb * (3 + rng() % 6);
                break;
        }
        // Generate A
        fputc('1' + rng() % 9, out);
        for (int i = 1; i < na; i++) fputc('0' + rng() % 10, out);
        fputc('\n', out);
        // Generate B
        fputc('1' + rng() % 9, out);
        for (int i = 1; i < nb; i++) fputc('0' + rng() % 10, out);
        fputc('\n', out);
    }
    if (out != stdout) fclose(out);
    return 0;
}
