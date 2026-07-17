#include <gmp.h>
#include <cstdio>
int main(){
    mpz_t a; mpz_init(a); mpz_set_ui(a, 42);
    gmp_printf("%Zd\n", a);
    mpz_clear(a);
    return 0;
}
