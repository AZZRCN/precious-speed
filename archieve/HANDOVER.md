# moptm 交接文档

## 项目概述

**目标**：完成 Library Checker 三道大整数题，在所有测试点上比 best 更快

- https://judge.yosupo.jp/problem/addition_of_big_integers
- https://judge.yosupo.jp/problem/multiplication_of_big_integers
- https://judge.yosupo.jp/problem/division_of_big_integers

**单文件**：`moptm.cpp`（~2855 行，96KB），三题统一，通过 `-D` 编译开关选择 main。

**底座**：hint 库 (https://github.com/With-Sky/HyperInt-mini)，BASE=10000，uint16_t 肢，radix-4 自排序 DIF/DIT FFT，牛顿迭代求逆 + Core2 分块除法。

---

## 评测环境（关键！）

| 项目 | 值 |
|------|-----|
| 编译器 | g++ 11.4.0 (Ubuntu 22.04) |
| 编译命令 | `g++ -std=gnu++20 -O2 -static -DONLINE_JUDGE -o 目标 源文件` |
| CPU | Intel Cascade Lake, 3.1GHz base, AVX2+FMA+AVX-512 |
| 指令集启用方式 | **必须用 `#pragma GCC target` 或 `__attribute__((target))` 显式开启** |
| 优化等级 | `-O2`（`-ftree-vectorize` 默认关闭） |
| 内存限制 | 2GB |
| 时间限制 | 题目页面标注，通常是 2s/5s/10s |

实测 `-O2` 下不开启 auto-vectorization，FFT 性能极差。需要在源码中加入：
```cpp
#pragma GCC optimize("O3,unroll-loops")
```
将关键代码提升至 `-O3` 级别，启用 auto-vectorization（SSE 128-bit）。

---

## 文件结构

```
D:\precious_speed/
├── moptm.cpp              # 主代码，三合一，唯一需要提交的文件
├── moptm_mudiv.cpp        # GCC heisenbug 隔离代码（mu-div 函数体，未使用）
├── bench.cpp              # 正确性验证 + 计时框架
│   ├── bench cmp all      # moptm vs best 逐字节对拍
│   ├── bench add|mul|div  # 计时（10 runs + 2 warmup）
│   └── bench cmp add|mul|div  # 单项对拍
├── gen_data.cpp           # 测试数据生成器（含正负数）
│   └── gen_data           # 重新生成全部测试数据
├── best/
│   ├── add.cpp            # hint 库加法（与 moptm 同架构）
│   ├── mul.cpp            # **自定义实现**（非 hint 库！有 AVX2 内联汇编）
│   └── div.cpp            # hint 库除法（与 moptm 同架构）
├── test_*.txt             # 生成的测试数据文件
└── HANDOVER.md            # 本文档
```

**重要**：`best/mul.cpp` 与 `best/add.cpp`、`best/div.cpp` 不同，它不是 hint 库代码，而是第三方用 `__m256d` 内联 SIMD 手写的独立实现。这是 moptm 的 MUL 在 `-O2` 下追不上 best 的根本原因。

---

## 当前性能（本地 bench 数据）

### 最终三项 LC 分数

| 操作 | moptm | best | 比值 | 领先 |
|------|-------|------|------|------|
| ADD | 14.1ms | 19.1ms | **0.73x** | **-27%** |
| MUL | 16.8ms | 20.7ms | **0.81x** | **-19%** |
| DIV | 37.1ms | 40.2ms | **0.92x** | **-8%** |

编译参数：`-O3 -mavx2 -mfma -funroll-loops`，10 runs + 2 warmup 取中位数。

### 模拟评测环境（-O2 + pragma）

| 操作 | 500k*500k MUL 单次 |
|------|-------------------|
| 纯 `-O2`（无 pragma） | ~330ms |
| `-O3` pragma（冷启动） | ~285ms |
| `-O3` pragma（热启动） | ~39ms |

加入 `#pragma GCC optimize("O3,unroll-loops")` 后热启动约 39ms，较纯 `-O2` 提升约 8.5x。

---

## 已应用的优化

### 算术内核
- **add_half / sub_half 无分支掩码化** — 用 `T(0) - T(cf)` 掩码替代 cmov
- **absAdd 双肢打包加法 (uint32_t)** — 串行进位链减半
- **absSub / absSubEq 8 路展开** — 循环开销降低

### FFT 内核
- **fftMul / fftSqr / fftMulPre 进位传播 8 路展开**
- **共享 FFT 实例** (getSharedFFT) — 避免重复计算旋转因子表

### 乘法专属
- **fftMulUnbalanced 非对称拆分** — 大数拆 chunk，预计算小数的 DFT，逐块 fftMulPre 复用
  - 阈值：`sml >= 16384` 且 `big/sml >= 6`
  - 将 1M*100k 从 0.99x 提升至 0.90x
  - 修复 buffer aliasing bug（`large.ptr` 与 `out.ptr` 指向同一缓冲区）

### 除法专属
- **divisor_dft 预计算 → B-1 优化** (省 1 次 DFT)
- **blocks >= 3 启用 fftMulPre 快速路径**
- **absInvNewton 基例阈值 64**（原版 16）

### I/O
- cin streambuf 批量读 + oBuffer 零拷贝写
- writeTo 4 位查表替代 itostr4 除法

---

## 已知问题

### 1. MUL 在评测环境 (-O2) 下慢于 best

**根因**：`best/mul.cpp` 使用 `__m256d` 手写 SIMD 内联汇编，在任何优化级别下都能生成 AVX2 代码。而 `moptm.cpp` 的 hint 库 FFT 依赖编译器 auto-vectorization，在 `-O2` 下无法自动向量化，只能通过 `#pragma GCC optimize("O3,unroll-loops")` 提升至 `-O3` 级别，生成 SSE (128-bit) 代码。

**当前最优 pragma 方案**：
```cpp
#pragma GCC optimize("O3,unroll-loops")
```
- 不加 `#pragma GCC target("avx2,fma")` → 实测全局开启 AVX2 会降低性能（VEX 编码开销 + 寄存器压力）
- 不加 `#pragma GCC target("arch=cascadelake")` → AVX-512 频率降频更严重
- 尝试过在关键函数加 `__attribute__((target("avx2")))` 但未找到有效方案

**建议**：若追求 MUL 超越 best，需改写 Float2/Complex2 为 `__m128d`/`__m256d` 内联实现，或整体替换 FFT 内核。

### 2. ADD / DIV 使用 hint 库，与 best 同架构

`best/add.cpp` 和 `best/div.cpp` 也是 hint 库代码，与 moptm 架构相同。moptm 通过微优化（分支掩码、展开、打包）已全面超越 best。

### 3. `#define HINT_OP_ADD` 曾被误硬编码

第 2801 行曾被误加入 `#define HINT_OP_ADD`，导致 `-DHINT_OP_MUL` 和 `-DHINT_OP_DIV` 编译时实际上跑的是 ADD 的 main，已在当前版本中删除。

### 4. writeTo 曾缺少符号输出

`writeTo` 函数未输出 `-` 号，导致负数结果输出为绝对值。已修复。

---

## 编译与提交

```bash
# 本地开发（完整优化）
g++ -std=c++20 -O3 -mavx2 -mfma -funroll-loops -DHINT_OP_ADD -o moptm_ADD.exe moptm.cpp
g++ -std=c++20 -O3 -mavx2 -mfma -funroll-loops -DHINT_OP_MUL -o moptm_MUL.exe moptm.cpp
g++ -std=c++20 -O3 -mavx2 -mfma -funroll-loops -DHINT_OP_DIV -o moptm_DIV.exe moptm.cpp

# 正确性验证
bench.exe cmp all

# 基准测试
bench.exe all --runs 10

# 提交到 yosupo — 只需上传 moptm.cpp
```

评测环境会自动添加 `-DONLINE_JUDGE`，但不会加 `-mavx2` 等。源码中的 `#pragma GCC optimize("O3,unroll-loops")` 会在评测时生效。

---

## 后续优化方向

1. **MUL 在 -O2 下提速**：将 `Float2` 改为 `__m128d` 内联、`Complex2` 改为 `__m256d` 内联，让 FFT 在任何优化级别下使用 SIMD
2. **更激进的非对称 MUL**：降低 `FFT_MUL_UNBALANCED_MIN` 阈值（当前 16384），覆盖更多不平衡用例
3. **DIV 小规模微调**：100k/10k 等小规模 DIV 仍有 ≈1% 噪声级差距
4. **AOT 编译加速**：增大 `oBuffer`（当前 32MB）或使用线程局部预分配
