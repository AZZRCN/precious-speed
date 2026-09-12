# MUL 大整数乘法优化项目 — 交接文档

## 日期
2026-07-31

## 项目概述
本项目旨在优化 LC (Library Checker) "Multiplication of Big Integers" 题目的乘法实现，目标是超越 BEST 版本（#385663，36ms）。

## 最终状态

### 当前生产代码
- **文件**: `d:\precious_speed\mul.cpp`
- **版本**: #385663 (BEST)
- **LC 成绩**: AC 36ms (取最长时间点)
- **最慢用例**: fft_killer_01 = 36ms, max_max_00/05/07 = 35ms
- **EXE 大小**: ~108 KB

### BEST 版本对比
| 版本 | LC 最长 | medium_02 | max_max_00 | fft_killer_01 | large_small_00 | 内存 |
|------|---------|-----------|------------|---------------|----------------|------|
| #385663 (当前) | 36ms | 27ms | 35ms | 36ms | 33ms | 32.30 MiB |
| #387374 (AZZRCN) | 36ms | 18ms | 36ms | 35ms | 36ms | 34.62 MiB |
| #389571 (旧CUR,已弃用) | 49ms | 18ms | 35ms | 37ms | 36ms | 34.78 MiB |

**关键观察**: 385663 和 387374 各有优劣，理论上存在结合空间，但 LC 取最长时间点，任何 case 变慢都会拉低成绩。

## 优化结果汇总

### 已尝试并验证的方向（全部无正收益）

| # | 方向 | 结果 | 文件位置 |
|---|------|------|----------|
| 1 | dif_two (双DFT交错) | 中性，±0.1%，max_max_06 回退14ms | 已回退 |
| 2 | WFTA8 (8点Winograd) | max_max 快4-7%，端到端仅1.1%，但max_max_06回退 | archieve/mul_research/tft_dev/WFTA_推广/ |
| 3 | WFTA16/32推广 | -3.4% vs BEST，152/200胜，EXE增大 | 同上(已回退) |
| 4 | real_dot FMA化 | 精度问题，C2::mul已SIMD，收益有限 | archieve/mul_research/tft_dev/real_dot_FMA/ |
| 5 | 三次变两次 | 代码已用RIRI打包实现，无需额外工作 | archieve/mul_research/tft_dev/三次变两次/ |
| 6 | TFT向量化 | 无净收益 | archieve/mul_research/tft_dev/TFT向量化/ |
| 7 | MTT共轭 | RIRI比MTT快5-10%，无收益 | 未封存(原型在tft_dev) |
| 8 | Six-step FFT | N=2^20仅快5.6%，小尺寸更慢，~3%边际 | 未封存(原型在tft_dev) |
| 9 | NTT vs FFT | NTT慢1.13-1.61倍 | archieve/mul_research/ssa_dev/ |
| 10 | SSA 30位NTT+CRT | 慢14.6倍 (78ms vs 16ms) | archieve/mul_research/ssa_dev/ntt_30bit/ |
| 11 | SSA 64位NTT单模数 | 慢9.1倍 (146ms vs 16ms) | archieve/mul_research/ssa_dev/ntt_64bit/ |
| 12 | SSA 完整递归 | 理论慢4.3倍 (~69ms vs 16ms) | 仅理论分析 |
| 13 | 混合自适应算法 | Karatsuba比基础慢6-10倍 | archieve/mul_research/tft_dev/自研_混合自适应/ |
| 14 | DCT乘法 | 理论证伪，0/20通过 | archieve/mul_research/tft_dev/自研_DCT乘法/ |
| 15 | SIMD查找表 | 慢1-25倍 | archieve/mul_research/tft_dev/自研_SIMD查找表/ |
| 16 | 矩阵乘法 | 慢2-10倍 | archieve/mul_research/tft_dev/自研_矩阵乘法/ |

### 未尝试的方向（未来可选）
| 方向 | 可行性评估 | 备注 |
|------|-----------|------|
| radix-4 蝶形 | 中等 | 2^20点FFT层数20→10，减少50%内存访问 |
| 针对 fft_killer 专项 | 中等 | 特殊构造case，可能有针对性优化 |
| 结合385663+387374优势 | 低风险 | 需定位medium_02差异根因 |
| Harvey 截断乘法 | 高风险 | 需独立开发周期 |
| split-radix FFT | 高风险 | 需独立开发周期 |

## 文件结构

### 生产代码
```
d:\precious_speed\
├── mul.cpp              # 当前生产版本 (#385663 BEST, 36ms)
├── best\
│   ├── mul_385663.cpp   # BEST 版本1 (推荐基线)
│   ├── mul_387374.cpp   # BEST 版本2 (AZZRCN优化)
│   └── mul.cpp          # 备份
├── ai.bat               # 统一编译/测试入口 (timeout机制)
└── toolbox\
    ├── tbx.exe          # 自研工具箱
    └── tto.exe          # 超时执行器
```

### 研究成果归档
```
d:\precious_speed\archieve\mul_research\
├── mul_cur_389571.cpp          # 旧CUR版本 (已弃用)
├── tft_dev\                    # 算法研究目录
│   ├── FINAL_REPORT.md         # 13个方向最终报告
│   ├── spec.md / tasks.md      # 规格文档
│   ├── WFTA_推广\              # WFTA8/16/32完整原型
│   ├── real_dot_FMA\           # FMA优化原型
│   ├── 三次变两次\              # RIRI打包验证
│   ├── TFT向量化\              # TFT原型
│   ├── 自研_混合自适应\         # Karatsuba+FFT混合
│   ├── 自研_DCT乘法\            # DCT卷积验证
│   ├── 自研_SIMD查找表\         # 查找表乘法
│   ├── 自研_矩阵乘法\           # Toeplitz矩阵乘法
│   └── survey\SURVEY.md        # 互联网资料调研
└── ssa_dev\                    # SSA算法研究
    ├── SSA_FINAL_REPORT.md     # SSA三路径评估报告
    ├── ntt_30bit\              # 30位NTT原型
    └── ntt_64bit\              # 64位NTT原型
```

