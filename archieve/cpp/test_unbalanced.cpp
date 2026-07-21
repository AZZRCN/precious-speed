// 调试不平衡乘法: 生成一个 2000*500 位数的乘法（~500*125 limbs → 不平衡）
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>
#include <algorithm>

int main() {
    // 测试: A=2000 digits (~500 limbs), B=500 digits (~125 limbs)
    // Ratio = 500/125 = 4, both >= 64 limbs → 走 fftMulUnbalanced
    // AND sml=125 < 4096 → 不触发不平衡，走正常 fftMul
    
    // 用更大的: A=40000 digits (~10000 limbs), B=2000 digits (~500 limbs)
    // Ratio = 10000/500 = 20, sml=500 < 4096 → 还是不触发
    
    // 需要 sml >= 4096. 用: A=100000 digits (~25000 limbs), B=20000 digits (~5000 limbs)
    // Ratio = 25000/5000 = 5, sml=5000 >= 4096 → 触发!! 
    int a_digits = 100000;
    int b_digits = 20000;
    
    // 生成并写入文件
    std::mt19937 rng(42);
    FILE *f = fopen("test_unbalanced.txt", "w");
    fprintf(f, "1\n");
    fputc('1' + rng() % 9, f);
    for (int i = 1; i < a_digits; i++) fputc('0' + rng() % 10, f);
    fputc('\n', f);
    fputc('1' + rng() % 9, f);
    for (int i = 1; i < b_digits; i++) fputc('0' + rng() % 10, f);
    fputc('\n', f);
    fclose(f);
    
    printf("Generated test_unbalanced.txt: A=%d digits, B=%d digits\n", a_digits, b_digits);
    printf("\nRun:\n");
    printf("  moptm_MUL.exe < test_unbalanced.txt > out_m.txt\n");
    printf("  best_mul.exe  < test_unbalanced.txt > out_b.txt\n");
    printf("  fc /b out_m.txt out_b.txt\n");
    return 0;
}
