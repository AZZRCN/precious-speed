/* GMP-based correct for Division of Big Integers */
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
        mpz_t a, b, q, r;
        mpz_init(a);
        mpz_init(b);
        mpz_init(q);
        mpz_init(r);
        mpz_set_str(a, buf1, 10);
        mpz_set_str(b, buf2, 10);
        mpz_tdiv_qr(q, r, a, b);
        char *qs = mpz_get_str(NULL, 10, q);
        char *rs = mpz_get_str(NULL, 10, r);
        printf("%s %s\n", qs, rs);
        free(qs);
        free(rs);
        mpz_clear(a);
        mpz_clear(b);
        mpz_clear(q);
        mpz_clear(r);
    }
    return 0;
}
