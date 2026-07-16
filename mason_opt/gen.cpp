#include <cstdio>
#include <cstdlib>
#include <ctime>
int main(int argc, char** argv) {
    int digitsA = atoi(argv[1]);
    int digitsB = atoi(argv[2]);
    int T = atoi(argv[3]);
    unsigned seed = argc > 4 ? (unsigned)atoi(argv[4]) : (unsigned)time(0);
    srand(seed);
    printf("%d\n", T);
    for (int t = 0; t < T; ++t) {
        for (int i = 0; i < digitsA; ++i)
            putchar(i == 0 ? '1' + rand() % 9 : '0' + rand() % 10);
        putchar(' ');
        for (int i = 0; i < digitsB; ++i)
            putchar(i == 0 ? '1' + rand() % 9 : '0' + rand() % 10);
        putchar('\n');
    }
    return 0;
}
