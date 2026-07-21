# O3 源码级展开优化 Spec

## 跨压缩传承提示性资料（必读）

> 本章节为防止对话压缩导致上下文丢失而设。每次新会话或上下文被压缩时，先读本章节。

### 项目最终目标
在 LC (Library Checker, judge.yosupo.jp) 真实评测约束下 (`-O2 -std=gnu++20 -static -DONLINE_JUDGE`)，让单一文件 `fusion.cpp` 在 ADD/MUL/DIV 三题同时达到或超越 `best/` 目录下三个独立文件的成绩。

### 本 Spec 的核心命题
用户提出的技术路线：**用编译器参数（如 `-O3 -S`）生成"已应用 O3 优化"的新 cpp 文件**，该文件以纯 `-O2` 编译参与比拼。**禁止使用 `#pragma GCC optimize("O3")`**。

### 关键约束（硬性）
1. **参与比拼的 cpp 不能开 O3 pragma** — 这是用户明确的红线
2. **输出必须是 cpp 后缀文件**，不是 .s 或 .o
3. **LC 编译参数**：`g++ -x c++ -O2 -std=gnu++20 -static -DONLINE_JUDGE`（详见 limits.md）
4. **LC 源码限制**：≤ 256KB
5. **LC 编译时间限制**：≤ 30s
6. **VM 与 LC 的 g++ 版本差异**：VM 是 g++ 15.2.0，LC 是 g++ 11.4.0（Ubuntu）。GIMPLE dump 格式在版本间不完全兼容，**任何基于 dump 的方案必须在 LC 同款 g++ 11.4 上重新验证**
7. **不碰 div_opt.cpp**（项目硬约束）

### 工作区结构（2026-07-18）
```
d:\precious_speed\
├── best/              # 只读参考基线
│   ├── add.cpp        # 111352 字节, masonxiong 库, BASE=10^8
│   ├── mul.cpp        # 107309 字节, masonxiong 库
│   └── div.cpp        # 103259 字节, hint 库 (用户覆盖过)
├── archieve/          # 历史归档
│   └── cpp/moptm.cpp  # fusion.cpp 的基础
├── fusion.cpp         # 当前最优融合版（v2, pragma O3 + 64KB 查表）
├── HANDBOOK.MD        # 自我提醒手册（必读）
├── ssh_exec.py        # SSH 工具
├── ai.bat             # 命令批处理（用户要求所有命令放这里）
├── limits.md          # LC 环境清单
└── .trae/specs/o3-source-expansion/  # 本 spec
```

### VM 访问信息
- SSH: `10.144.33.157`, 用户 `azzr`, 密码 `1234`
- Ubuntu 7.0.0, g++ 15.2.0, Intel i7-11370H @ 3.30GHz (Tiger Lake, AVX2+FMA+AVX512)
- 工作目录约定：`/tmp/fusion/`
- SSH 工具用法：`python d:\precious_speed\ssh_exec.py "cmd1" "cmd2"` / `--file local remote` / `--get remote local`

### 当前 fusion.cpp 状态（v2, 基线）
- 基础：archieve/cpp/moptm.cpp + `#pragma GCC optimize("O3,unroll-loops")` + 64KB 查表法 parse
- 编译：LC 环境 -O2 无警告，3.7s
- 性能（VM g++ 15.2, LC 编译参数）：
  - ADD 1M+1M: 3.14ms (best 2.97ms, 落后 5.7%)
  - MUL 500k*500k: 8.31ms (best 10.82ms, 领先 23.2%)
  - DIV 1M/500k: 26.29ms (无 best 二进制)
- 正确性：全 PASS

### 历史教训（来自 project_memory）
1. **O3 pragma 对 archieve 版 moptm 效果不稳定**：DIV 200k 略快，DIV 1M/500k 反而慢 1.3ms
2. **moptm 代码已手工 8 路展开**：-O3 自动展开收益被指令缓存压力抵消
3. **fread 优化曾导致回退**：36.0ms → 38.8ms（Python pipeline + 16MB 静态数组缓存影响）
4. **PowerShell 有重定向问题**：用 C++/SYSTEM 函数或 Python 更可控
5. **LC 计分是 CPU 时间**：BSS 段零初始化不计入
6. **masonxiong AVX2 代码在 LC 默认不启用**：需 `#pragma GCC target("avx2")` 显式开启
7. **BASE=10^4 (hint) vs BASE=10^8 (masonxiong)**：limb 数翻 4 倍，ADD 路径有架构固有代价

