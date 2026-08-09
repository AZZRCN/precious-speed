# O3 开发周期交接文档

> **更新时间**: 2026-07-27
> **当前状态**: 所有已识别优化方向已穷尽，NEW(O3) +22% 打过 BEST(O2)
> **下一步**: 等待用户决定是否进入 O3 预展开阶段或探索新方向

---

## 1. 项目概述

在 LC (Library Checker, judge.yosupo.jp) 真实评测约束下，让 `add.cpp`/`mul.cpp`/`div.cpp` 在 ADD/MUL/DIV 三题上保持或超越 `best/` 快照。

- **O2 周期**: 已完成，三题全部 #1 (ADD 16ms / MUL 36ms / DIV 87ms)
- **O3 周期**: 当前阶段，NEW(O3) 已 +22% 打过 BEST(O2)
- **最终目标**: 通过 O3 预展开工具，将 O3 优化效果以 O2 编译方式产出 cpp 文件

## 2. O3 周期成果汇总

### 2.1 有效优化（保留）

| Phase | 优化内容 | DIV 收益 | MUL 收益 | 代码位置 |
|---|---|---|---|---|
| 9/11 | 叶子阈值 8→16 | +2.9% | +5.2% | div.cpp difSmall/iditSmall |
| 12 | Huge Page (MADV_HUGEPAGE) | +6.67% | +13.15% | mul.cpp/div.cpp 内存分配 |
| 13 | 迭代 FFT (MUL) | — | +4.2% | mul.cpp difIter/iditIter |
| 17 | 迭代 FFT (DIV) | +1.5% | — | div.cpp difIter/iditIter |

**累计收益**: DIV ~11%, MUL ~22% (含 O3 编译器自动优化)

### 2.2 无效/放弃的优化（代码保留但禁用）

| Phase | 优化内容 | 结果 | 宏控制 | 失败原因 |
|---|---|---|---|---|
| 10/16 | 叶子阈值 16→32 | 慢 7.7% | — | register spill |
| 14 | 手工 FMA | 噪音内 | — | 迭代 FFT 下 FMA 无法调度 |
| 19-20 | 四步法 FFT Bailey 4-step | 慢 80% | HINT_USE_4STEP_FFT=0 | 转置 + twiddle 开销过大 |
| 21 | Barrett carry chain | ratio=1.0035 | HINT_BARRETT_UNBALANCED=0 | chunk~1000 Barrett 收益不抵开销 |
| 22 | radix-8 蝶形 | 放弃 | — | RRRI 布局下 register spill 不可避免 |
| 23 | carry chain SIMD 并行化 | 放弃 | — | 串行依赖是数学本质 |
| 25 | RIRI 临时 permute | 慢 40% | HINT_USE_RIRI_PERMUTE=0 | permute 税过重 (port 5 占 6c) |
| 26 | cache miss 替代方案 | 放弃 | — | L1 miss 是容量问题, 四步法已失败 |
| 27 | DIV absInvNewtonGMP 内联 | 放弃 | — | 内联收益 <0.01%, 两次 FFT 串行依赖 |

### 2.3 参考资料研究结论（Phase 24）

- **FFTW codelet**: RIRI 交错布局 + `_mm256_fmaddsub_pd` 优势明显，但需要全局布局重构
- **FFTW buffered/transpose**: large_small_00 cache miss 是容量问题，5-10% 是上限
- **四步法 FFT**: 彻底无望（转置 + twiddle 开销无法消除）

## 3. 文件结构

```
d:\precious_speed\
├── add.cpp            # O3 周期 ADD (当前 #1, 16ms)
├── mul.cpp            # O3 周期 MUL (当前 #1 tied, 36ms)
├── div.cpp            # O3 周期 DIV (当前 #1, 87ms)
├── best/              # O2 对比基准 (add 16ms / mul 36ms / div 87ms)
├── HANDBOOK.MD        # 主记录文件 (Phase 1b-27)
├── O3_HANDOVER.md     # 本文件
├── ai.bat             # 命令集中文件
├── docs/              # limits.md + O3_DEV_LOG.md + GMP_DEV.md
├── asm/               # 汇编对比
├── gmp_index/         # GMP 源码阅读索引
├── archieve/          # O2 周期归档
├── toolbox/           # tbx 工具箱
├── scripts/           # ssh_exec.py + bench_*.sh
└── .trae/specs/o3-perf-roadmap/  # O3 周期 spec
    ├── spec.md        # 长技术路线
    ├── tasks.md       # 任务清单
    └── checklist.md   # 检查清单
```

## 4. 关键技术状态

### 4.1 编译选项
- **O3 开发周期**: `g++ -O3 -std=c++23 -march=native` (VM 本地)
- **BEST 对比基准**: `g++ -O2 -std=c++23 -march=native` (与 LC 平台一致)
- **LC 平台**: `g++ -O2 -std=c++23 -DEVAL -DONLINE_JUDGE -march=native` (只支持 O2)

