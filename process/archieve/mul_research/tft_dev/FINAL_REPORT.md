# MUL 优化最终性能报告

> **日期**: 2026-07-31
> **优化对象**: MUL（mul.cpp）
> **编译选项**: O2 -march=native
> **基准**: CUR vs BEST，20 LC 用例 × 多轮测速

---

## 一、总体结论

**CUR 与 BEST 完全持平（-0.1%, 104/200 轮胜），所有尝试的方向均无正收益。**

核心限制：FFT 核心代码与 BEST 完全相同，占总执行时间 82%，非 FFT 部分仅 18%。即使非 FFT 部分全部优化，整体收益天花板也仅 18%。

---

## 二、阶段二：技术方向全面尝试

### 2.1 五个原型实测

| # | 方向 | 结论 | 实测数据 | 文件夹 |
|---|------|------|----------|--------|
| 1 | MTT共轭优化 | 无收益 | MTT比标准FFT快25-35%，但RIRI比MTT快5-10% | tft_dev/proto_mtt.cpp |
| 2 | Six-step FFT | 边际收益 | 仅N=2^20时快5.6%，小规模更慢，~3%边际 | tft_dev/proto_sixstep.cpp |
| 3 | NTT vs FFT | 无收益 | NTT慢1.13-1.61x（x86蝶形6cycles vs FFT 2-3cycles） | tft_dev/proto_ntt.cpp |
| 4 | dif_two（双DFT交错） | 中性 | 白天O2: -0.1%（104/200轮胜），已集成 | mul.cpp |
| 5 | WFTA 8点 | 已集成 | max_max MUL快4-7%，端到端快1.1%，CUR vs BEST快3.8% | mul.cpp |

### 2.2 WFTA 16/32 推广

- **结论**: -3.4% 负优化（CUR wins 70/200），已回退
- **根因**: W16^1/W16^3 实虚部不等，无法实现 (s,d) 2-乘法优化，破坏 SIMD 流水线
- **当前**: difWfta16/32 函数定义保留在 mul.cpp 但不调用
- **文件夹**: tft_dev/WFTA_推广/

### 2.3 四个回填方向

| # | 方向 | 结论 | 文件夹 |
|---|------|------|--------|
| 1 | real_dot FMA | 暂停（精度问题：FMA单次舍入触发FFT浮点误差） | tft_dev/real_dot_FMA/ |
| 2 | 三次变两次 | 已在现有代码实现（RIRI打包=dif<true>），无需额外工作 | tft_dev/三次变两次/ |
| 3 | WFTA推广向量化 | 不可行（C2打包下"复数0乘1"无意义，mul1st乘法次数相同） | tft_dev/WFTA_推广/ |
| 4 | TFT向量化 | 无净收益（TFT递归剪枝开销抵消padding节省） | tft_dev/TFT向量化/ |

---

## 三、阶段三：自研算法方向

### 3.1 四个自研方向汇总

| # | 方向 | 结论 | 实测数据 | 文件夹 |
|---|------|------|----------|--------|
| A | 混合自适应 | 无收益 | Karatsuba比basic慢6-10x（vector+递归开销），basic在N≤1024都比FFT快 | tft_dev/自研_混合自适应/ |
| B | DCT乘法 | 理论证伪 | DCT直接点积0/20 PASS，对称卷积≠标准卷积 | tft_dev/自研_DCT乘法/ |
| C | SIMD查找表 | 理论证伪 | 查找表比base 10^8打包慢1-25x | tft_dev/自研_SIMD查找表/ |
| D | 矩阵乘法 | 理论证伪 | Toeplitz比直接卷积慢2-10x，分块矩阵慢6-26x | tft_dev/自研_矩阵乘法/ |

### 3.2 自研方向失败根因分析

**方向A（混合自适应）**：
- 原型实现（vector+递归）无法与生产级FFT竞争
- Karatsuba的vector动态分配开销远超算法节省的乘法次数
- basic_mul在N≤1024都比原型FFT快（原型FFT常数大）

**方向B（DCT乘法）**：
- DFT卷积定理：DFT(a*b) = DFT(a)·DFT(b)，适用于大数乘法
- DCT卷积定理：DCT对应"对称卷积"≠ 标准卷积
- 要用DCT做标准卷积需对称延拓（长度2L），但DFT只需长度L
- RIRI打包已覆盖实数序列的高效DFT

**方向C（SIMD查找表）**：
- 标量查找表比直接乘法更慢（额外内存访问4-5周期 vs 乘法1周期）
- base 10^8打包已将O(n²)降低4倍
- LC场景basic_mul路径占比 < 1%