### 调研结论（2026-07-18 子代理已完成）
- **GCC 无官方选项输出"O3 优化后的 C/C++ 源码"**
- `-fdump-tree-optimized` 输出是 debug 文本，不是合法 C++
- VM 无 GIMPLE→C 转换工具，无 clang/llc/llvm-cbe
- **方案 B（-O3 -S 汇编 + 内联 asm 嵌入）**：技术可行，2KB 汇编，但有平台风险
- **方案 E（针对性 GIMPLE 转换）**：工作量中等，只处理 hot 函数
- **方案 A（手动 intrinsics/unroll）**：最务实，作为兜底基线

### 善用子代理原则
- 大多数调研、benchmark、文件转换用 `general_purpose_task` 子代理
- 不要亲自上阵跑长命令
- 所有命令放 `ai.bat` 防止 IDE 阻塞

### 用户工作风格
- 中文交流，代码注释按最新指示（当前中文）
- 期望主动质疑提问（AskUserQuestion）
- 期望主动说"继续"推进任务
- 5 秒间隔提交 LC 避免违规
- 浏览器用 Firefox，遇验证码手动完成
- 工作区清理后再开始新融合任务
- 调试输出和临时文件保留，用途写入 README

---

## Why

当前 fusion v2 在 LC -O2 约束下：
- MUL 已领先 best 23%
- ADD 仍落后 best 5.7%（BASE=10^4 架构固有代价）
- DIV 无 best 对比但绝对值有优化空间

用户提出新思路：**用编译器在 O3 下生成"已展开优化"的源码文件**，让该文件在 O2 下编译时已固化了 O3 才会做的优化（循环展开、函数内联、向量化等），绕过 LC 不允许 O3 命令行参数的限制，且不使用 O3 pragma。

若该路径可行，可获得：
1. 一份"O3 等价但 O2 可编译"的 cpp，直接提交 LC
2. 验证"编译器预展开 + O2 再优化"路径的工程可行性
3. 为后续疯狂优化阶段提供新工具

## What Changes

### 主线：方案 B — `-O3 -S` 汇编内联嵌入
- 用 `g++ -O3 -S -std=gnu++20 fusion.cpp` 生成汇编
- 提取 hot 函数（absAdd/absSub/fftMul/fftSqr/str4toi/writeTo/fromCharRange 等）的汇编
- 删除 `.cfi_*`、`.p2align` 等无关指令
- 用 `asm volatile(...)` 包裹嵌入到新 cpp `fusion_o3expand.cpp`
- 非 hot 部分保留原 C++ 代码
- 该文件以纯 `-O2` 编译，asm 块原样保留

### 备线：方案 E — 针对性 GIMPLE 转换
- 用 `g++ -O3 -fno-tree-vectorize -fdump-tree-optimized=file.txt` 生成干净 dump
- 写 Python 脚本转换 hot 函数（只处理本项目特定代码，不追求通用）
- 处理 PHI 节点、SSA 重命名、标签重写
- 输出合法 C++ 嵌入新 cpp

### 兜底：方案 A — 手动 intrinsics/unroll
- 作为方案 B/E 失效时的基线
- 手工 AVX2 intrinsics + 循环展开
- 与 fusion v2 对比验证收益

### 版本管理
- 每个版本独立 cpp 文件（fusion_o3expand_v1.cpp, v2.cpp...）
- VM 上保留所有编译产物和 benchmark 日志
- 本地 git 提交每次大变更

### 疯狂优化阶段
- 方案验证通过后，持续优化直到：
  - 对话长度被 IDE 终止
  - 用户说"我醒来"
- 实在没事做时：
  - 翻看 E:\ 盘根目录
  - 自己找论文阅读（FFT、大整数除法、SIMD 等）
  - 不要停下

## Impact

- **Affected specs**: 无（本项目无其他 spec）
- **Affected code**:
  - 新增：`fusion_o3expand.cpp`（主线产物）
  - 新增：`tools/asm_extract.py`（汇编提取脚本，方案 B）
  - 新增：`tools/gimple2cpp.py`（GIMPLE 转换脚本，方案 E）
  - 新增：`ai.bat`（所有命令批处理）
  - 新增：`benchmark_o3expand.sh`（VM benchmark 脚本）
  - 不修改：`fusion.cpp`（保留作为 v2 基线对照）
  - 不修改：`best/*`（只读基线）