### 4.2 测试环境
- **VM**: 10.144.33.157 (2026-07-27 重启后新 IP)
- **系统**: Ubuntu 7.0, Intel i7-11370H Tiger Lake (AVX2+FMA+AVX512)
- **GCC**: 15.2.0
- **perf**: vPMU 已启用，可采集硬件事件
- **测试方法**: 15 runs × 5 loops 交替运行取 median ratio

### 4.3 代码中的实验宏（默认全 0，代码保留）

| 宏名 | 默认值 | 位置 | 说明 |
|---|---|---|---|
| HINT_USE_4STEP_FFT | 0 | mul.cpp | 四步法 FFT (Phase 20 失败) |
| HINT_BARRETT_UNBALANCED | 0 | mul.cpp | Barrett carry chain (Phase 21 无效) |
| HINT_DIVBASE_UNBALANCED_SIMPLE | 0 | mul.cpp | divBASE 简单循环 |
| HINT_USE_RIRI_PERMUTE | 0 | mul.cpp | RIRI 临时 permute (Phase 25 慢 40%) |

### 4.4 FFT 架构
- **算法**: split-radix DIF/IDIT，radix-4 蝶形
- **实现**: 迭代 FFT (difIter/iditIter，单栈结构)
- **布局**: RRRI 拆分 (1 YMM = 1 复数)
- **叶子阈值**: 16 (Phase 9/11 验证最优)
- **旋转因子**: AlignedVec32 32字节对齐 + assume_aligned
- **缓冲区**: thread_local + MADV_HUGEPAGE (2MB 大页)

### 4.5 DIV 架构
- **算法**: GMP 风格 mu_div_qr 近似逆分块除法
- **关键函数**: absInvNewtonGMP (35.1%), absDivMu blocks loop (47.0%)
- **cyclic 卷积**: mod B^m-1 加速 (CYCLIC_MIN_K=4096)
- **纯除法性能**: 比 GMP 快 9% (Phase 7)

## 5. 性能瓶颈分析

### 5.1 当前瓶颈（已穷尽）

| 瓶颈 | 占比 | 优化状态 | 原因 |
|---|---|---|---|
| large_small_00 L1 cache miss | 65.1% cycles | ❌ 放弃 | 容量问题 (512KB >> 48KB), 四步法已失败 |
| DIV absInvNewtonGMP | 35.1% | ❌ 放弃 | 算法本质, 内联无收益 |
| DIV blocks loop | 47.0% | ❌ 放弃 | 两次 FFT 串行依赖 |
| MUL carry chain | 9-11% | ❌ 放弃 | 串行依赖无法并行化 |
| FFT 复数乘法 | — | ❌ 放弃 | RRRI 已最优, RIRI permute 慢 40% |

### 5.2 未探索的方向

| 方向 | 预期收益 | 风险 | 工作量 | 建议 |
|---|---|---|---|---|
| NTT 替代浮点 FFT | 未知 | 极高 | 极大 | 不推荐（通常更慢） |
| Schönhage-Strassen | 未知 | 极高 | 极大 | 需研究 GMP mul_fft.c |
| Vc SIMD 抽象 | 低 | 中 | 中 | 低优先级 |
| 其他测试点优化 | 未知 | 低 | 中 | 需 profile small_00/medium_00 |
| 接受现状 | 0 | 无 | 无 | 推荐（进入 O3 预展开） |

## 6. O3 预展开前置工作

O3 周期完成后，需调用 O3 预展开工具生成 O2 编译的 cpp 文件。

### 6.1 需要标记的 O3 依赖代码段
- 迭代 FFT (difIter/iditIter): 依赖 O3 的指令调度优化
- Huge Page 分配: 不依赖 O3，直接可用
- 叶子阈值 16: 不依赖 O3，直接可用

### 6.2 预展开调用时机
- 用户确认 O3 周期完成后
- 由其他工作区负责执行预展开工具

## 7. 下一步建议

1. **推荐**: 接受当前 +22% 优势，进入 O3 预展开阶段
2. **可选**: profile small_00/medium_00 测试点，寻找未探索的瓶颈
3. **高风险**: 研究 GMP mul_fft.c (Schönhage-Strassen) 是否有可借鉴的算法级优化
4. **低优先级**: 研究 Vc SIMD 抽象库

## 8. 重要约束（硬性）

1. `div_opt.cpp` 不可查看或使用
2. `best/*` 不可修改（仅最终易位时更新）
3. 代码注释语言遵循用户最新指令
4. 程序须包含 5 秒超时机制
5. I/O 须用 fread + oBuffer
6. BASE = 10^4（无需 BASE 转换）
7. 所有命令放入 BAT 文件防止 IDE 阻塞
