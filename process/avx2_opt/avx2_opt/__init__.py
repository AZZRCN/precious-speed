# AVX2 自动优化器（avx2_opt）

一个用于 LC 风格大数运算程序（及一般标量热循环）的**自动化 AVX2 优化工作台**。
核心闭环：**callgrind 采集 Ir → hotspot 排序 → 检测标量热惯用法 → 策展式 SIMD 改写 → 重测 Ir + bytecmp 闸门**。

详见 README.md（技术规范 / 设计文档）。
