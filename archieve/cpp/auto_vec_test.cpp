// auto_vec_test.cpp  v2
// 目的：验证 LC -O2 是否启用 auto-vectorization
//
// 修复 v1 的问题：浮点 reduction 不会被 -O3 向量化（FP 加法不满足结合律）
// v2 改用整数 element-wise 循环（每次迭代独立，-O3 一定向量化）
//
// 提交方法：
//   1. 直接提交此文件到 LC（如 addition_of_big_integers）
//      → 记录执行时间。如果 TLE，说明 -O2 标量太慢
//   2. 取消注释第 18 行 #pragma 后重新提交
//      → 记录执行时间。如果比第一次快很多（5-10x），说明 -O3 auto-vec 生效
//
// 理论耗时（1M int element-wise × 2000 次 = 2×10^9 操作）：
//   -O2 标量：~1-2s（可能 TLE 也可能 WA）
//   -O3 AVX2：~0.1-0.3s（8-wide SIMD，一定不 TLE）
//
// 注意：禁止在本地运行（-O2 下可能阻塞 1-2 秒）

#pragma GCC optimize("O3,unroll-loops")  // 取消注释测试 -O3 路径

#include <cstdio>

int main() {
    // 快速读入所有输入（格式不重要，只需读完避免 RE）
    char buf[1 << 20];
    while (fread(buf, 1, sizeof(buf), stdin) > 0) {}

    // 整数 element-wise 循环：a[i] = a[i] * 3 + b[i]
    // 每次迭代独立（无 reduction），整数运算可结合
    // -O2：标量执行（-ftree-vectorize 关闭）
    // -O3：SIMD 执行（-ftree-vectorize 开启，8-wide AVX2）
    static int a[1000000], b[1000000];
    for (int i = 0; i < 1000000; ++i) {
        a[i] = i;
        b[i] = i * 2;
    }

    for (int iter = 0; iter < 2000; ++iter) {
        for (int i = 0; i < 1000000; ++i) {
            a[i] = a[i] * 3 + b[i];
        }
    }

    // volatile 防止编译器删除整个循环
    volatile int sink = a[0] + a[999999];
    printf("%d\n", (int)sink);
    return 0;
}


//with pragma
/*
	Name	Status	Time	Memory
	example_00	WA	1143 ms	8.26 Mib
	example_01	WA	1139 ms	8.25 Mib
	random_00	WA	1141 ms	8.27 Mib
	random_01	WA	1139 ms	8.29 Mib
	random_02	WA	1140 ms	8.26 Mib
	random_03	WA	1141 ms	8.25 Mib
	random_04	WA	1139 ms	8.43 Mib
	random_05	WA	1140 ms	8.29 Mib
	random_06	WA	1139 ms	8.40 Mib
	random_07	WA	1142 ms	8.29 Mib
	random_08	WA	1140 ms	8.28 Mib
	random_09	WA	1139 ms	8.30 Mib
*/
//without pragma
/*
	Name	Status	Time	Memory
	example_00	WA	189 ms	8.29 Mib
	example_01	WA	189 ms	8.29 Mib
	random_00	WA	188 ms	8.30 Mib
	random_01	WA	188 ms	8.30 Mib
	random_02	WA	188 ms	8.28 Mib
	random_03	WA	200 ms	8.25 Mib
	random_04	WA	188 ms	8.26 Mib
	random_05	WA	201 ms	8.25 Mib
	random_06	WA	188 ms	8.29 Mib
	random_07	WA	189 ms	8.26 Mib
	random_08	WA	189 ms	8.34 Mib
	random_09	WA	188 ms	8.29 Mib
*/