# Tasks - O3 源码级展开优化

## 阶段 0：准备工作

- [ ] Task 0.1: 创建 `ai.bat` 命令批处理文件框架
  - [ ] 包含 SSH 调用模板、VM 文件上传/下载、benchmark 触发
  - [ ] 所有长命令用 `start /b` 后台执行
- [ ] Task 0.2: 在 VM 上准备 O3 展开工作目录 `/tmp/o3expand/`
  - [ ] 复制 fusion.cpp、best/* 到该目录
  - [ ] 准备测试数据（ADD/MUL/DIV 各 2 组）
- [ ] Task 0.3: 确认 VM 上 g++ 15.2 的 `-O3 -S` 与 `-O2 -S` 输出差异
  - [ ] 用 fusion.cpp 生成 `fusion_O3.s` 和 `fusion_O2.s`
  - [ ] 统计大小、函数数量、关键差异点
  - [ ] 记录到 HANDBOOK.MD

## 阶段 1：方案 B 单函数 POC（验证可行性）

- [ ] Task 1.1: 识别 fusion.cpp 中的 hot 函数清单
  - [ ] 用 `perf record` 或简单 instrumentation 量化每个函数在 ADD 1M+1M 中的耗时占比
  - [ ] 输出 top 5 hot 函数清单（预计：absAdd/absSub/str4toi/writeTo/fromCharRange）
- [ ] Task 1.2: 写 `tools/asm_extract.py` 脚本
  - [ ] 输入：`.s` 汇编文件 + 函数名
  - [ ] 输出：清洗后的 asm 字符串（去 `.cfi_*`、`.p2align`、`endbr` 等）
  - [ ] 处理：label 重命名避免冲突、识别 input/output/clobber 寄存器
- [ ] Task 1.3: 选择 absAdd 作为 POC 函数
  - [ ] 从 `fusion_O3.s` 提取 absAdd 汇编
  - [ ] 用 `asm volatile(...)` 包装，正确指定 input/output/clobber
  - [ ] 嵌入到 `fusion_o3expand_v1.cpp`（其余函数保留原 C++）
- [ ] Task 1.4: VM 验证 POC 正确性
  - [ ] 编译 `fusion_o3expand_v1.cpp`（-O2，不带 pragma）
  - [ ] 跑 ADD 1M+1M，diff 输出 vs fusion v2 输出
  - [ ] 若 FAIL，调试 asm 块（检查寄存器clobber、内存别名）
- [ ] Task 1.5: VM benchmark POC 性能
  - [ ] 对比：fusion v2 -O2 / fusion v2 -O3 / fusion_o3expand_v1 -O2
  - [ ] 期望：v1 -O2 接近 v2 -O3，远超 v2 -O2
  - [ ] 记录到 HANDBOOK.MD

## 阶段 2：方案 B LC 平台合规性验证（关键关卡）

- [ ] Task 2.1: 提交 `fusion_o3expand_v1.cpp` 到 LC ADD 题目（带 asm 块）
  - [ ] 仅提交，观察是否编译通过
  - [ ] 记录 LC 返回：AC/CE/RE
- [ ] Task 2.2: 若 LC 拒绝 asm 块
  - [ ] 记录拒绝原因（编译错误信息）
  - [ ] 跳转到阶段 5（方案 E）
- [ ] Task 2.3: 若 LC 接受 asm 块
  - [ ] 记录 LC 真实成绩（CPU 时间、内存）
  - [ ] 与 best LC ADD 18ms 对比
  - [ ] 继续 阶段 3

## 阶段 3：方案 B 全函数覆盖

- [ ] Task 3.1: 扩展 asm_extract.py 支持批量函数提取
  - [ ] 输入：函数名列表
  - [ ] 输出：每个函数的 asm 块 + 统一的 clobber 分析
- [ ] Task 3.2: 逐个替换 hot 函数到 asm 版本
  - [ ] 每替换 1 个函数 → 编译 → 正确性测试 → benchmark
  - [ ] 顺序：absAdd → absSub → str4toi → writeTo → fromCharRange → fftMul → fftSqr
  - [ ] 每步生成 v1.1, v1.2, v1.3... 版本
- [ ] Task 3.3: 处理 MUL 路径的 hot 函数
  - [ ] fftMulUnbalanced、fftMulPre 等
  - [ ] 同样逐个替换 + 测试
- [ ] Task 3.4: 处理 DIV 路径的 hot 函数
  - [ ] absInvNewton、absDivRemCore1/Core2 等
  - [ ] 同样逐个替换 + 测试
- [ ] Task 3.5: 生成 `fusion_o3expand_v2.cpp`（全函数覆盖版）
  - [ ] 检查文件大小 ≤ 256KB
  - [ ] 完整正确性测试（ADD/MUL/DIV 各 5 组）
  - [ ] 完整 benchmark（vs best + fusion v2）

## 阶段 4：方案 B 性能 benchmark 与迭代

- [ ] Task 4.1: 完整 benchmark 矩阵
  - [ ] ADD: 1M+1M, 100k+100k, 500k+500k
  - [ ] MUL: 500k*500k, 100k*100k, 300k*300k
  - [ ] DIV: 1M/500k, 200k/100k, 1M/100k, 1M/999k
  - [ ] 对比：fusion v2 -O2 / fusion v2 -O3 / fusion_o3expand_v2 -O2 / best -O2
- [ ] Task 4.2: 若性能不达标，分析瓶颈
  - [ ] 用 perf/cachegrind 分析热点
  - [ ] 识别：是 asm 块本身慢，还是 asm 块之间调用开销
  - [ ] 考虑：把多个 asm 块合并成一个（避免调用开销）
- [ ] Task 4.3: 迭代优化
  - [ ] v2.1, v2.2... 持续优化
  - [ ] 每次变更后跑正确性 + benchmark
  - [ ] git commit 每个里程碑

## 阶段 5（备选）：方案 E — 针对性 GIMPLE 转换

> **触发条件**：阶段 2 LC 拒绝 asm 块，或阶段 4 性能不达标

- [ ] Task 5.1: 在 VM 上装 g++ 11.4 双版本（对齐 LC）
  - [ ] `apt install gcc-11 g++-11`
  - [ ] 验证 `g++-11 --version`
- [ ] Task 5.2: 用 g++-11 生成干净 GIMPLE dump
  - [ ] `g++-11 -O3 -fno-tree-vectorize -fdump-tree-optimized=fusion_O3_gimple.txt fusion.cpp`
  - [ ] 分析 dump 格式，识别 hot 函数的优化形式
- [ ] Task 5.3: 写 `tools/gimple2cpp.py` 转换脚本
  - [ ] 处理：PHI 节点 → 循环变量、SSA 重命名、标签重写、注释删除
  - [ ] 不处理：向量化 treecode（已用 -fno-tree-vectorize 关闭）
  - [ ] 输出：合法 C++ 函数体
- [ ] Task 5.4: 将转换后的函数嵌入 `fusion_o3expand_e.cpp`
  - [ ] 正确性测试
  - [ ] benchmark 对比

## 阶段 6（兜底）：方案 A — 手动 intrinsics/unroll

> **触发条件**：方案 B 和 E 都失败

- [ ] Task 6.1: 识别可手动向量化的循环
  - [ ] absAdd：uint16_t 加法，可 16 路并行（AVX2 __m256i）
  - [ ] str4toi：查表法已优化，可考虑 SWAR 批量
  - [ ] writeTo：4 位查表已优化
- [ ] Task 6.2: 手写 AVX2 intrinsics 版本
  - [ ] absAdd_avx2、absSub_avx2
  - [ ] 用 `#pragma GCC target("avx2,fma")` 启用（不是 O3）
- [ ] Task 6.3: 嵌入 `fusion_manual_avx2.cpp`
  - [ ] 正确性 + benchmark
  - [ ] 提交 LC

## 阶段 7：提交 LC 三模式获取真实成绩

- [ ] Task 7.1: 提交 ADD 模式
  - [ ] 用 `fusion_o3expand_final.cpp` + `-DHINT_OP_ADD`
  - [ ] 记录 Record ID、score、CPU 时间
  - [ ] 5 秒间隔后提交下一个
- [ ] Task 7.2: 提交 MUL 模式
  - [ ] 同上，`-DHINT_OP_MUL`
- [ ] Task 7.3: 提交 DIV 模式
  - [ ] 同上，`-DHINT_OP_DIV`
- [ ] Task 7.4: 汇总 LC 真实成绩
  - [ ] 对比 best LC 历史：ADD 18ms / MUL 36ms / DIV 108ms
  - [ ] 记录到 HANDBOOK.MD

## 阶段 8：疯狂优化阶段（不停歇）

> **触发条件**：阶段 7 完成，用户仍未醒来

- [ ] Task 8.1: 翻看 E:\ 盘根目录
  - [ ] 用 `LS E:\` 列出内容
  - [ ] 寻找参考资料、历史代码、论文 PDF
  - [ ] 若有相关内容，阅读并记录可借鉴的优化思路
- [ ] Task 8.2: 阅读相关论文
  - [ ] FFT 优化：split-radix、mixed-radix
  - [ ] 大整数除法：GMP approximate inverse、Möller block strategy
  - [ ] SIMD：AVX2/AVX-512 intrinsics 最佳实践
  - [ ] 编译器优化：GCC internals、autovectorization guide
- [ ] Task 8.3: 持续尝试新优化
  - [ ] 每个新思路 → 独立 cpp 版本 → benchmark
  - [ ] git commit 每次实验
  - [ ] 即使失败也记录原因
- [ ] Task 8.4: 定期检查对话长度
  - [ ] 若接近上限，主动总结到 HANDBOOK.MD
  - [ ] 确保 spec.md 的"跨压缩传承"章节最新

## 阶段 9：兜底行为（实在没事做时）

- [ ] Task 9.1: 重新通读 best/add.cpp, best/mul.cpp, best/div.cpp
  - [ ] 寻找之前忽略的优化点
- [ ] Task 9.2: 重新通读 archieve/cpp/moptm.cpp
  - [ ] 对比 fusion.cpp，确认是否有遗漏的优化
- [ ] Task 9.3: 探索 GMP 源码（若 VM 已装）
  - [ ] 学习其除法算法
- [ ] Task 9.4: 探索其他大整数库
  - [ ] Boost.Multiprecision、libtommath、OpenSSL BN
- [ ] Task 9.5: 不要停下，继续找事做

---

# Task Dependencies

- Task 0.* → 所有后续任务
- Task 1.* → Task 2.*（POC 完成才能 LC 验证）
- Task 2.2 (LC 拒绝) → 跳转 Task 5.*（方案 E）
- Task 2.3 (LC 接受) → Task 3.*（全函数覆盖）
- Task 3.* → Task 4.*（benchmark）
- Task 4.* 不达标 → Task 5.*（方案 E）
- Task 5.* 不达标 → Task 6.*（方案 A）
- Task 4.* 或 6.* 达标 → Task 7.*（LC 提交）
- Task 7.* → Task 8.*（疯狂优化）
- Task 8.* 随时可触发 Task 9.*（兜底）

# 并行化机会

- Task 1.1（hot 函数识别）可与 Task 1.2（asm_extract.py）并行
- Task 3.2/3.3/3.4（ADD/MUL/DIV 三路径替换）可并行，但需先完成 Task 3.1
- Task 8.1（翻 E:\）与 Task 8.2（读论文）可并行