## ADDED Requirements

### Requirement: O3 源码展开产物
系统 SHALL 生成一个 `.cpp` 后缀的文件，该文件满足：
1. 内容为合法 C++20 源码
2. 不包含 `#pragma GCC optimize("O3")` 或任何 O3 级别的 pragma
3. 在 LC 编译参数 `g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE` 下能成功编译
4. 编译产物在 ADD/MUL/DIV 三题上正确性通过
5. 性能至少有一题超过 fusion v2 基线

#### Scenario: 方案 B 单函数 POC 成功
- **WHEN** 用 `-O3 -S` 生成 absAdd 函数汇编并嵌入 cpp
- **AND** 该 cpp 以 -O2 编译运行
- **THEN** absAdd 的运行时间应接近原 fusion v2 在 -O3 下的运行时间
- **AND** 正确性不变

#### Scenario: 方案 B 全函数覆盖
- **WHEN** 所有 hot 函数都用 asm 内联替换
- **AND** 以 -O2 编译
- **THEN** 整体性能应优于 fusion v2 在 -O2 下的性能
- **AND** 不劣于 fusion v2 在 -O3 下的性能

#### Scenario: LC 平台合规性
- **WHEN** 将 fusion_o3expand.cpp 提交 LC
- **THEN** 编译应通过（无 asm 块被拒）
- **AND** 运行结果正确
- **AND** 性能数据被记录

### Requirement: 版本管理与回归测试
系统 SHALL 维护版本管理：
1. 每个版本独立 cpp 文件
2. 每次变更后跑正确性测试（vs fusion v2 输出 diff）
3. 每次变更后跑 benchmark（vs best + fusion v2）
4. VM 上保留所有历史产物

#### Scenario: 版本回滚
- **WHEN** 新版本性能回退
- **THEN** 应能立即回滚到上一版本
- **AND** 记录回退原因

### Requirement: 持续优化不停歇
系统 SHALL 在主任务完成后继续优化：
1. 翻看 E:\ 盘根目录寻找参考资料
2. 阅读相关论文（FFT、大整数、SIMD）
3. 尝试新的优化思路
4. 直到对话被 IDE 终止或用户说"我醒来"

#### Scenario: 主任务完成后的行为
- **WHEN** fusion_o3expand.cpp 已通过 LC 提交并获得成绩
- **THEN** 应继续探索新的优化方向
- **AND** 不应停下等待用户

## MODIFIED Requirements

### Requirement: 工作流约束
所有命令 SHALL 放入 `ai.bat` 执行，防止 IDE 阻塞。
- 长命令用 `start /b` 后台执行
- 子代理优先，不亲自上阵跑长命令
- SSH 操作通过 `ssh_exec.py`

## REMOVED Requirements

### Requirement: 使用 O3 pragma
**Reason**: 用户明确禁止参与比拼的程序开 O3 优化
**Migration**: fusion.cpp v2 保留作为历史基线，新文件不带 O3 pragma

---

## 风险与对策

### 风险 1：LC 平台禁用 inline asm
- **概率**：中（需实测）
- **对策**：先用方案 B 做单函数 POC，本地验证后立即提交 LC 测试
- **降级**：若 LC 禁用 asm，切换方案 E 或方案 A

### 风险 2：GIMPLE dump 跨 g++ 版本不兼容
- **概率**：高（VM 15.2 vs LC 11.4）
- **对策**：方案 E 必须在 LC 同款 g++ 11.4 上重新生成 dump
- **降级**：在 VM 上装 g++ 11.4 双版本

### 风险 3：汇编大小超 256KB
- **概率**：低（单函数 ~2KB，全 hot 函数 ~50KB）
- **对策**：定期检查文件大小
- **降级**：只优化最 hot 的 3-5 个函数

### 风险 4：内联 asm 寄存器 clobber 错误
- **概率**：中
- **对策**：每个 asm 块单独测试正确性
- **降级**：用 `__attribute__((naked))` 函数替代

### 风险 5：方案 B 性能不如预期
- **概率**：中（O3 的自动向量化可能不如手动 intrinsics）
- **对策**：保留方案 A 作为兜底
- **降级**：切换方案 A

### 风险 6：用户睡觉期间对话被压缩
- **概率**：高
- **对策**：本 spec 的"跨压缩传承提示性资料"章节 + HANDBOOK.MD 持续更新