### 测试工具
```
d:\precious_speed\lc_bench\
├── cases\mul\                  # LC测试点 (20个case)
├── daytime_bench.py            # 白天精确测速 (10轮中位数)
├── verify_mul.py               # 正确性验证
└── run_mul_profile.py          # 性能剖析
```

## 技术细节

### 385663 (BEST) 核心架构
- **FFT**: AVX2 `__m256d` 蝶形，一次处理2个复数
- **R2C对称**: 利用实数DFT共轭对称性减半计算
- **自平方路径**: a=b时省一次DIF+半次点积
- **不平衡乘法拆分**: length >= 2*other.length 时分块，复用other的DIF
- **内存池**: 24桶size-class，按2的幂次预分配
- **BASE**: 10^4 limbs，base 10^8 打包基础乘法

### 性能剖析数据 (max_max_00, float_len=1048576)
```
real_conv 分解:
  DIF  = 8.7ms (54%)
  DOT  = 2.6ms (16%)
  IDIT = 4.9ms (30%)
```

### 核心限制
1. **FFT核心代码与BEST完全相同**，占总执行时间82%
2. 非FFT部分仅18%，优化空间有限
3. LC取最长时间点，任何case回退都会拉低成绩
4. 本地(Intel)与LC(AMD Zen3)架构差异可能导致优化效果不一致

## 关键发现

### 1. max_max_06 异常回退现象
- 旧CUR(389571)在max_max_06上49ms，比BEST(34ms)慢15ms
- 根因：dif_two + WFTA8 在 AMD Zen3 上导致该特定case性能回退
- 教训：本地测速无法完全反映LC环境，Zen3对某些指令序列敏感

### 2. WFTA8 的局限性
- W8^1 实虚部绝对值相等，可大幅减少乘法次数
- W16^1/W16^3 实虚部不等，无法复现优化，推广失败
- WFTA8仅对float_len=16路径有效，占比小

### 3. NTT在LC规模下无法竞争
- LC最大~3.3M bits，远低于SSA优势阈值(>10^7 bits)
- NTT标量模运算无法向量化，与AVX2浮点蝶形差距过大
- 即使Goldilocks prime优化模约简，仍慢一个数量级

### 4. 两个BEST版本各有优劣
- 385663: medium_02慢(27ms)，但max_max_00快(35ms)
- 387374: medium_02快(18ms)，但max_max_00慢(36ms)
- 理论上存在结合空间，但未实现

## Bug修复记录

### 1. 30位NTT正确性验证失败
- 原因：旋转因子表未重新初始化
- 修复：验证前强制重置twiddle_n=0，重新init_twiddle(M)

### 2. 64位NTT原根验证失败
- 原因：g=3不是Goldilocks素数的原根
- 修复：实现find_primitive_root()，自动搜索找到g=7

### 3. 1ULL << 64 溢出
- 原因：64位无符号左移64位未定义
- 修复：用十六进制直接表示 p=0xFFFFFFFF00000001ULL

### 4. NTT蝶形运算顺序错误
- 原因：DIF蝶形先乘旋转因子后加减，与算法相反
- 修复：DIF先加减后乘，DIT先乘后加减

### 5. FMA指令精度问题
- 原因：FMA优化点积导致浮点精度变化，触发罕见除法余数错误
- 修复：移除FMA优化，回归普通加法乘法

### 6. 交错DFT大规模回退
- 原因：float_len>4096时寄存器溢出到L2/L3，性能下降7-8%
- 修复：引入DIF_TWO_MAX_LEN=4096阈值

## 测试方法论

### 测速原则
1. **白天O2测速**: 使用daytime_bench.py，10轮中位数，避免夜间VM噪声
2. **即时交替对比**: CUR vs BEST 交替运行，避免时间漂移
3. **20 case全覆盖**: 不只测max_max，也测small/medium/large
4. **本地+VM双环境**: 本地Intel + VM(AMD Zen3模拟)交叉验证

### 编译命令
```
ai.bat compile_mul  # = tto.exe 30 g++ -std=c++20 -O2 -march=native -I. mul.cpp -o cur_mul.exe -lpthread
ai.bat test_mul     # = tto.exe 10 cur_mul.exe
```

## 下一步建议

### 短期（低风险）
1. **对比385663 vs 387374**: diff两个版本，定位medium_02(27vs18ms)差异根因，尝试结合优势
2. **radix-4蝶形**: 减少FFT层数，理论减少内存访问

### 中期（中风险）
3. **fft_killer专项分析**: 分析特殊构造case的输入特征
4. **内存对齐+预取**: 改善缓存局部性

### 长期（高风险，需独立开发周期）
5. **Harvey截断乘法**: 2017年理论，75%全乘法时间
6. **split-radix FFT**: 最优乘法次数，需重写FFT核心
7. **算法级突破**: NTT替代FFT需在>10^7 bits规模才有效

## 项目结论

MUL优化已触及当前FFT架构的天花板。核心限制在于FFT核心代码与BEST完全相同（占82%执行时间），非FFT部分优化空间仅18%。所有13个算法方向均无正收益，SSA在LC规模下慢4.3-14.6倍。

建议：若需继续突破，应转向算法级创新（split-radix/Harvey截断），需独立开发周期。否则，当前385663(36ms)已是FFT架构下的最优解。
