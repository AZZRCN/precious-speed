// test_write_simd.cpp: 验证 6-AI 综合 WRITE 方案的正确性和性能
// 编译: g++ -O3 -std=c++23 -march=native -o test_write_simd test_write_simd.cpp
#include <immintrin.h>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <vector>
#include <algorithm>

// ============= 6-AI 综合方案 =============
__attribute__((aligned(32)))
void pack_digits_avx2_final(const uint16_t* data, size_t n, char* p) {
    size_t i = n;
    const __m256i c100     = _mm256_set1_epi32(0x28F5D);
    const __m256i c100_val = _mm256_set1_epi32(100);
    const __m256i c10      = _mm256_set1_epi32(0x199A);
    const __m256i c10_val  = _mm256_set1_epi32(10);
    const __m256i ascii0   = _mm256_set1_epi32('0');
    const __m256i rev_mask = _mm256_setr_epi8(
        14,15,12,13,10,11,8,9,6,7,4,5,2,3,0,1,
        14,15,12,13,10,11,8,9,6,7,4,5,2,3,0,1);

    if (i < 16) goto tail;
    do {
        _mm_prefetch((const char*)(data + i - 16 - 32), _MM_HINT_T0);
        // --- 第一轮 ---
        __m256i v256 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&data[i - 16]));
        __m256i v256_swapped = _mm256_permute2x128_si256(v256, v256, 1);
        __m256i v256_rev = _mm256_shuffle_epi8(v256_swapped, rev_mask);
        __m256i v_lo = _mm256_cvtepu16_epi32(_mm256_castsi256_si128(v256_rev));
        __m256i v_hi = _mm256_cvtepu16_epi32(_mm256_extracti128_si256(v256_rev, 1));
        __m256i hi_lo = _mm256_srli_epi32(_mm256_mullo_epi32(v_lo, c100), 24);
        __m256i lo_lo = _mm256_sub_epi32(v_lo, _mm256_mullo_epi32(hi_lo, c100_val));
        __m256i th_lo = _mm256_srli_epi32(_mm256_mullo_epi32(hi_lo, c10), 16);
        __m256i h_lo  = _mm256_sub_epi32(hi_lo, _mm256_mullo_epi32(th_lo, c10_val));
        __m256i t_lo  = _mm256_srli_epi32(_mm256_mullo_epi32(lo_lo, c10), 16);
        __m256i o_lo  = _mm256_sub_epi32(lo_lo, _mm256_mullo_epi32(t_lo, c10_val));
        th_lo = _mm256_add_epi32(th_lo, ascii0);
        h_lo  = _mm256_add_epi32(h_lo,  ascii0);
        t_lo  = _mm256_add_epi32(t_lo,  ascii0);
        o_lo  = _mm256_add_epi32(o_lo,  ascii0);
        o_lo  = _mm256_slli_epi32(o_lo, 24);
        t_lo  = _mm256_slli_epi32(t_lo, 16);
        h_lo  = _mm256_slli_epi32(h_lo, 8);
        __m256i merged_lo = _mm256_or_si256(_mm256_or_si256(o_lo, t_lo), _mm256_or_si256(h_lo, th_lo));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(p), merged_lo);
        __m256i hi_hi = _mm256_srli_epi32(_mm256_mullo_epi32(v_hi, c100), 24);
        __m256i lo_hi = _mm256_sub_epi32(v_hi, _mm256_mullo_epi32(hi_hi, c100_val));
        __m256i th_hi = _mm256_srli_epi32(_mm256_mullo_epi32(hi_hi, c10), 16);
        __m256i h_hi  = _mm256_sub_epi32(hi_hi, _mm256_mullo_epi32(th_hi, c10_val));
        __m256i t_hi  = _mm256_srli_epi32(_mm256_mullo_epi32(lo_hi, c10), 16);
        __m256i o_hi  = _mm256_sub_epi32(lo_hi, _mm256_mullo_epi32(t_hi, c10_val));
        th_hi = _mm256_add_epi32(th_hi, ascii0);
        h_hi  = _mm256_add_epi32(h_hi,  ascii0);
        t_hi  = _mm256_add_epi32(t_hi,  ascii0);
        o_hi  = _mm256_add_epi32(o_hi,  ascii0);
        o_hi  = _mm256_slli_epi32(o_hi, 24);
        t_hi  = _mm256_slli_epi32(t_hi, 16);
        h_hi  = _mm256_slli_epi32(h_hi, 8);
        __m256i merged_hi = _mm256_or_si256(_mm256_or_si256(o_hi, t_hi), _mm256_or_si256(h_hi, th_hi));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(p + 32), merged_hi);
        p += 64; i -= 16;
        if (i < 16) break;
        // --- 第二轮 ---
        v256 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&data[i - 16]));
        v256_swapped = _mm256_permute2x128_si256(v256, v256, 1);
        v256_rev = _mm256_shuffle_epi8(v256_swapped, rev_mask);
        v_lo = _mm256_cvtepu16_epi32(_mm256_castsi256_si128(v256_rev));
        v_hi = _mm256_cvtepu16_epi32(_mm256_extracti128_si256(v256_rev, 1));
        hi_lo = _mm256_srli_epi32(_mm256_mullo_epi32(v_lo, c100), 24);
        lo_lo = _mm256_sub_epi32(v_lo, _mm256_mullo_epi32(hi_lo, c100_val));
        th_lo = _mm256_srli_epi32(_mm256_mullo_epi32(hi_lo, c10), 16);
        h_lo  = _mm256_sub_epi32(hi_lo, _mm256_mullo_epi32(th_lo, c10_val));
        t_lo  = _mm256_srli_epi32(_mm256_mullo_epi32(lo_lo, c10), 16);
        o_lo  = _mm256_sub_epi32(lo_lo, _mm256_mullo_epi32(t_lo, c10_val));
        th_lo = _mm256_add_epi32(th_lo, ascii0);
        h_lo  = _mm256_add_epi32(h_lo,  ascii0);
        t_lo  = _mm256_add_epi32(t_lo,  ascii0);
        o_lo  = _mm256_add_epi32(o_lo,  ascii0);
        o_lo  = _mm256_slli_epi32(o_lo, 24);
        t_lo  = _mm256_slli_epi32(t_lo, 16);
        h_lo  = _mm256_slli_epi32(h_lo, 8);
        merged_lo = _mm256_or_si256(_mm256_or_si256(o_lo, t_lo), _mm256_or_si256(h_lo, th_lo));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(p), merged_lo);
        hi_hi = _mm256_srli_epi32(_mm256_mullo_epi32(v_hi, c100), 24);
        lo_hi = _mm256_sub_epi32(v_hi, _mm256_mullo_epi32(hi_hi, c100_val));
        th_hi = _mm256_srli_epi32(_mm256_mullo_epi32(hi_hi, c10), 16);
        h_hi  = _mm256_sub_epi32(hi_hi, _mm256_mullo_epi32(th_hi, c10_val));
        t_hi  = _mm256_srli_epi32(_mm256_mullo_epi32(lo_hi, c10), 16);
        o_hi  = _mm256_sub_epi32(lo_hi, _mm256_mullo_epi32(t_hi, c10_val));
        th_hi = _mm256_add_epi32(th_hi, ascii0);
        h_hi  = _mm256_add_epi32(h_hi,  ascii0);
        t_hi  = _mm256_add_epi32(t_hi,  ascii0);
        o_hi  = _mm256_add_epi32(o_hi,  ascii0);
        o_hi  = _mm256_slli_epi32(o_hi, 24);
        t_hi  = _mm256_slli_epi32(t_hi, 16);
        h_hi  = _mm256_slli_epi32(h_hi, 8);
        merged_hi = _mm256_or_si256(_mm256_or_si256(o_hi, t_hi), _mm256_or_si256(h_hi, th_hi));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(p + 32), merged_hi);
        p += 64; i -= 16;
    } while (i >= 16);

