# O3 开发周期交接文档

> **创建**：2026-07-23
> **用途**：O2 周期结束（三题 #1），O3 预展开周期开始。本文档供下一个 AI session 快速进入状态。
> **配套文件**：`HANDBOOK.MD`（项目总览）、`docs/limits.md`（LC 真实编译环境）、`docs/O3_DEV_LOG.md`（O3 开发日志）

---

## 0. 给下一个 AI 的开场提示词

```
你是 precious_speed 项目的延续 AI。这是一个 LC (Library Checker) 大整数 ADD/MUL/DIV 三题的性能优化项目。

当前状态：O2 周期已结束，三题全部 #1（ADD 16ms / MUL 36ms / DIV 87ms）。现在进入 O3 预展开周期。

开始前必读：
1. O3_HANDOVER.md（本文件）—— O3 周期交接
2. HANDBOOK.MD —— 项目总览和通用规则
3. docs/limits.md —— LC 真实编译环境（2026-07-23 用 langs.toml 一手数据查证，旧文档已废弃）
4. docs/O3_DEV_LOG.md —— O3 开发日志

关键事实：
- LC 编译：g++ 15.2 -O2 -std=c++23 -march=native（AMD EPYC 7B13 Milan/Zen3）
- #pragma GCC optimize("O3") 在 LC 上增益 <1ms（噪声级，用户实测）
- O3 预展开是唯一能拿到 O3 级优化的路径

工作方式：
- 自驱动模式：你主动提出想法和方案，用户审核
- 所有命令放 ai.bat 防止 IDE 阻塞
- 每优化一次立即 fuzz 验证
- 完成任何任务后用 AskUserQuestion 询问下一步，不要主动终止对话
- 不要用夸赞语气（除非明确要求）
- 反复推敲，优先保证准确度
- 必要时主动质疑或提问

现在请读完上述文件，然后 AskUserQuestion 询问用户今天要做什么。
```

---

## 1. 项目概述

在 LC 真实评测约束下，让单一文件在 ADD/MUL/DIV 三个问题上达到或超越 `best/` 目录下记录的当前 #1 快照成绩。`best/` 是"宝座"，存放当前 LC #1 代码快照，易位根据 LC 实测排名判断。

### 三题地址
- ADD: https://judge.yosupo.jp/problem/addition_of_big_integers
- MUL: https://judge.yosupo.jp/problem/multiplication_of_big_integers
- DIV: https://judge.yosupo.jp/problem/division_of_big_integers

### GitHub 仓库
https://github.com/AZZRCN/precious-speed

---

## 2. O2 周期成果（已结束）

| 题目 | 提交ID | 耗时 | 排名 |
|---|---|---|---|
| ADD | #387304 | **16 ms** | **#1** |
| MUL | #387374 | **36 ms** | **#1 (tied)** |
| DIV | #387301 | **87 ms** | **#1** |

**O2 三题全部 #1。** `best/` 已易位为本项目代码（宝座）。

### 核心文件
- `add.cpp` (~4600行) — ADD 优化完成
- `mul.cpp` (~4800行) — basicMul 已重写为 BASE=10^8 打包版本
- `div.cpp` (~4600行) — DIV 优化完成，inv precision boost 消除 basicMul fallback
- `best/` — 当前 #1 快照（宝座），当前内容即本项目 add/mul/div（16/36/87ms），易位根据 LC 实测排名判断

### 技术栈
- hint 库（radix-4 auto-vec FFT，无 splitting，dedicated square path，thread_local buffer）
- BASE=10^4
- fread + oBuffer I/O
- AlignedVec32 twiddle table + `__builtin_assume_aligned`

---

## 3. O3 预展开周期目标

### 3.1 背景

用户既定路线："先开发 O2，O2 到极限之后再开发 O3"。

LC 实测：O2 和 `#pragma GCC optimize("O3")` 效果一致。推测是 LC 编译器禁用了 O3 优化（就像洛谷）。

