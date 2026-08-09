# real_dot FMA化 原型

## 状态：暂停（精度问题未解决）

## 背景
- mul.cpp line 10 注释：`fma 已移除 — 实测 fma 导致 FFT 浮点精度变化, 触发罕见除法余数错误 (broad#134)`
- real_dot 占执行时间 21.2%，FMA 化理论收益 2-5%
- 之前已尝试并回退

## 代码分析
- real_dot_binrev2 (line 1178): 大规模点积入口
- dot_rfftX2 (line 1070): 核心向量化点积，用 C2::mul 做 4 次 SIMD 乘法
- C2::mul 已是 SIMD 优化（addsub + mul + mul），FMA 化空间有限

## 精度问题根因
- FMA 的舍入方式不同（单次舍入 vs 双次舍入）
- 导致 FFT 结果略有差异，触发边界 case 错误

## 待研究的补偿方案
1. 选择性 FMA：只在非敏感路径用 FMA
2. 双精度补偿：double-double 算法（太慢）
3. Kahan summation：适用于累加，不适用于复乘

## 结论
- 优先做 Task 5（三次变两次），理论收益更高且无精度风险
- Task 3 暂停，待 Task 5 完成后研究补偿方案
- 若无法解决精度问题，记录为"理论收益正但工程不可行"