tail:
    if (i == 0) return;
    while (i >= 8) {
        __m128i v16 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&data[i - 8]));
        v16 = _mm_shuffle_epi8(v16, _mm_setr_epi8(14,15,12,13,10,11,8,9, 6,7,4,5,2,3,0,1));
        __m256i v = _mm256_cvtepu16_epi32(v16);
        __m256i hi = _mm256_srli_epi32(_mm256_mullo_epi32(v, _mm256_set1_epi32(0x28F5D)), 24);
        __m256i lo = _mm256_sub_epi32(v, _mm256_mullo_epi32(hi, _mm256_set1_epi32(100)));
        __m256i th = _mm256_srli_epi32(_mm256_mullo_epi32(hi, _mm256_set1_epi32(0x199A)), 16);
        __m256i h  = _mm256_sub_epi32(hi, _mm256_mullo_epi32(th, _mm256_set1_epi32(10)));
        __m256i te = _mm256_srli_epi32(_mm256_mullo_epi32(lo, _mm256_set1_epi32(0x199A)), 16);
        __m256i o  = _mm256_sub_epi32(lo, _mm256_mullo_epi32(te, _mm256_set1_epi32(10)));
        __m256i a0 = _mm256_set1_epi32('0');
        th = _mm256_add_epi32(th, a0); h = _mm256_add_epi32(h, a0);
        te = _mm256_add_epi32(te, a0); o = _mm256_add_epi32(o, a0);
        o  = _mm256_slli_epi32(o, 24);
        te = _mm256_slli_epi32(te, 16);
        h  = _mm256_slli_epi32(h, 8);
        __m256i merged = _mm256_or_si256(_mm256_or_si256(o, te), _mm256_or_si256(h, th));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(p), merged);
        p += 32; i -= 8;
    }
    while (i--) {
        uint16_t x = data[i];
        p[3] = '0' + (x % 10); x /= 10;
        p[2] = '0' + (x % 10); x /= 10;
        p[1] = '0' + (x % 10); x /= 10;
        p[0] = '0' + x;
        p += 4;
    }
}

