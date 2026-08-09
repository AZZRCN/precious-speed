# XFT 综合优化任务清单（圆梦版）

> 关联文档：`spec.md`  工作目录：`d:\precious_speed\tft_dev\`

---

## 阶段 0：洛谷 XFT 调研沉淀（已完成）

- [x] 0.1 收集洛谷 P3803/P4245/P4239/P5282 题解中的 XFT 优化思想
- [x] 0.2 整理 XFT 变种核心思想与可借鉴性矩阵（写入 spec.md §1.2）
- [x] 0.3 评估当前 mul.cpp 已实施的洛谷优化（共轭打包、迭代FFT、单位根表等）
- [x] 0.4 输出洛谷优化吸收清单（spec.md §1.4）

---

## 阶段 1：理论研究与 Python 原型（待 spec 批准后启动）

### T1.1 搭建 Python TFT 原型骨架
- [ ] 创建 `tft_proto.py`
- [ ] 实现递归 TFT（基于 Cooley-Tukey 分解，输入长度 n=2^k，输出前 m 个系数）
- [ ] 实现逆向 ITFT（van der Hoeven 逆向剪枝算法）
- 输出：可独立运行的 Python 模块

### T1.2 TFT 正确性验证
- [ ] 14 边界用例：不等长、全零、全 1、大数、n=2^k、n=2^k±1、n=16/256/1024
- [ ] 与 `numpy.fft.fft` 对比 TFT 输出前 m 项 == DFT 前 m 项
- [ ] 1000 随机用例 fuzzing
- 输出：测试报告 `tft_proto_test.md`

### T1.3 TFT 乘法原型
- [ ] 实现 `tft_mul(a, b)` 流程：TFT(a) → TFT(b) → 点积 → ITFT
- [ ] 与 `numpy.fft` 乘法对比正确性
- 输出：`tft_mul_proto.py`

### T1.4 TFT 性能基准
- [ ] 扫描 conv_len ∈ {2^k, 2^k-1, 2^k+1 | k=4..20}
- [ ] 测量 TFT 乘法 vs FFT 乘法耗时
- [ ] 输出收益曲线，确认理论与实测一致
- 输出：`tft_perf_curve.csv` + `tft_perf_curve.png`

---

## 阶段 2：C++ 原型实现

### T2.1 模块设计
- [ ] 设计 `tft_mul.cpp` 接口（对齐 `real_conv`）
- [ ] 设计 TFT 类模板（Float/Complex 类型参数化）
- [ ] 旋转因子表复用方案（与现有 `expand` 机制兼容）

### T2.2 TFT 核心实现
- [ ] 实现 `tft_dif<Float>(inout, n, m)`（前向 TFT，剪枝到 m 个输出）
- [ ] 实现 `itft_dit<Float>(inout, n, m)`（逆向 ITFT）
- [ ] AVX2 向量化（参考现有 `dif/idit` 的 C2 打包结构）

### T2.3 TFT 卷积
- [ ] 实现 `tft_real_conv(v1, v2, conv_len, float_len)` 替代 `real_conv`
- [ ] 实数 DFT 共轭对称性处理（参考 `real_dot_binrev2`）
- [ ] 添加 `PROFILE_TFT` 宏用于阶段计时

### T2.4 C++ 原型正确性
- [ ] 20 LC 用例对比 BEST 输出（复用 `verify_mul.py`）
- [ ] 大规模随机用例（10^6 digits）对比 Python 原型
- 输出：`tft_cpp_verify.md`

### T2.5 C++ 原型性能
- [ ] 单用例测速：max_max_00, large_00, medium_00
- [ ] 与 BEST 对比，定位性能瓶颈
- 输出：`tft_cpp_bench.csv`

---

## 阶段 3：组合优化集成（吸收洛谷 XFT 思想）

### T3.1 Karatsuba 快速路径
- [ ] 实现 `karatsuba_mul(in1, in2, out)`（覆盖 64-2048 limbs）
- [ ] 扫描拐点：确定 Karatsuba/FFT 切换阈值
- [ ] 中等规模用例（medium_*, large_*）性能对比

### T3.2 双 DFT 交错执行
- [ ] 实现 `dif_interleaved(v1, v2, float_len)`（2 次 DFT 在每层交错）
- [ ] 缓存局部性分析（L2/L3 命中率）
- [ ] 大规模用例（max_max_*, fft_killer_*）性能对比

### T3.3 自平方路径三次变两次优化（吸收洛谷 P3803 NaCly_Fish 思想）
- [ ] 实现 `sqr_three_to_two(a, out)`：a 放实部+虚部，DFT(a+ai)²，IDFT
- [ ] 与现有 `absSqr` 对比性能
- [ ] 仅 a=b 场景生效，需在 `absMul` 中识别

### T3.4 集成到 mul.cpp
- [ ] 添加 `#define USE_XFT` 编译开关
- [ ] `absMul` 中按规模选择路径：basic / Karatsuba / TFT / FFT
- [ ] `absSqr` 中按需切换三次变两次路径
- [ ] 保留原 `real_conv` 作为回退

