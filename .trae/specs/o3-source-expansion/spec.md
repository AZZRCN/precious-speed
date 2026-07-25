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
**O3 预展开**：用修改版 GCC 的 `-fo3-pre-expand` 开关，将 O3 优化的 GIMPLE IR 转回合法 C++ 源码，该源码以 LC 的 O2 编译参数编译，获得 O3 级优化。

**为什么不用 `#pragma GCC optimize("O3")`**：用户实测在 LC 上增益 <1ms（噪声级），推测原因是 GCC 15.2 的 O2 已原生包含大量旧版本 O3 优化。O3 预展开是唯一能拿到 O3 级优化的路径。

### 关键约束（硬性）
1. **参与比拼的 cpp 不能开 O3 pragma** — 用户明确的红线
2. **输出必须是 cpp 后缀文件**，不是 .s 或 .o
3. **LC 编译参数**：`g++ -O2 -std=c++23 -DEVAL -DONLINE_JUDGE -march=native -o main main.cpp -I /opt/ac-library`（详见 docs/limits.md）
4. **LC 源码限制**：≤ 256KB
5. **LC 编译时间限制**：≤ 30s（需实测确认）
6. **GCC 版本**：LC 用 GCC 15.2（AMD EPYC 7B13 Milan/Zen3）。修改版 GCC 当前基于 11.4.0，需移植到 15.2。

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

### O3 预展开技术架构（GCC MODIFIELD 项目）
三大组件：
1. **GCC 修改版**：基于 GCC 11.4.0 源码，新增 `-fo3-pre-expand` 开关。需移植到 GCC 15.2。
2. **GIMPLE-to-C++ emitter**：将 GIMPLE IR（PHI 节点、SSA 名、CFG）转回合法 C++20 源码。产出 670KB 的 `moptm_pre_ADD.cpp`。
3. **AAMP (Automatic Association-Mining of Patterns)**：自主压缩算法，发现可复用模式并替换，含权衡回退。670KB → 161KB。

AAMP 动机：LC 源码限制 256KB，670KB 交不上去 + 避免作弊嫌疑。

参考文件：
- `shrunk_ADD_v4_ref.cpp`（untracked，含 VM 路径，AAMP 压缩产物参考）
- `D:\gcc modifield\moptm_pre_ADD.cpp`（670KB，GIMPLE-TO-CPP 产出，**最多查看两次**）

### 历史教训
1. **pragma O3 在 LC 上增益 <1ms**（噪声级，用户实测）
2. **GCC 15.2 的 O2 已包含大量旧 O3 优化**（推测 pragma 无效的原因）
3. **GIMPLE IR 跨版本不完全兼容**，但 GIMPLE-to-C++ 产出的是合法 C++ 源码，版本错配只影响"O3 pass 集合"，不影响"源码能否编译"
4. **LC 硬件是 AMD EPYC 7B13 (Milan/Zen3)**，不是 Intel；有 -march=native，无需手动 #pragma target
5. **-std=c++23 不是 gnu++23**（需实测确认 GNU 扩展是否可用）
6. **PowerShell 有重定向问题**：用 C++/SYSTEM 函数更可控
7. **LC 计分是 CPU 时间**（用户态+内核态）

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

O2 周期三题已全部 #1，但用户既定路线是"先开发 O2 到极限，再开发 O3"。pragma O3 在 LC 上无效（<1ms 增益），因此 O3 预展开是唯一路径——把 O3 GIMPLE IR 回转成 C++ 源码，再用 LC 的 O2 编译。

若该路径可行，可获得：
1. 一份"O3 等价但 O2 可编译"的 cpp，直接提交 LC
2. 验证"编译器预展开 + O2 再优化"路径的工程可行性
3. GCC MODIFIELD 工具链（修改版 GCC + GIMPLE-to-C++ + AAMP）作为独立工程资产

## What Changes

### 主线：GCC MODIFIELD 移植到 GCC 15.2
- 用 diff 扫 GCC 11.4 vs 15.2 的 O3 pass 差异
- 将 `-fo3-pre-expand` 开关移植到 GCC 15.2 源码
- 用移植版 GCC 15.2 重新生成预展开 cpp
- AAMP 压缩到 256KB 以内
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
5. 文件大小 ≤ 256KB（AAMP 压缩后）
6. 性能至少有一题超过 O2 基线（当前 best/ 快照）

#### Scenario: GCC 15.2 移植成功
- **WHEN** `-fo3-pre-expand` 开关移植到 GCC 15.2
- **AND** 用移植版 GCC 15.2 对 add.cpp 生成预展开 cpp
- **THEN** 预展开 cpp 应能以 -O2 成功编译
- **AND** 正确性不变

#### Scenario: AAMP 压缩成功
- **WHEN** 预展开 cpp（~670KB）经 AAMP 压缩
- **THEN** 压缩后文件 ≤ 256KB
- **AND** 压缩后 cpp 仍能正确编译运行

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
**Reason**: 用户实测在 LC 上增益 <1ms（噪声级），且用户明确禁止参与比拼的程序开 O3 pragma
**Migration**: O2 基线 cpp 保留，新预展开文件不带 O3 pragma

### Requirement: 方案 B（汇编内联嵌入）/ 方案 E（GIMPLE 脚本转换）/ 方案 A（手动 intrinsics）
**Reason**: 用户已确定 GCC MODIFIELD（修改版 GCC + GIMPLE-to-C++ emitter）路线，旧方案不再作为主线
**Migration**: 旧方案描述保留在 git 历史中

---

## 风险与对策

### 风险 1：GCC 15.2 移植工作量超预期
- **概率**：中
- **对策**：用 diff 扫 11.4 vs 15.2 的 O3 pass 差异，用户判断"GIMPLE 回转应该没啥影响"
- **降级**：接受 11.4 预展开 + 15.2 O2 编译的版本错配，赌 15.2 不会反优化

### 风险 2：预展开 cpp 编译超时（30s）
- **概率**：中（670KB 大文件）
- **对策**：AAMP 压缩后文件更小，编译更快
- **降级**：只预展开 hot 函数，非 hot 部分保留原 C++

### 风险 3：AAMP 压缩后超 256KB
- **概率**：低（670KB → 161KB 历史已验证）
- **对策**：AAMP 算法能力可压缩更多内容
- **降级**：只预展开 ADD，MUL/DIV 暂不做

### 风险 4：O3 预展开增益被 GCC 15.2 O2 吃掉
- **概率**：中（15.2 的 O2 已包含大量旧 O3 优化）
- **对策**：先做 ADD 验证，有增益再扩展到 MUL/DIV
- **降级**：如果增益 <2ms，项目作为学术性试验封存

### 风险 5：用户睡觉期间对话被压缩
- **概率**：高
- **对策**：本 spec 的"跨压缩传承提示性资料"章节 + HANDBOOK.MD + O3_HANDOVER.md 持续更新