// ============= 原方案 (add.cpp 1597-1651 的 AVX2 循环体) =============
void pack_digits_orig(const uint16_t* data, size_t n, char* p) {
    size_t i = n;
    while (i >= 8) {
        __m128i v16 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&data[i - 8]));
        v16 = _mm_shuffle_epi8(v16, _mm_setr_epi8(14, 15, 12, 13, 10, 11, 8, 9,
                                                   6, 7, 4, 5, 2, 3, 0, 1));
        __m256i v = _mm256_cvtepu16_epi32(v16);
        __m256i hi = _mm256_srli_epi32(_mm256_mullo_epi32(v, _mm256_set1_epi32(0x28F5D)), 24);
        __m256i lo = _mm256_sub_epi32(v, _mm256_mullo_epi32(hi, _mm256_set1_epi32(100)));
        __m256i thousands = _mm256_srli_epi32(_mm256_mullo_epi32(hi, _mm256_set1_epi32(0x199A)), 16);
        __m256i hundreds = _mm256_sub_epi32(hi, _mm256_mullo_epi32(thousands, _mm256_set1_epi32(10)));
        __m256i tens = _mm256_srli_epi32(_mm256_mullo_epi32(lo, _mm256_set1_epi32(0x199A)), 16);
        __m256i ones = _mm256_sub_epi32(lo, _mm256_mullo_epi32(tens, _mm256_set1_epi32(10)));
        __m256i ascii0 = _mm256_set1_epi32('0');
        thousands = _mm256_add_epi32(thousands, ascii0);
        hundreds = _mm256_add_epi32(hundreds, ascii0);
        tens = _mm256_add_epi32(tens, ascii0);
        ones = _mm256_add_epi32(ones, ascii0);
        __m256i packed_th_hi = _mm256_packus_epi32(thousands, hundreds);
        __m256i packed_tens_ones = _mm256_packus_epi32(tens, ones);
        __m256i packed = _mm256_packus_epi16(packed_th_hi, packed_tens_ones);
        __m128i shuffle_mask = _mm_setr_epi8(0, 4, 8, 12, 1, 5, 9, 13,
                                              2, 6, 10, 14, 3, 7, 11, 15);
        __m128i lo128 = _mm256_castsi256_si128(packed);
        __m128i hi128 = _mm256_extracti128_si256(packed, 1);
        lo128 = _mm_shuffle_epi8(lo128, shuffle_mask);
        hi128 = _mm_shuffle_epi8(hi128, shuffle_mask);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(p), lo128);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(p + 16), hi128);
        p += 32; i -= 8;
    }
    while (i--) {
        uint16_t x = data[i];
        p[3] = '0' + (x % 10); x /= 10;
        p[2] = '0' + (x % 10); x /= 10;
        p[1] = '0' + (x % 10); x /= 10;
        p[0] = '0' + x;
        p += 4;
    }
}

// ============= 标量参考 =============
void pack_digits_scalar(const uint16_t* data, size_t n, char* p) {
    for (size_t i = n; i--; ) {
        uint16_t x = data[i];
        p[3] = '0' + (x % 10); x /= 10;
        p[2] = '0' + (x % 10); x /= 10;
        p[1] = '0' + (x % 10); x /= 10;
        p[0] = '0' + x;
        p += 4;
    }
}

