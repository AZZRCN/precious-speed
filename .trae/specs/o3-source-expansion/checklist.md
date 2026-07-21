# Checklist - O3 源码级展开优化

## 阶段 0：准备工作
- [ ] `ai.bat` 文件已创建，包含 SSH 调用模板
- [ ] VM 上 `/tmp/o3expand/` 目录已建立，包含 fusion.cpp 和 best/*
- [ ] VM 上已生成 `fusion_O3.s` 和 `fusion_O2.s`，差异已记录

## 阶段 1：方案 B 单函数 POC
- [ ] hot 函数清单已识别（top 5，含耗时占比）
- [ ] `tools/asm_extract.py` 脚本已编写，能正确提取单个函数汇编
- [ ] absAdd 的 asm 块已嵌入 `fusion_o3expand_v1.cpp`
- [ ] `fusion_o3expand_v1.cpp` 在 -O2 下编译通过，无警告
- [ ] POC 正确性测试通过（输出 diff vs fusion v2 为空）
- [ ] POC benchmark 已记录，absAdd 路径性能接近 fusion v2 -O3

## 阶段 2：方案 B LC 平台合规性
- [ ] `fusion_o3expand_v1.cpp` 已提交 LC ADD 题目
- [ ] LC 返回结果已记录（AC/CE/RE）
- [ ] 若 AC：LC 真实 CPU 时间已记录
- [ ] 若 CE：拒绝原因已记录，决策是否切换方案 E

## 阶段 3：方案 B 全函数覆盖
- [ ] asm_extract.py 已扩展支持批量函数提取
- [ ] ADD 路径所有 hot 函数已替换为 asm 版本
- [ ] MUL 路径所有 hot 函数已替换为 asm 版本
- [ ] DIV 路径所有 hot 函数已替换为 asm 版本
- [ ] `fusion_o3expand_v2.cpp` 文件大小 ≤ 256KB
- [ ] 完整正确性测试通过（ADD/MUL/DIV 各 5 组）

## 阶段 4：方案 B 性能 benchmark
- [ ] 完整 benchmark 矩阵已执行（8 个测试用例 × 4 个版本）
- [ ] 性能数据已记录到 HANDBOOK.MD
- [ ] 若性能达标（至少一题超 fusion v2），进入阶段 7
- [ ] 若性能不达标，已分析瓶颈并决策是否进入阶段 5

## 阶段 5（备选）：方案 E — GIMPLE 转换
- [ ] VM 上已安装 g++-11 双版本
- [ ] g++-11 生成的 GIMPLE dump 已分析
- [ ] `tools/gimple2cpp.py` 脚本已编写
- [ ] 至少一个 hot 函数已成功转换为合法 C++
- [ ] 转换后函数正确性测试通过

## 阶段 6（兜底）：方案 A — 手动 intrinsics
- [ ] 可手动向量化的循环已识别
- [ ] AVX2 intrinsics 版本的 absAdd/absSub 已编写
- [ ] `fusion_manual_avx2.cpp` 正确性测试通过

## 阶段 7：LC 三模式提交
- [ ] ADD 模式已提交，Record ID 已记录
- [ ] MUL 模式已提交，Record ID 已记录
- [ ] DIV 模式已提交，Record ID 已记录
- [ ] 三题成绩已汇总对比 best LC 历史

## 阶段 8：疯狂优化
- [ ] E:\ 盘根目录已翻看，发现的可借鉴内容已记录
- [ ] 至少 1 篇论文已阅读，可借鉴思路已记录
- [ ] 至少 3 个新优化思路已尝试（无论成败）
- [ ] HANDBOOK.MD 已更新到最新状态

## 全局质量门
- [ ] 所有 cpp 文件均无 O3 pragma
- [ ] 所有 cpp 文件均在 LC 编译参数下成功编译
- [ ] 所有版本均有独立文件（不覆盖历史版本）
- [ ] VM 上保留所有编译产物和 benchmark 日志
- [ ] git 提交历史清晰，每个里程碑有 commit
- [ ] HANDBOOK.MD 持续更新，包含所有决策和结果

## 跨压缩传承
- [ ] spec.md 的"跨压缩传承提示性资料"章节已包含所有关键信息
- [ ] HANDBOOK.MD 与 spec.md 不冲突
- [ ] 任何重大决策都已记录在两个文件中
