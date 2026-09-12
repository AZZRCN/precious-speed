# XFT 综合优化验证检查清单（圆梦版）

> 关联文档：`spec.md` `tasks.md`  工作目录：`d:\precious_speed\tft_dev\`

---

## 阶段 0：洛谷 XFT 调研沉淀

### 调研覆盖度
- [x] 洛谷 P3803（FFT 模板）题解调研
- [x] 洛谷 P4245（任意模数 NTT）题解调研
- [x] 洛谷 P4239（任意模数多项式乘法逆）题解调研
- [x] 洛谷 P5282（快速阶乘）题解调研
- [x] 洛谷博客 FFT/NTT/FWT/MTT 学习笔记调研

### XFT 变种可借鉴性评估
- [x] FFT 基础优化（已实施 split-radix）
- [x] NTT（不适用：LC 无模数）
- [x] 三模 NTT + CRT（不适用：常量因子大）
- [x] MTT 拆系数 FFT（思想已部分应用：共轭打包）
- [x] MTT 4次DFT 优化版（已实施等价优化）
- [x] 三次变两次优化（待评估：自平方路径）
- [x] FWT（不适用：非乘法场景）
- [x] TFT（本项目核心创新点）
- [x] 迭代 FFT + 位逆序置换（已实施）
- [x] 预处理单位根表（已实施）

---

## 阶段 1：Python 原型验证

### 理论正确性
- [ ] TFT 输出前 m 项 == `numpy.fft.fft` 前 m 项（14 边界用例）
- [ ] ITFT 从 m 个 DFT 系数恢复时域前 m 项 == 原序列前 m 项
- [ ] 1000 随机用例 fuzzing 全部通过
- [ ] m=1, m=n/2, m=n-1, m=n 边界情况正确

### 乘法正确性
- [ ] `tft_mul(a, b)` == `numpy.fft` 卷积前 conv_len 项
- [ ] 不等长输入（a=1000, b=500）正确
- [ ] 全零输入、全 1 输入正确
- [ ] 大数（10^6 digits）正确

### 性能曲线
- [ ] 收益曲线与理论预估一致（conv_len 接近 2^k 时收益低，远离时收益高）
- [ ] 测得 max_max 对应规模（conv_len=1M-1）的 TFT 收益 < 1%
- [ ] 测得 large 对应规模（conv_len=49K-1）的 TFT 收益 > 10%

---

## 阶段 2：C++ 原型验证

### 编译与运行
- [ ] `tft_mul.cpp` 在 O2 下编译通过（GCC 15.2 兼容）
- [ ] 无未定义行为（ubsan 通过）
- [ ] 无内存泄漏（asan 通过）
- [ ] EXE 体积合理（不超过现有 mul.exe +50KB）

### 正确性
- [ ] 20 LC 用例对比 BEST 输出全部 PASS
- [ ] LC `example_00.in` 通过
- [ ] LC `zero_00.in`（含 0 输入）通过
- [ ] LC `fft_killer_01.in`（2M digits）通过
- [ ] LC `max_max_06.in`（2M digits）通过
- [ ] 大规模随机用例（10^6 digits × 10^3 组）对比 Python 原型通过

### 性能
- [ ] 单用例 max_max_00：TFT 不慢于 BEST（容差 +5%）
- [ ] 单用例 large_00：TFT 比 BEST 快 ≥ 8%（验证 padding 收益）
- [ ] 单用例 medium_00：TFT 与 BEST 持平（容差 ±3%）
- [ ] PROFILE_TFT 输出 DIF/DOT/IDIT 各阶段耗时分布合理

---

## 阶段 3：组合优化验证

### Karatsuba 快速路径
- [ ] Karatsuba 在 64-2048 limbs 范围内比 basicMul 快
- [ ] Karatsuba/FFT 拐点扫描确定（预期 1024-2048 limbs）
- [ ] Karatsuba 用例正确性（1000 随机用例）

### 交错 DFT
- [ ] `dif_interleaved` 输出 == 两次独立 `dif` 输出
- [ ] 大规模用例（max_max）缓存命中率改善（L2/L3 miss 减少）
- [ ] 大规模用例性能提升 ≥ 3%

### 三次变两次（自平方路径）
- [ ] `sqr_three_to_two` 输出 == `absSqr` 输出
- [ ] 自平方用例（如 large_small_00 中 a=b 场景）性能提升 ≥ 2%
- [ ] 在 `absMul` 中正确识别 a=b 场景切换路径

### 集成验证
- [ ] `absMul` 三档路径切换正确
- [ ] `absSqr` 自平方路径切换正确
- [ ] 20 LC 用例全部 PASS
- [ ] 无 thread_local 内存膨胀（tv1/tv2 复用现有缓冲）

### 10% 阈值验证
- [ ] 10 轮 A/B 测速 CUR_XFT 中位数 < BEST 中位数 × 0.90
- [ ] 至少 5 个用例类别（max_max/fft_killer/large/medium/large_small）单独达标
- [ ] 无用例出现 > 5% 退化

---

## 阶段 4：调优与提交

### LC 在线提交
- [ ] CUR_XFT 提交通过 LC 编译
- [ ] 所有 LC 用例 AC
- [ ] 实测耗时记录（Record ID）
- [ ] 与 BEST LC 提交记录对比，差距 ≥ 10%

### 回退准备
- [ ] 若未达 10%，失败原因分析写入 `FAILURE_REPORT.md`
- [ ] XFT 代码保留在 `tft_dev/` 下，通过 `#ifdef USE_XFT` 控制
- [ ] `mul.cpp` 回退到 `real_conv` 路径，20 LC 用例复测通过

---

## 工程约束检查

- [ ] 所有编译使用 `ai.bat compile_div` 或 `tto.exe 30 g++ ...`
- [ ] 所有测试使用 `ai.bat test_div` 或 `tto.exe 10 test.exe`
- [ ] 代码注释全英文
- [ ] I/O 使用 fread + oBuffer
- [ ] BASE = 10^4（无 BASE 转换）
- [ ] 无针对特定源文件的特调
- [ ] O2 编译（无 O3 pragma）
- [ ] 程序包含 5 秒超时机制
