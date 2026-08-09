// GMP mpn 纯除法基准: 跳过 I/O, 仅测 mpn_tdiv_qr / mpn_mu_div_qr / mpn_sbpi1_div_qr 速度
// 编译: g++ -O2 -std=gnu++20 bench_mpn_div.cpp -o bench_mpn_div -lgmp
#include <gmp.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <chrono>
#include <vector>

// 用 mpz 生成随机数, 然后导出为 mpn limb 数组 (2进制 limb)
// 这样除法纯计算, 无 I/O 进制转换

static double now_s() {
    return std::chrono::duration<double>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

int main(int argc, char **argv) {
    // 参数: <case_file> <iters>
    // case_file 格式与 div 相同 (首行 t, 后续 a b 对)
    // 但我们只读入做 mpz, 然后导出 mpn, 再测 mpn_tdiv_qr
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <case_file> <iters>\n", argv[0]);
        return 1;
    }
    int iters = atoi(argv[2]);
    FILE *fp = fopen(argv[1], "r");
    if (!fp) { perror("fopen"); return 1; }

    size_t t;
    if (fscanf(fp, "%zu", &t) != 1) { fprintf(stderr, "bad input\n"); return 1; }
    fprintf(stderr, "[debug] t=%zu\n", t);

    // 读所有 case 为 mpz (手动读字符串再 set_str, 避免 gmp_fscanf 大数问题)
    // 用纯指针数组, 避免 std::vector<mpz_t> 的 length_error
    __mpz_struct **A = new __mpz_struct*[t];
    __mpz_struct **B = new __mpz_struct*[t];
    for (size_t i = 0; i < t; i++) {
        A[i] = new __mpz_struct;
        B[i] = new __mpz_struct;
        mpz_init(A[i]);
        mpz_init(B[i]);
        // 手动读 token (跳过空白)
        auto readTok = [](FILE *f) -> std::string {
            int c;
            std::string s;
            while ((c = fgetc(f)) != EOF && isspace(c));
            while (c != EOF && !isspace(c)) { s += (char)c; c = fgetc(f); }
            return s;
        };
        std::string sa = readTok(fp), sb = readTok(fp);
        mpz_set_str(A[i], sa.c_str(), 10);
        mpz_set_str(B[i], sb.c_str(), 10);
    }
    fclose(fp);

    // 导出为 mpn limb 数组 (高位在前转低位在前)
    // mpz_get_limb: limb 0 是最低位
    std::vector<std::vector<mp_limb_t>> a_limbs(t), b_limbs(t);
    std::vector<size_t> a_sizes(t), b_sizes(t);
    for (size_t i = 0; i < t; i++) {
        a_sizes[i] = mpz_size(A[i]);
        b_sizes[i] = mpz_size(B[i]);
        a_limbs[i].resize(a_sizes[i] ? a_sizes[i] : 1);
        b_limbs[i].resize(b_sizes[i] ? b_sizes[i] : 1);
        if (a_sizes[i]) memcpy(a_limbs[i].data(), A[i]->_mp_d, a_sizes[i] * sizeof(mp_limb_t));
        if (b_sizes[i]) memcpy(b_limbs[i].data(), B[i]->_mp_d, b_sizes[i] * sizeof(mp_limb_t));
    }

    // 准备 quotient/remainder 缓冲 (取最大尺寸)
    size_t max_q = 0, max_r = 0;
    for (size_t i = 0; i < t; i++) {
        // 保护: a<b 时 qn=1, 避免下溢
        size_t qn = (a_sizes[i] >= b_sizes[i]) ? (a_sizes[i] - b_sizes[i] + 1) : 1;
        if (qn > max_q) max_q = qn;
        if (b_sizes[i] > max_r) max_r = b_sizes[i];
        fprintf(stderr, "[debug] case %zu: a_size=%zu b_size=%zu qn=%zu\n", i, a_sizes[i], b_sizes[i], qn);
    }
    max_q = (max_q ? max_q : 1) + 2;
    max_r = (max_r ? max_r : 1) + 2;
    fprintf(stderr, "[debug] max_q=%zu max_r=%zu\n", max_q, max_r);

    std::vector<mp_limb_t> qp(max_q), rp(max_r);

    // 预热: 1 次 (跳过 a<b 和零除数)
    for (size_t i = 0; i < t; i++) {
        if (a_sizes[i] == 0 || b_sizes[i] == 0) continue;
        if (a_sizes[i] < b_sizes[i]) continue;  // a<b: q=0 r=a, 跳过
        mpn_tdiv_qr(qp.data(), rp.data(), 0,
                    a_limbs[i].data(), a_sizes[i],
                    b_limbs[i].data(), b_sizes[i]);
    }

    // 计时: iters 次
    double t0 = now_s();
    for (int iter = 0; iter < iters; iter++) {
        for (size_t i = 0; i < t; i++) {
            if (a_sizes[i] == 0 || b_sizes[i] == 0) continue;
            if (a_sizes[i] < b_sizes[i]) continue;
            mpn_tdiv_qr(qp.data(), rp.data(), 0,
                        a_limbs[i].data(), a_sizes[i],
                        b_limbs[i].data(), b_sizes[i]);
        }
    }
    double t1 = now_s();

    double total_ms = (t1 - t0) * 1000.0;
    double avg_ms = total_ms / iters;
    printf("%.3f", avg_ms);  // 输出纯数字: 平均每次 ms
    return 0;
}
