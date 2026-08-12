/*
Submission #216205
ID	Date	Problem	Lang	User	Status	Time	Memory
216205	2024/6/20 22:55:01	

Division of Hex Big Integers
	C++23	toxicpie	AC	113 ms	10.59 Mib
	Name	Status	Time	Memory
	a_max_b_random_00	AC	44 ms	7.02 Mib
	a_max_b_random_01	AC	44 ms	8.38 Mib
	a_max_b_random_02	AC	53 ms	8.41 Mib
	burnikel_ziegler_bound_00	AC	16 ms	2.79 Mib
	burnikel_ziegler_bound_01	AC	19 ms	2.99 Mib
	burnikel_ziegler_bound_02	AC	15 ms	2.84 Mib
	burnikel_ziegler_bound_03	AC	19 ms	3.00 Mib
	example_00	AC	2 ms	0.71 Mib
	large_00	AC	11 ms	2.66 Mib
	large_01	AC	9 ms	2.63 Mib
	length_ratio_integer_00	AC	58 ms	8.12 Mib
	length_ratio_integer_01	AC	67 ms	8.30 Mib
	length_ratio_integer_02	AC	71 ms	8.80 Mib
	length_ratio_integer_03	AC	77 ms	9.44 Mib
	length_ratio_integer_04	AC	66 ms	7.85 Mib
	length_ratio_integer_05	AC	57 ms	8.01 Mib
	max_00	AC	12 ms	6.68 Mib
	max_01	AC	12 ms	5.34 Mib
	max_02	AC	15 ms	10.59 Mib
	medium_00	AC	65 ms	2.89 Mib
	medium_01	AC	15 ms	2.01 Mib
	medium_02	AC	10 ms	2.21 Mib
	power_00	AC	12 ms	1.71 Mib
	r_nearly_zero_00	AC	79 ms	2.40 Mib
	r_nearly_zero_01	AC	9 ms	1.21 Mib
	r_nearly_zero_02	AC	8 ms	1.80 Mib
	small_00	AC	113 ms	3.73 Mib
*/
#include <dlfcn.h>
#include <iostream>
#include <unistd.h>
#include <unordered_map>

using namespace std;


// assumes no bad things will ever happen
namespace DLL {
unordered_map<string, void *> syms;
void *find_name(const char *name) {
    if (auto it = syms.find(name); it != syms.end()) {
        return it->second;
    }
    return syms[name] = dlsym(RTLD_DEFAULT, name);
}
template <typename R = void, typename... T> R call(const char *name, T... t) {
    auto func = reinterpret_cast<R (*)(T...)>(find_name(name));
    return func(t...);
};
}; // namespace DLL

#ifdef EVAL
constexpr const char *LIB_PATH = "/usr/lib/x86_64-linux-gnu/libgmp.so.10";
#else
constexpr const char *LIB_PATH = "libgmp.so";
#endif

static_assert(sizeof(unsigned long) == 8);
using mpz_t = char[16];

int main(int argc, char **argv) {
    if (getenv("LD_PRELOAD") == nullptr) {
        setenv("LD_PRELOAD", LIB_PATH, 1);
        execve("/proc/self/exe", argv, environ);
        exit(0);
    }

    ios_base::sync_with_stdio(0), cin.tie(0);

    mpz_t a, b, q, r;

    DLL::call("__gmpz_init", a);
    DLL::call("__gmpz_init", b);
    DLL::call("__gmpz_init", q);
    DLL::call("__gmpz_init", r);

    int t;
    cin >> t;
    while (t--) {
        string sa, sb;
        cin >> sa >> sb;
        if (sa.size() <= 16 && sb.size() <= 16) {
            unsigned long ua = stoul(sa, nullptr, 16),
                          ub = stoul(sb, nullptr, 16);
            auto uq = ua / ub, ur = ua % ub;
            DLL::call("__gmp_printf", "%lX %lX\n", uq, ur);
        } else if (sa.size() <= 16) {
            DLL::call("__gmp_printf", "0 %s\n", sa.c_str());
        } else if (sb.size() <= 16) {
            DLL::call("__gmpz_set_str", a, sa.c_str(), 16);
            unsigned long ub = stoul(sb, nullptr, 16);
            auto ur = DLL::call<unsigned long>("__gmpz_tdiv_q_ui", q, a, ub);
            DLL::call("__gmp_printf", "%ZX %lX\n", q, ur);
        } else {
            DLL::call("__gmpz_set_str", a, sa.c_str(), 16);
            DLL::call("__gmpz_set_str", b, sb.c_str(), 16);
            DLL::call("__gmpz_tdiv_qr", q, r, a, b);
            DLL::call("__gmp_printf", "%ZX %ZX\n", q, r);
        }
    }

    DLL::call("__gmpz_clear", a);
    DLL::call("__gmpz_clear", b);
    DLL::call("__gmpz_clear", q);
    DLL::call("__gmpz_clear", r);
}
