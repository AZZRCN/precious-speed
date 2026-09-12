/* GMP-based correct for Multiplication of Big Integers */
#include <gmp.h>
#include <stdio.h>
#include <stdlib.h>

static char buf1[4000010], buf2[4000010];

int main(void)
{
    int t;
    scanf("%d", &t);
    while (t--)
    {
        scanf("%s %s", buf1, buf2);
        mpz_t a, b, x;
        mpz_init(a);
        mpz_init(b);
        mpz_init(x);
        mpz_set_str(a, buf1, 10);
        mpz_set_str(b, buf2, 10);
        mpz_mul(x, a, b);
        char *xs = mpz_get_str(NULL, 10, x);
        printf("%s\n", xs);
        free(xs);
        mpz_clear(a);
        mpz_clear(b);
        mpz_clear(x);
    }
    return 0;
}
