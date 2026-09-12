# SEMANTICS.md — 缩减机等价契约（AVX2 大整数除法 FFT 核）

本文件定义"缩减机"对 `393027_opt.cpp`（HEX 大整数除法，LC #393027）做 AVX2 指令缩减时
**必须遵守的等价契约**。任何被缩减机产出的变体，当且仅当满足以下契约时，才允许替换基线。

## 1. 外部契约（黑盒等价，唯一裁判）

程序从 stdin 读：
```
T
A_1 B_1
...
A_T B_T
```
每行 A,B 为十六进制大整数（无 `0x` 前缀，大端）。对每组输出一行：
```
Q R
```
其中 `Q = A // B`，`R = A % B`，十六进制大端、无前导零（零输出为 `0`）。

**等价判据**：变体在**任意**输入上输出的字节流必须与基线 `393027_opt.cpp` 完全一致
（包括边界：A<B、B=1、A=0、商为 0、商/余数前导零处理）。

- 裁判脚本：`reducer/bench.py oracledir <name> ref <casedir>`（单 query 独立用例，规避 v27 多 query 段错误）
- 用例集：`reducer/cases_dir/`（60 例，含小/中/大 FFT 区间）+ `reducer/cases.txt.big`（单巨例，用于 I refs）
- `failures == 0` 是冻结变体的**必要且充分**条件。

## 2. 内部契约（浮点 FFT 的数值容差）

FFT 用双精度复数做卷积，结果经 `merge_b2` 四舍五入到最近整数 limb。缩减机允许改动：
- 运算顺序、向量化宽度（SSE→AVX2→AVX-512）、SoA/AoS 布局、twiddle 表组织（2 级 ↔ 全表）、
  FMA 融合、蝶形基（radix-2/4/8）、pass 合并。
- **不允许**改变数学语义：卷积结果必须落在 `±0.5` limb 的舍入容差内（现有 `k<=25` 窗口 + 双精度
  已保证 `2^(2k)*lm <= 2^48` 精度预算）。

等价性由黑盒字节比对保证，故数值改动只要不超出舍入容差即被自动接受。

## 3. 度量契约（指令数唯一真值）

- **唯一真值**：`valgrind --tool=callgrind --cache-sim=yes` 的 `I   refs:`（指令 fetch 数）。
  跨 x86-64 二进制在 Intel/AMD 一致（微码不改原始指令条数），故 VM66(Intel 285H) 的 I refs
  直接预测判官 AMD EPYC 7B13(Zen3) 的指令数。
- **判官约束**：提交平台 = Zen3，**仅 AVX2，无 AVX-512**。AVX-512 在判官上约 3× 时间惩罚
  （频率/吞吐回退），故缩减机默认只出 AVX2 决策；AVX-512 仅作显式 `gamble` flag（非判官目标）。
- **编译标志**：`-O3 -march=haswell`（= AVX2/FMA/BMI2，匹配 Zen3 指令集；**禁止** `-mavx512`）。
- **基线**：单巨例 `cases.txt.big`（103900÷51950 limb）I refs = **206,952,320**（v27, md5 5b7bafa1）。

## 4. 缩减机的"更好运算路径"搜索空间（候选变换）

| 编号 | 变换 | 等价性 | 预期收益 | 风险 |
|---|---|---|---|---|
| T1 | 蝶形 SoA 布局（消 3 次 shuffle → 纯 2-FMA） | 黑盒等价 | I refs −10~18% | 中（需改全部 FFT 数据布局） |
| T2 | 2 级 twiddle→全表（256 位成对，2 蝶形/iter） | 黑盒等价 | 不确定（可能 −缓存） | 低 |
| T3 | 递归 difRec/ditRec→全迭代 | 黑盒等价 | ~−1%（调用开销极小） | 低 |
| T4 | AVX-512 路径（仅 `gamble`） | 黑盒等价 | 判官上 3× 惩罚（禁用） | 高（判官） |

缩减机必须对每个候选独立跑 oracle + callgrind，**只保留 `failures==0` 且 I refs 下降**的变体
（T2/T3 若上升则丢弃，不采用）。

## 5. 已知问题（非缩减机引入，需单独修复）

- **v27 多 query 段错误**：`393027_opt.cpp`（md5 5b7bafa1）在**多个大 query 同文件**时段错误
  （单 query 正常）。疑似 FMG 缓冲跨 query 别名/状态污染。oracle 已用单 query 用例规避。
  提交前必须修复（或确认 LC 单测试文件 T=1），否则提交会被判 RE。
- **VM66 perf PMU 不可用**：用 callgrind I refs 替代 `perf instructions:u`（两者跨架构一致）。