仍然允许在本地测试使用 O3 选项。

### 3.2 GCC 修改版

用户有一个叫"GCC 修改版"的工具，能够保证 O3 预展开的提交效果。

**O3 开发周期基本结束后，再去询问用户了解 GCC 修改版的进一步内容。当前不需要了解细节。**

---

## 4. 工作习惯和约束

### 4.1 开发流程
- **自驱动模式**：AI 主动提出想法和方案，用户审核
- 所有命令放 `ai.bat` 防止 IDE 阻塞
- 每优化一次立即 fuzz 验证
- 使用 GIT 进行版本管理
- 完成任何任务后用 AskUserQuestion 询问下一步，**不要主动终止对话**
- 用户说"继续"即继续推进任务

### 4.2 代码约束
- O3 周期基于当前 `add.cpp`/`mul.cpp`/`div.cpp`（O2 #1 成品，即 `best/` 快照源）
- BASE=10^4（无需 BASE 转换）
- I/O 必须用 fread + oBuffer
- hint library 统一基库
- 程序必须包含 5 秒超时机制，避免阻塞命令行
- 代码注释语言：跟随用户最新指令（中文或全英文）
- 编译器参数式代码生成（如 O3 优化段）必须产出 CPP 文件（不是 EXE）

### 4.4 沟通方式
- 不要用夸赞语气（除非明确要求）
- 反复推敲，优先保证准确度
- 答案不一定正确，用户判断也不一定正确
- 必要时主动质疑或提问
- 完成后 ASK，而不是终止对话
- 避免重复操作，密切跟踪步骤

### 4.5 工具箱
- `d:\precious_speed\toolbox\tbx.exe` — 用户自研工具箱
- 命令：sam（SAM 后缀自动机搜索）、es（Everything HTTP）、split、glob、tree、head、lines、hex、wc
- **优先使用 tbx 工具而非 TRAE 自带工具**（自带 Glob 对工作区外路径失效）

### 4.6 编译测试环境
- **VM**：Ubuntu 26.04, g++ 11.5, Tiger Lake i7-11370H, IP 192.168.1.55
- **LC**：GCC 15.2, AMD EPYC 7B13 (Milan/Zen3), 1 core, 1 GiB
- VM 和 LC 微架构不同，本地测速仅供参考
- 代码提交 LC 需至少 5 秒间隔，避免违规检测
- 遇到验证码手动完成

---

## 5. 隐私安全提醒（重要）

### 5.1 VM 凭据泄漏历史

**2026-07-23 发现**：`ssh_manager.py` 和 100+ 个 `scripts/*.py` 硬编码了 VM 凭据（HOST/USER/PWD）。

这些文件在 2026-07-17 的 force-push（90 文件）中**可能已推送到 GitHub 公开仓库**。

### 5.2 已采取措施

本次 commit：
- `ssh_manager.py` 和 `scripts/` 整个目录从 git tracking 移除
- `.gitignore` 加入隐私文件排除规则
- `winbench/`（大测试数据）、`temp/`（临时文件）、`exploration/`（无关项目）也移除

### 5.3 待办（建议）

- **修改 VM 密码**（凭据已泄漏）
- 考虑用 `git filter-branch` 或 BFG 清除历史中的凭据（可选，但会改写历史）
- GitHub 仓库设为 private（如果还不是）

---

## 6. 关键经验教训

