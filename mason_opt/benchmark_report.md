# 大整数运算库性能对比报告

**测试日期**：2026-07-15
**测试环境**：

- CPU：11th Gen Intel(R) Core(TM) i7-11370H @ 3.30GHz
- OS：Windows
- 编译器：g++ 15.2.0 (MinGW-W64 x86_64-win32-seh-rev0)
- 编译选项：`-O3 -mavx2 -mfma -funroll-loops`
- CPU 指令集：AVX2 / FMA / AVX-512F/DQ/BW/VL（注：Tiger Lake AVX-512 仅 1 FMA 端口，与 AVX2 持平）

## 参赛库

| 库 | 版本 | 说明 |
|----|------|------|
| **OPT** | masonxiong_opt.cpp (HEAD b07a039) | 本项目优化版，复数 FFT + 融合 Newton 除法 |
| **ORIG** | masonxiong.cpp | 原始版本，基准对照 |
| **GMP** | 6.3.0 (msys2 mingw64 预编译) | 行业标准 C 大数库，Schönhage-Strassen FFT |
| **Boost** | 1.89.0 (E:\boost) | `boost::multiprecision::cpp_int`，header-only |

## 测试方法

- **统一测试框架** `bench_all.cpp`：相同随机数生成（seed=42）、相同计时逻辑（`chrono::high_resolution_clock`）、相同操作语义
- **数据规模**：10K / 100K / 1M 位十进制数字
- **操作**：`mul`（a×a 乘法）、`sqr`（a² 平方）、`div2`（a²/a Newton 除法）
- **轮数**：多数测试 5 轮取总和；1M div2 因 Boost 极慢，ORIG/OPT/GMP 取 3 轮，Boost 取 1 轮
- **单位**：毫秒 (ms)，越小越快

## 性能数据

### 10K 规模（5 轮总和，ms）

| 操作 | ORIG | OPT | GMP | Boost |
|------|------|-----|-----|-------|
| mul  | 0.634 | **0.313** | 0.304 | 0.629 |
| sqr  | 0.494 | **0.310** | 0.487 | 0.595 |
| div2 | 3.223 | 1.852 | **0.663** | 3.492 |

### 100K 规模（5 轮总和，ms）

| 操作 | ORIG | OPT | GMP | Boost |
|------|------|-----|-----|-------|
| mul  | 4.951 | **2.836** | 6.501 | 29.404 |
| sqr  | 4.339 | **2.572** | 5.844 | 29.456 |
| div2 | 32.809 | 15.361 | **17.951** | 401.626 |

### 1M 规模

| 操作 | ORIG | OPT | GMP | Boost |
|------|------|-----|-----|-------|
| mul (5轮) | 53.862 | **27.685** | 86.693 | 1768.42 |
| sqr (5轮) | 54.338 | **27.214** | 86.498 | 1800.11 |
| div2 (3轮) | 250.354 | **115.804** | 167.758 | 9174.88 (1轮) |

## 相对性能（以 OPT 为基准 = 1.00x）

### 1M 规模单轮均值对比

| 操作 | ORIG | OPT | GMP | Boost |
|------|------|-----|-----|-------|
| mul  | 2.08x 慢 | **1.00x** | 3.13x 慢 | 63.9x 慢 |
| sqr  | 2.00x 慢 | **1.00x** | 3.18x 慢 | 66.1x 慢 |
| div2 | 2.16x 慢 | **1.00x** | 1.45x 慢 | 79.2x 慢 |

### OPT vs ORIG 优化效果（1M 规模）

| 操作 | ORIG (ms) | OPT (ms) | 加速比 |
|------|-----------|----------|--------|
| mul  | 10.77 | 5.54 | **1.94x** |
| sqr  | 10.87 | 5.44 | **2.00x** |
| div2 | 83.45 | 38.60 | **2.16x** |

## 关键发现

1. **OPT 全面领先**：在 100K-1M 规模的所有操作中，OPT 均为最快，无一例外
2. **乘法/平方优势显著**：1M 规模 OPT 比 GMP 快 3.1-3.2x，比 Boost 快 64-66x
3. **除法优势较小**：1M div2 OPT 比 GMP 仅快 1.45x；10K 规模 GMP 反超（GMP 小规模除法有优化）
4. **Boost 表现最差**：1M 规模所有操作均比 OPT 慢 60-80 倍，`cpp_int` 的通用设计不适合大规模数值计算
5. **ORIG 优化效果**：OPT 比 ORIG 在所有操作稳定快约 2x，验证了优化有效性

## OPT 优化技术概览

| 优化 | 影响操作 | 说明 |
|------|----------|------|
| AVX2 蝶形（`__m256d` 一次 2 复数） | mul/sqr | FFT 指令数减半 |
| 平方专用路径 + 点乘优化 | sqr | 省一次 DIF + 一半点乘 |
| 不平衡乘法拆分 + DIF 缓存 | 2n×n 乘法 | FFT 长度 4n→2n，cache 友好 |
| 融合 Newton 迭代（shift+double+subtract 单次） | div2 | 减少中间分配 |
| 截断 dividend 低位（shiftBack==0 时） | div2 | 减少乘法规模 |
| BruteforceThreshold = 96 | 小规模 | FFT/BF 交叉点调优 |
| 内存池（size-class buckets） | 全部 | 避免重复 malloc/free |

## 测试文件

- 统一测试框架：`bench_all.cpp`
- 编译产物：`bench_orig.exe` / `bench_opt.exe` / `bench_gmp.exe` / `bench_boost2.exe`
- 正确性测试：`comprehensive_test.cpp`（全部通过，含 1M 规模）
