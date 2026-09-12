// 喵喵喵~ https://space.bilibili.com/620657947
// GMP 真身除法测速: 读同一个 case, 用 mpz_tdiv_qr 做除法.
// 用途: 对比 GMP 的 mpn_mu_div_qr (cyclic mulmod based) 与我们线性 FFT 路径的指令数.
// 用法: gmp_div_test <case_file> <reps> <do_output>
//   reps=1 全程单次(含解析); reps=N 摊薄解析测纯除法内核; do_output=1 才输出 q r
#include <gmp.h>
#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv){
    if (argc < 2){ fprintf(stderr, "usage: %s <file> [reps] [do_out]\n", argv[0]); return 1; }
    const char* path = argv[1];
    int reps   = (argc > 2) ? atoi(argv[2]) : 1;
    int do_out = (argc > 3) ? atoi(argv[3]) : 0;

    FILE* fp = fopen(path, "r");
    if (!fp){ fprintf(stderr, "open fail\n"); return 1; }
    int T = 0;
    if (fscanf(fp, "%d", &T) != 1){ fprintf(stderr, "no T\n"); return 1; }

    mpz_t A, B, Q, R;
    mpz_inits(A, B, Q, R, NULL);
    // 读入首个 case (one_case.txt 中 T=1)
    if (mpz_inp_str(A, fp, 16) == 0){ fprintf(stderr, "no A\n"); return 1; }
    if (mpz_inp_str(B, fp, 16) == 0){ fprintf(stderr, "no B\n"); return 1; }
    fclose(fp);

    for (int i = 0; i < reps; ++i){
        mpz_tdiv_qr(Q, R, A, B);   // 精确商+余数, GMP 自动选 mu_div_qr
    }

    if (do_out){
        mpz_out_str(stdout, 16, Q); fputc(' ', stdout);
        mpz_out_str(stdout, 16, R); fputc('\n', stdout);
    } else {
        // 抑制输出但强制不被优化掉: 打印低位 limb
        gmp_fprintf(stderr, "Qlimbs=%d Rlimbs=%d\n",
                    (int)mpz_size(Q), (int)mpz_size(R));
    }
    mpz_clears(A, B, Q, R, NULL);
    return 0;
}
