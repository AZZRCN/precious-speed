# div_1st 除法优化任务书

## 任务目标

仅优化大整数**除法**（div），冲击 div 性能第一。

工作流程：
1. 阅读 `div_1st.cpp`，理解其除法实现
2. 在其基础上进行除法优化
3. 本地速度达到最快后，将除法成果融合到 `masonxiong_opt.cpp`（加法/乘法第一），形成全能版本

**仅专注除法优化，不要动加法/乘法/平方等其他操作。**

## 环境信息

- CPU：11th Gen Intel(R) Core(TM) i7-11370H @ 3.30GHz
- OS：Windows
- 编译器：g++ 15.2.0 (MinGW-W64 x86_64-win32-seh-rev0)
- 编译选项：`-O3 -mavx2 -mfma -funroll-loops`
- CPU 指令集：AVX2 / FMA / AVX-512F/DQ/BW/VL
  - 注：Tiger Lake AVX-512 仅 1 FMA 端口，与 AVX2 持平

## 文件清单

| 文件 | 作用 |
|------|------|
| `div_1st.cpp` | **优化起点**。第一名除法实现，全新干净的副本。阅读并在此基础上优化 |
| `masonxiong_opt.cpp` | 加法/乘法第一的实现，含 AVX2 优化、thread_local 缓冲区、内存池等成熟技术，可作为优化思路参考。最终融合目标 |
| `masonxiong.cpp` | masonxiong_opt 的原始版本，基准对照 |
| `benchmark_report.md` | 已有的性能对比报告，含 GMP/Boost 的对比数据 |
| GMP | 6.3.0，行业标准 C 大数库，Schönhage-Strassen FFT |
| Boost | 1.89.0，`boost::multiprecision::cpp_int` |

## 参考实现

优化过程中，应在以下实现之间进行速度对比：

1. **div_1st.cpp** — 优化起点（第一名除法）
2. **masonxiong_opt.cpp** — 加法/乘法第一
3. **masonxiong.cpp** — masonxiong_opt 原始版
4. **GMP** — 行业标准
5. **Boost** — 通用大数库

## 验证方法

需要自行设计验证流程，要求：

- **正确性验证**：C++ 验证器程序，对拍检查 `q*B + r == A` 且 `0 <= r < B`
- **性能测量**：使用微秒级计时器（如 `std::chrono::high_resolution_clock`）
- **可复现**：固定随机 seed，确保每次测试数据一致
- **多次运行**：可选运行次数，取总和或平均，减少噪声
- **规模可选**：支持不同规模的测试用例（小/中/大/极端 unbalanced 等）
