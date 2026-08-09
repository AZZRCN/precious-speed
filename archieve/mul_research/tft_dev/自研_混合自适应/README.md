# 自研算法方向A：混合自适应大数乘法

## 状态：无收益（原型实现无法与生产级代码竞争）

## 设计思路

### 核心创新
- **运行时 benchmark 动态测定阈值**：不同于 GMP 的静态阈值，程序启动时做微型 benchmark 测定当前硬件的最优切换点
- **三档算法选择**：basicMul (O(n²)) → Karatsuba (O(n^1.585)) → FFT (O(n log n))

### 算法选择策略
```
N < T1:  basicMul  (O(n²) 但常数最小)
T1 ≤ N < T2: Karatsuba (O(n^1.585))
N ≥ T2:  FFT       (O(n log n))
```
T1 和 T2 在程序启动时通过微型 benchmark 测定，适应不同硬件。

## 实测结果

### Calibrate 阈值测定
- T1=16 (basicMul → Karatsuba)：所有测试点 Karatsuba 均比 basic 慢 (ratio 2.5-10x)
- T2=64 (Karatsuba → FFT)：FFT 在 N=64 时就比 Karatsuba 快

### Benchmark 关键数据
| N | basic_ms | kara_ms | fft_ms | hybrid | 问题 |
|---|----------|---------|--------|--------|------|
| 8 | 0.0001 | 0.0001 | 0.0005 | 0.0001 [basic] | OK |
| 32 | 0.0004 | 0.0023 | 0.0024 | 0.0023 [kara] | basic 更快! |
| 64 | 0.0008 | 0.0080 | 0.0044 | 0.0040 [fft] | basic 更快! |
| 128 | 0.0021 | 0.0273 | 0.0088 | 0.0089 [fft] | basic 更快! |
| 256 | 0.0078 | 0.0891 | 0.0205 | 0.0178 [fft] | basic 更快! |
| 512 | 0.0264 | 0.2573 | 0.0393 | 0.0376 [fft] | basic 更快! |
| 1024 | 0.0969 | 0.7905 | 0.1067 | 0.1047 [fft] | basic 更快! |
| 2048 | 0.4171 | 2.6298 | 0.2207 | 0.2584 [fft] | FFT 首次优于 basic |
| 4096 | 2.8852 | 11.3949 | 0.4743 | 0.5095 [fft] | FFT 优势明显 |

## 失败原因

### 1. Karatsuba 实现极慢 (比 basic 慢 6-10x)
- **根因**：使用 `std::vector` 动态分配内存 + 递归调用开销
- 每次 Karatsuba 递归都创建多个 vector (z0, z1, z2, a_sum, b_sum)
- 内存分配/释放开销远超算法节省的乘法次数
- 对比：GMP 使用预分配栈内存 + 迭代实现，才能在中等规模胜出

### 2. 原型 FFT 常数过大
- 使用 `std::complex<double>` 和递归蝶形，无 SIMD 优化
- basic_mul 在 N≤1024 都比 FFT 快
- 对比：mul.cpp 的 FFT 使用 AVX2 向量化 + 迭代式 + RIRI 打包

### 3. 阈值测定逻辑缺陷
- T1 测定：所有测试点 kara 都比 basic 慢，T1 被设为最小值 16
- 实际应跳过 Karatsuba，直接 basic → FFT

## 结论

混合自适应算法的**思路本身有价值**（运行时 benchmark 适应硬件），但：
1. **原型实现无法与生产级代码竞争**：vector+递归 vs SIMD+迭代
2. **在 LC 场景不适用**：LC 测试点都是大规模 (max_max 用 float_len=1048576)，远超 Karatsuba 适用范围
3. **GMP 已实现类似机制**：静态阈值 + 多级切换，且实现高度优化

### 对 CUR 的可借鉴性
- **无直接收益**：CUR 的 FFT 已高度优化，basic_mul 也用了打包优化
- **思路参考**：运行时 benchmark 思想可用于 CUR 的阈值调优，但需在 mul.cpp 框架内实现

## 代码位置
- [proto.cpp](file:///d:/precious_speed/tft_dev/自研_混合自适应/proto.cpp) 自包含原型
- [build.bat](file:///d:/precious_speed/tft_dev/自研_混合自适应/build.bat) 一键编译
- [run.bat](file:///d:/precious_speed/tft_dev/自研_混合自适应/run.bat) 一键运行
- [bench_result.txt](file:///d:/precious_speed/tft_dev/自研_混合自适应/bench_result.txt) 实测原始数据

## Bug 修复记录
- **a_sum/b_sum 进位传播 bug**：当 a_hi_len < a_lo_len 且循环结束后 carry>0 时，carry 直接 push_back 到末尾，跳过了 a_sum[a_hi_len..a_lo_len-1] 的中间位置。修复为继续从 a_hi_len 位置传播 carry。