int main() {
    // 测试数据: 各种 n 值
    size_t test_sizes[] = {1, 2, 7, 8, 9, 15, 16, 17, 23, 24, 25, 31, 32, 33,
                           100, 500, 1000, 5000, 50000, 500000};
    int n_tests = sizeof(test_sizes) / sizeof(test_sizes[0]);

    // 正确性验证
    printf("=== 正确性验证 ===\n");
    int pass = 0, fail = 0;
    for (int t = 0; t < n_tests; t++) {
        size_t n = test_sizes[t];
        std::vector<uint16_t> data(n);
        // 随机填充 0-9999
        for (size_t i = 0; i < n; i++) data[i] = rand() % 10000;

        std::vector<char> out_scalar(n * 4 + 64);
        std::vector<char> out_new(n * 4 + 64);
        std::vector<char> out_orig(n * 4 + 64);

        pack_digits_scalar(data.data(), n, out_scalar.data());
        pack_digits_avx2_final(data.data(), n, out_new.data());
        pack_digits_orig(data.data(), n, out_orig.data());

        bool ok_new = memcmp(out_scalar.data(), out_new.data(), n * 4) == 0;
        bool ok_orig = memcmp(out_scalar.data(), out_orig.data(), n * 4) == 0;

        if (ok_new) pass++; else {
            fail++;
            printf("  FAIL [new] n=%zu\n", n);
            // 显示前几个差异
            for (size_t i = 0; i < n * 4 && i < 64; i++) {
                if (out_scalar[i] != out_new[i]) {
                    printf("    byte %zu: scalar='%c' new='%c'\n", i, out_scalar[i], out_new[i]);
                    break;
                }
            }
        }
        if (!ok_orig) printf("  WARN [orig] n=%zu also differs!\n", n);
    }
    printf("结果: %d PASS, %d FAIL\n\n", pass, fail);

    // 性能测试
    printf("=== 性能测试 (中位数, 15 runs × 10 loops) ===\n");
    printf("%-10s %12s %12s %12s %8s\n", "n", "scalar_ms", "orig_ms", "new_ms", "new/orig");
    printf("%-10s %12s %12s %12s %8s\n", "----", "---------", "-------", "------", "--------");

    for (int t = 0; t < n_tests; t++) {
        size_t n = test_sizes[t];
        if (n < 100) continue;  // 太小的跳过

        std::vector<uint16_t> data(n);
        for (size_t i = 0; i < n; i++) data[i] = rand() % 10000;
        std::vector<char> out(n * 4 + 64);

        int RUNS = 15, LOOPS = 10;
        double sc_vals[15], or_vals[15], nw_vals[15];

        // 热身
        for (int i = 0; i < 3; i++) {
            pack_digits_scalar(data.data(), n, out.data());
            pack_digits_orig(data.data(), n, out.data());
            pack_digits_avx2_final(data.data(), n, out.data());
        }

        for (int r = 0; r < RUNS; r++) {
            auto s = std::chrono::high_resolution_clock::now();
            for (int l = 0; l < LOOPS; l++) pack_digits_scalar(data.data(), n, out.data());
            auto e = std::chrono::high_resolution_clock::now();
            sc_vals[r] = std::chrono::duration<double, std::milli>(e - s).count() / LOOPS;

            s = std::chrono::high_resolution_clock::now();
            for (int l = 0; l < LOOPS; l++) pack_digits_orig(data.data(), n, out.data());
            e = std::chrono::high_resolution_clock::now();
            or_vals[r] = std::chrono::duration<double, std::milli>(e - s).count() / LOOPS;

            s = std::chrono::high_resolution_clock::now();
            for (int l = 0; l < LOOPS; l++) pack_digits_avx2_final(data.data(), n, out.data());
            e = std::chrono::high_resolution_clock::now();
            nw_vals[r] = std::chrono::duration<double, std::milli>(e - s).count() / LOOPS;
        }

        std::sort(sc_vals, sc_vals + RUNS);
        std::sort(or_vals, or_vals + RUNS);
        std::sort(nw_vals, nw_vals + RUNS);
        int mid = RUNS / 2;
        double ratio = nw_vals[mid] / or_vals[mid];

        printf("%-10zu %12.4f %12.4f %12.4f %8.4f\n", n, sc_vals[mid], or_vals[mid], nw_vals[mid], ratio);
    }

    printf("\n=== 完成 ===\n");
    return 0;
}
