# O3 源码级预展开优化 Spec

## 跨压缩传承提示性资料（必读）

> 本章节为防止对话压缩导致上下文丢失而设。每次新会话或上下文被压缩时，先读本章节。
> **最后更新**：2026-07-25

### 项目最终目标
在 LC (Library Checker, judge.yosupo.jp) 真实评测约束下，让 `add.cpp`/`mul.cpp`/`div.cpp` 在 ADD/MUL/DIV 三题上保持或超越当前 `best/` 快照（宝座，16/36/87ms）。

### O2 周期成果（已完成）
三题全部 #1：ADD 16ms / MUL 36ms (tied) / DIV 87ms。
- `best/` 已易位为本项目代码（宝座），易位根据 LC 实测排名判断。
- 技术栈：hint 库（radix-4 auto-vec FFT）+ BASE=10^4 + fread/oBuffer。

### 本 Spec 的核心命题
LC 实测：O2 和 `#pragma GCC optimize("O3")` 效果一致。推测是 LC 编译器禁用了 O3 优化（就像洛谷）。

仍然允许在本地测试使用 O3 选项。

用户有一个叫"GCC 修改版"的工具，能够保证 O3 预展开的提交效果。**O3 开发周期基本结束后，再去询问用户了解 GCC 修改版的进一步内容。当前不需要了解细节。**

### 关键约束（硬性）
1. **参与比拼的 cpp 不能开 O3 pragma** — 用户明确的红线
2. **输出必须是 cpp 后缀文件**，不是 .s 或 .o
3. **LC 编译参数**：`g++ -O2 -std=c++23 -DEVAL -DONLINE_JUDGE -march=native -o main main.cpp -I /opt/ac-library`（详见 docs/limits.md）
4. **LC 源码限制**：≤ 256KB
5. **LC 编译时间限制**：≤ 30s（需实测确认）
6. **GCC 版本**：LC 用 GCC 15.2（AMD EPYC 7B13 Milan/Zen3）

### 工作区结构（2026-07-25 整理后）
```
d:\precious_speed\
├── add.cpp / mul.cpp / div.cpp  # O2 三题成品（#1，best/ 快照源）
├── best/              # 当前 #1 快照（宝座），16/36/87ms
├── HANDBOOK.MD        # 项目总览（必读）
├── O3_HANDOVER.md     # O3 周期交接文档（必读）
├── ai.bat             # 命令集中文件
├── docs/              # limits.md + O3_DEV_LOG.md + GMP_DEV.md + GMP_INVERTAPPR_SOURCE.md
├── asm/               # 汇编对比（O2/O3/NOVEC/PRG，O3 预展开验证用）
├── gmp_index/         # GMP 源码阅读索引（DIV 移植参考）
├── archieve/          # O2 周期归档
├── toolbox/           # tbx 工具箱
└── .trae/specs/o3-source-expansion/  # 本 spec
```

### VM 访问信息
- SSH: `192.168.1.55`（本地 VM，凭据已移除）
- Ubuntu 26.04, g++ 11.5, Intel i7-11370H @ 3.30GHz (Tiger Lake, AVX2+FMA+AVX512)
- 注意：VM 的 g++ 版本（11.5）与 LC（15.2）不同，本地测速仅供参考

### 历史教训
1. **LC 实测 O2 和 pragma O3 效果一致**，推测 LC 编译器禁用了 O3 优化（像洛谷）
2. **LC 硬件是 AMD EPYC 7B13 (Milan/Zen3)**，不是 Intel；有 -march=native，无需手动 #pragma target
3. **-std=c++23 不是 gnu++23**（需实测确认 GNU 扩展是否可用）
4. **PowerShell 有重定向问题**：用 C++/SYSTEM 函数更可控
5. **LC 计分是 CPU 时间**（用户态+内核态）

### 善用子代理原则
- 大多数调研、benchmark、文件转换用 `general_purpose_task` 子代理
- 不要亲自上阵跑长命令
- 所有命令放 `ai.bat` 防止 IDE 阻塞

### 用户工作风格
- 中文交流，代码注释按最新指示
- 期望主动质疑提问（AskUserQuestion），完成后 ASK 而非终止对话
- 期望主动说"继续"推进任务
- 5 秒间隔提交 LC 避免违规
- 浏览器用 Firefox，遇验证码手动完成
- 工作区清理后再开始新任务
- 调试输出和临时文件保留，用途写入 README

---

## Why

O2 周期三题已全部 #1，但用户既定路线是"先开发 O2 到极限，再开发 O3"。LC 上 pragma O3 无效（与 O2 效果一致），因此使用 GCC 修改版进行 O3 预展开。

## What Changes

### 主线：O3 预展开开发
- 使用 GCC 修改版生成预展开 cpp（细节待开发周期结束后询问用户）
- 提交 LC 验证实际增益

### 版本管理
- 每个版本独立 cpp 文件
- VM 上保留所有编译产物和 benchmark 日志
- 本地 git 提交每次大变更

## Impact

- **Affected specs**: 无（本项目无其他 spec）
- **Affected code**:
  - 新增：预展开产物 cpp（主线产物）
  - 不修改：`add.cpp`/`mul.cpp`/`div.cpp`（保留作为 O2 基线对照）
  - 不修改：`best/*`（宝座快照）

## ADDED Requirements

### Requirement: O3 预展开产物
系统 SHALL 生成一个 `.cpp` 后缀的文件，该文件满足：
1. 内容为合法 C++20/23 源码
2. 不包含 `#pragma GCC optimize("O3")` 或任何 O3 级别的 pragma
3. 在 LC 编译参数 `g++ -O2 -std=c++23 -DEVAL -DONLINE_JUDGE -march=native` 下能成功编译
4. 编译产物在 ADD/MUL/DIV 三题上正确性通过
5. 文件大小 ≤ 256KB
6. 性能至少有一题超过 O2 基线（当前 best/ 快照）

#### Scenario: LC 平台合规性
- **WHEN** 将预展开 cpp 提交 LC
- **THEN** 编译应通过
- **AND** 运行结果正确
- **AND** 性能数据被记录

### Requirement: 版本管理与回归测试
系统 SHALL 维护版本管理：
1. 每个版本独立 cpp 文件
2. 每次变更后跑正确性测试（vs O2 基线输出 diff）
3. 每次变更后跑 benchmark（vs best/ 快照）
4. VM 上保留所有历史产物

#### Scenario: 版本回滚
- **WHEN** 新版本性能回退
- **THEN** 应能立即回滚到上一版本
- **AND** 记录回退原因

## REMOVED Requirements

### Requirement: 使用 O3 pragma
**Reason**: LC 实测与 O2 效果一致（LC 疑似禁用 O3 优化），且用户明确禁止参与比拼的程序开 O3 pragma
**Migration**: O2 基线 cpp 保留，新预展开文件不带 O3 pragma

---

## 风险与对策

### 风险 1：O3 预展开增益不明显
- **概率**：中
- **对策**：先做 ADD 验证，有增益再扩展到 MUL/DIV
- **降级**：如果增益 <2ms，项目作为学术性试验封存

### 风险 2：用户睡觉期间对话被压缩
- **概率**：高
- **对策**：本 spec 的"跨压缩传承提示性资料"章节 + HANDBOOK.MD + O3_HANDOVER.md 持续更新