- PowerShell 有重定向问题，用 C++ 和 SYSTEM 函数更可控
- 分块数组用向上取整除法
- carry chain 串行依赖是真正瓶颈，不是除法指令
- BASE=10^4 + radix-4 auto-vec FFT (hint) 比 BASE=10^8 + radix-2 AVX2 split FFT (masonxiong) 快
- hint 计算速度优势比观测更大（因为其 I/O cin/cout 比 masonxiong 的 mmap+oBuffer 慢）
- fread 优化曾导致性能回归（36.0ms → 38.8ms），Python 管线问题和 16MB 静态数组缓存影响
- 递归调用会 clobber memory，阻止向量化
- AlignedVec32 twiddle table + `__builtin_assume_aligned` 启用对齐 load/store 指令
- o3 pragma 提交无效（LC 上增益 <1ms）
- 旧 HANDBOOK 记录"O2 下 moptm 三题全胜"曾因测试数据格式 bug 错误（missing count line '1' 导致 size_t overflow SIGSEGV）

---

## 7. O3 周期待办

1. O3 预展开开发（使用 GCC 修改版，细节待开发周期结束后询问用户）
2. 提交 LC 验证实际增益
3. 更新 HANDBOOK.MD 记录 O3 进度

### 待实测验证项（来自 limits.md）
- `-std=c++23` 是否启用 GNU 扩展（`__int128`、语句表达式、`__builtin` 系列）
- AVX-512 在 Milan 256-bit 通路下的实际增益
- AC Library 1.6 包含哪些模块

---

## 8. 文件结构（O3 周期相关）

```
precious_speed/
├── O3_HANDOVER.md          # 本文件
├── HANDBOOK.MD             # 项目总览（已更新 LC 环境）
├── README.md
├── ai.bat                  # 命令集中文件
├── add.cpp / mul.cpp / div.cpp  # O2 三题成品（#1，当前 best/ 快照源）
├── .gitignore              # 隐私排除（ssh_manager.py/scripts/winbench/temp/shrunk_*）
│
├── best/                   # 当前 #1 快照（宝座），易位根据 LC 实测排名判断
│   ├── add.cpp / mul.cpp / div.cpp  # 16/36/87ms（O2 周期提交后易位）
│
├── docs/                   # O3 周期保留文档
│   ├── limits.md           # LC 真实编译环境（langs.toml 一手数据）
│   ├── O3_DEV_LOG.md       # O3 开发日志
│   ├── GMP_DEV.md          # GMP 移植开发记录
│   └── GMP_INVERTAPPR_SOURCE.md  # GMP 近似逆源码分析
│
├── asm/                    # 汇编对比（O2/O3/NOVEC/PRG，O3 预展开验证用）
├── gmp_index/              # GMP 源码阅读索引（DIV 移植 GMP 参考）
│
├── archieve/               # 归档（O2 周期 + 本周期整理归档）
│   ├── HANDOVER.md
│   ├── OPTIMIZATION_SUMMARY.md
│   ├── add_test.txt        # 旧测试文件
│   ├── cpp/                # 旧测试代码 + fast_io.cpp（I/O 参考，已归档）
│   ├── docs/               # O2 HANDBOOK + CLEANUP_LOG/FFT_MULMOD/GCC_BENCH/MOPTM_FUSION/VM_BENCH
│   └── scripts/            # 旧脚本（仍 tracked，含 VM 路径，待清理）
│
├── toolbox/                # tbx 工具箱源码
└── .trae/specs/o3-source-expansion/  # O3 预展开 spec
    ├── spec.md
    ├── checklist.md
    └── tasks.md
```

### 已从 git 移除（本地保留，untracked）
- `ssh_manager.py` — VM SSH 管理（含凭据）
- `scripts/` — 全部开发脚本（含 VM 凭据/路径）
- `winbench/` — 大测试数据
- `temp/` — 临时文件
- `exploration/` — 无关项目探索记录
- `shrunk_ADD_v4_ref.cpp` — AAMP 压缩产物参考（含 VM 路径，有价值，本地保留）

---

## 9. 下一步

读完本文档后，AskUserQuestion 询问用户：
1. 是否现在开始 GCC 15.2 移植？
2. 还是先做其他验证（如 -std=c++23 GNU 扩展测试）？
3. 还是有其他安排？

**不要主动终止对话。完成任何任务后都要 ASK。**