**方向D（矩阵乘法）**：
- 大数乘法（卷积）= Toeplitz矩阵乘向量，不是标准矩阵乘法
- Toeplitz矩阵乘向量复杂度O(n²)，与直接卷积相同
- 标准矩阵乘法C=A×B是三矩阵运算，大数乘法只有两输入
- 即使SIMD化，O(n²)仍比FFT的O(n log n)慢

---

## 四、当前 CUR 状态

### 4.1 已集成优化
1. **dif_two（双DFT交错）**: float_len ≤ 4096 使用交错DFT（L1缓存效率），> 4096 使用单独DFT
2. **WFTA 8点**: float_len=16 路径使用 difWfta8 替代标准 split-radix DFT
3. **RIRI打包（三次变两次）**: dif<true> 实现两个实数序列打包为复数序列

### 4.2 性能数据
- **CUR vs BEST**: -0.1%（104/200 轮胜），完全持平
- **CUR EXE**: 225.8 KB（比 BEST 小 12.9 KB）
- **20 LC 用例**: 20/20 PASS

### 4.3 核心限制
- FFT核心代码与BEST完全相同，占总执行时间82%
- 非FFT部分仅18%，即使全部优化也只能获得有限收益
- real_dot占21.2%，但FMA化因精度问题不可行
- DIF占FFT时间52.3%，已完全向量化（addsd/mulsd=0）
- IDIT占26.5%，已SIMD优化
- split-radix乘法次数已最优

---

## 五、剩余可行方向

### 5.1 低优先级（收益不确定）
1. **real_dot FMA补偿方案**: 选择性FMA（非敏感路径）或双精度补偿
2. **WFTA向量化blend方案**: 用blend/mask跳过零乘法（可能拯救WFTA16/32）
3. **Karatsuba中规模优化**: 需确定FFT拐点，LC中规模用例占比小

### 5.2 高成本（需独立开发周期）
1. **NTT替代FFT**: x86上NTT慢1.13-1.61x，需专用硬件（IFMA52）
2. **Split-radix FFT重新实现**: 需完整重写FFT核心，高风险
3. **Harvey截断乘法**: 2017年论文，75%全乘法时间，实现复杂

---

## 六、文件夹结构总览

```
tft_dev/
├── survey/SURVEY.md              # 55+算法调研报告
├── self_research.md              # 自研算法方向调研
├── FINAL_REPORT.md               # 本报告
├── real_dot_FMA/                 # FMA精度问题（暂停）
├── 三次变两次/                    # 已在现有代码实现
├── WFTA_推广/                    # WFTA16/32不可行
├── TFT向量化/                    # TFT无净收益
├── 自研_混合自适应/              # 原型无法竞争
├── 自研_DCT乘法/                 # DCT卷积≠标准卷积
├── 自研_SIMD查找表/              # 查找表比打包慢
├── 自研_矩阵乘法/                # 矩阵范式无法优于FFT
├── proto_mtt.cpp                 # MTT共轭原型
├── proto_sixstep.cpp             # Six-step FFT原型
├── proto_ntt.cpp                 # NTT原型
├── proto_wfta.cpp                # WFTA原始原型
├── proto_split.cpp               # Split-radix原型
├── interleaved_dft_proto.cpp     # 交错DFT原型
├── karatsuba_proto.cpp           # Karatsuba原型
├── tft_cpp_proto.cpp             # TFT C++原型
├── tft_proto.py                  # TFT Python原型
├── debug_tft.py                  # TFT调试
└── run_tests.py                  # 测试脚本
```

---

## 七、结论

本次 SPEC 优化目标是 MUL，尝试了 13 个方向（5个原型 + 4个回填 + 4个自研），均无正收益。

**根本原因**: CUR 的 FFT 核心代码与 BEST 完全相同，占总执行时间 82%。所有尝试的方向要么无法优化 FFT 核心（已完全向量化），要么优化非 FFT 部分（仅18%天花板），要么理论上不适用于大数乘法（DCT/矩阵）。

**CUR 当前状态**: 与 BEST 完全持平（-0.1%），已集成 dif_two + WFTA8 + RIRI 打包三个优化，20/20 用例通过，EXE 比 BEST 小 12.9 KB。

**下一步建议**: 算法层面突破（NTT/split-radix/Harvey截断乘法）需要独立开发周期，超出当前优化范围。建议暂停优化，等待新的硬件支持（如 AVX-512 IFMA52）或新的算法突破。