### T3.5 10 轮 A/B 测速
- [ ] 扩展 `ab_mul_final.py` 支持 XFT 版本
- [ ] 10 轮测速取中位数：CUR_XFT vs BEST
- [ ] 验证 10% 硬性阈值
- 输出：`xft_ab_bench.csv`

---

## 阶段 4：调优与回退

### T4.1 阈值调优
- [ ] 三档切换点扫描：Karatsuba↔TFT, TFT↔FFT
- [ ] 各档用例性能曲线
- [ ] 自平方路径切换阈值

### T4.2 LC 在线提交
- [ ] 提交 CUR_XFT 版本到 LC
- [ ] 记录 Record ID 与实测耗时
- [ ] 与 BEST LC 提交记录对比

### T4.3 回退准备
- [ ] 若未达 10%，失败原因分析写入 `FAILURE_REPORT.md`
- [ ] XFT 代码保留在 `tft_dev/` 下，通过 `#ifdef USE_XFT` 控制
- [ ] 回退 `mul.cpp` 到 `real_conv` 路径

---

## 阶段汇报节点

| 节点 | 触发条件 | 汇报内容 |
|------|----------|----------|
| N0 | 阶段 0 完成 | 洛谷 XFT 调研报告与吸收清单（已写入 spec §1） |
| N1 | T1.4 完成 | TFT 理论收益曲线与实测对比 |
| N2 | T2.5 完成 | C++ 原型性能瓶颈分析 |
| N3 | T3.1 完成 | Karatsuba 拐点确定 |
| N4 | T3.3 完成 | 三次变两次自平方路径收益评估 |
| N5 | T3.5 完成 | 10% 阈值达成/未达成判定 |
| N6 | T4.2 完成 | LC 实测对比与最终决策 |

---

## 最终完成状态（2026-07-31）

### 阶段二：技术方向全面尝试（仅 MUL）— 全部完成

**5个原型实测**:
- [x] MTT共轭优化 → 无收益（RIRI比MTT快5-10%）
- [x] Six-step FFT → 边际收益（~3%）
- [x] NTT vs FFT → 无收益（NTT慢1.13-1.61x）
- [x] dif_two（双DFT交错）→ 中性（-0.1%），已集成到mul.cpp
- [x] WFTA 8点 → 已集成到mul.cpp

**WFTA 16/32推广**:
- [x] 标量版 → -3.4%负优化，已回退
- [x] 向量化方案 → 不可行（C2打包下"复数0乘1"无意义）

**4个回填方向**:
- [x] real_dot_FMA → 暂停（精度问题）
- [x] 三次变两次 → 已在现有代码实现（RIRI打包）
- [x] WFTA_推广 → 不可行
- [x] TFT向量化 → 无净收益

### 阶段三：自研算法方向 — 全部完成（均无收益）

- [x] 方向A：混合自适应 → 原型无法与生产级FFT竞争
- [x] 方向B：DCT乘法 → DCT卷积≠标准卷积（0/20 PASS）
- [x] 方向C：SIMD查找表 → 查找表比base 10^8打包慢1-25x
- [x] 方向D：矩阵乘法 → 大数乘法≠标准矩阵乘法

### Task 15: 合并正收益优化
- [x] 无正收益，无需合并

### Task 16: 最终报告
- [x] FINAL_REPORT.md 已生成

### 最终结论
CUR与BEST完全持平（-0.1%, 104/200轮胜），所有13个方向均无正收益。
核心限制：FFT核心代码与BEST完全相同，占总执行时间82%。
