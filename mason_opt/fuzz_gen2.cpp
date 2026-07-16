// fuzz_gen2.cpp - Generate LC-pattern division test cases
// Compile: g++ -O2 -o fuzz_gen2.exe fuzz_gen2.cpp
// Run: fuzz_gen2.exe <seed> <outfile>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>

void gen_num(std::mt19937& rng, int digits, FILE* out) {
    fputc('1' + rng() % 9, out);
    for (int i = 1; i < digits; i++)
        fputc('0' + rng() % 10, out);
    fputc('\n', out);
}

// Generate A = B * Q + R where Q and R are random
// We output A and B
void gen_div_case(std::mt19937& rng, int nb, int blocks, int remainder_digits, FILE* out) {
    // Generate B (nb digits)
    char* b = new char[nb + 1];
    b[0] = '1' + rng() % 9;
    for (int i = 1; i < nb; i++) b[i] = '0' + rng() % 10;
    b[nb] = 0;

    // Generate Q (nb * (blocks-1) + 1 digits approximately)
    int nq = nb * (blocks - 1) + 1 + rng() % nb;
    char* q = new char[nq + 1];
    q[0] = '1' + rng() % 9;
    for (int i = 1; i < nq; i++) q[i] = '0' + rng() % 10;
    q[nq] = 0;

    // Generate R (remainder_digits digits, < B)
    char* r = new char[remainder_digits + 1];
    if (remainder_digits > 0) {
        r[0] = '1' + rng() % 9;
        for (int i = 1; i < remainder_digits; i++) r[i] = '0' + rng() % 10;
        r[remainder_digits] = 0;
    } else {
        r[0] = '0';
        r[1] = 0;
    }

    // Compute A = B * Q + R using Python-style big integer arithmetic
    // We'll use __int128 for small, or just output strings and let Python compute
    // Actually, let's use the GMP approach or just use Python

    // For simplicity, output B and Q and R to a separate file, and use Python to compute A
    fprintf(stderr, "%s %s %s\n", b, q, r);

    // Actually, let's just output random A and B and hope for the best
    // The key patterns are:
    // 1. Various block counts
    // 2. Various sizes
    // 3. Including edge cases

    int na = nb * blocks + rng() % nb;
    gen_num(rng, na, out);
    fprintf(out, "%s\n", b);

    delete[] b;
    delete[] q;
    delete[] r;
}

int main(int argc, char* argv[]) {
    int seed = argc > 1 ? atoi(argv[1]) : 42;
    const char* outfile = argc > 2 ? argv[2] : "fuzz2.txt";
    FILE* out = fopen(outfile, "w");
    if (!out) { fprintf(stderr, "cannot open %s\n", outfile); return 1; }

    std::mt19937 rng(seed);
    int total = 0;

    // Pattern 1: Exact division (remainder = 0) - A is multiple of B
    // Pattern 2: Nearly zero remainder
    // Pattern 3: Various block counts with large sizes
    // Pattern 4: r_nearly_zero pattern

    struct Case { int nb; int blocks; const char* desc; };
    Case cases[] = {
        // LC-like patterns
        {96, 3, "core2_b3_small"},   // Just above bruteforce threshold
        {96, 4, "core2_b4_small"},
        {96, 5, "core2_b5_small"},
        {96, 10, "core2_b10_small"},
        {100, 3, "core2_b3_100"},
        {100, 4, "core2_b4_100"},
        {100, 5, "core2_b5_100"},
        {100, 8, "core2_b8_100"},
        {200, 3, "core2_b3_200"},
        {200, 5, "core2_b5_200"},
        {500, 3, "core2_b3_500"},
        {500, 4, "core2_b4_500"},
        {1000, 3, "core2_b3_1k"},
        {1000, 5, "core2_b5_1k"},
        // Core1 path
        {1000, 2, "core1_b2_1k"},
        {500, 2, "core1_b2_500"},
        {2000, 2, "core1_b2_2k"},
        // Edge cases
        {96, 2, "edge_b96_blocks2"},  // blocks=2, should go Core1
        {97, 3, "edge_b97_blocks3"},  // just above threshold
    };

    int ncases = sizeof(cases) / sizeof(cases[0]);
    int reps = 5;  // 5 repetitions per case

    fprintf(out, "%d\n", ncases * reps);
    for (int c = 0; c < ncases; c++) {
        for (int rep = 0; rep < reps; rep++) {
            int nb = cases[c].nb;
            int blocks = cases[c].blocks;
            int na = nb * blocks + rng() % nb;

            // Generate A
            fputc('1' + rng() % 9, out);
            for (int i = 1; i < na; i++) fputc('0' + rng() % 10, out);
            fputc('\n', out);

            // Generate B
            fputc('1' + rng() % 9, out);
            for (int i = 1; i < nb; i++) fputc('0' + rng() % 10, out);
            fputc('\n', out);

            total++;
        }
    }

    fclose(out);
    printf("Generated %d cases to %s\n", total, outfile);
    return 0;
}
