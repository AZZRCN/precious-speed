# Library Checker (judge.yosupo.jp) C++ 真实编译环境

> **数据来源**：yosupo06/library-checker-judge 仓库官方 `langs/langs.toml`（一手资料）+ LC 官网 `/help` 页面
> **查证日期**：2026-07-23
> **历史**：本文件原由豆包 AI 编写，含大量错误（GCC 11.4、Cascade Lake、-static、无 -march=native 等），已全部废弃并用 langs.toml 一手数据重写。原 `docs/limtis2.md`（豆包交叉编译问答）已删除。

---

## 1. C++ 编译选项（langs.toml 官方源）

### cpp（C++23，默认）
```toml
id = "cpp"
name = "C++23"
version = "GCC 15.2 + AC Library(1.6)"
source = "main.cpp"
image_name = "library-checker-images-gcc"
compile = ["g++", "-O2", "-std=c++23", "-DEVAL", "-DONLINE_JUDGE", "-march=native", "-o", "main", "main.cpp", "-I", "/opt/ac-library"]
exec = ["./main"]
```

### cpp20（C++20）
```toml
id = "cpp20"
name = "C++20"
version = "GCC 15.2 + AC Library(1.6)"
compile = ["g++", "-O2", "-std=c++20", "-DEVAL", "-DONLINE_JUDGE", "-march=native", "-o", "main", "main.cpp", "-I", "/opt/ac-library"]
```

### cpp17 / cpp-func
同理，`-std=c++17` / `cpp-func`（带 grader.cpp + fastio.h + solve.hpp）。

---

## 2. 关键编译参数解读

| 参数 | 值 | 说明 |
| --- | --- | --- |
| 编译器 | **GCC 15.2** | 不是 11.4！豆包旧文档严重过时 |
| 优化级别 | **-O2** | 确认是 O2，不是 O3 |
| 标准 | **-std=c++23**（或 c++20/c++17） | 注意是 `c++23` 不是 `gnu++23`（严格 ISO 模式，无 GNU 扩展？需实测验证 `__int128` 是否可用） |
| 静态链接 | **无 -static** | 豆包旧文档错误的写了 -static，实际没有 |
| 架构 | **-march=native** | 关键！会针对评测机 CPU 自动启用全部可用指令集 |
| 预定义宏 | **-DEVAL -DONLINE_JUDGE** | 两个宏都定义（题目 checker 用 EVAL） |
| AC Library | **-I /opt/ac-library** | AtCoder Library 可用（1.6 版本） |
| 警告 | **无 -Wall/-Wextra** | 警告不影响编译 |
| LTO | **未启用** | 命令行无 -flto；可用 `#pragma GCC optimize("lto")` 显式开 |

---

## 3. 评测机硬件（LC /help 页面）

| 项 | 值 |
| --- | --- |
| 云平台机型 | **GCP c2d-highcpu-8** |
| CPU | **AMD EPYC™ 7B13**（Milan / Zen 3） |
| 核心限制 | **1 core**（单核调度） |
| 内存 | **1 GiB** |
| 栈大小 | **Unlimited** |

### AMD EPYC 7B13 (Milan / Zen 3) 指令集

- **AVX2** + **FMA** + **BMI1/BMI2** + **POPCNT** + **LZCNT** + **ADX** + **AES-NI**
- **AVX-512**: AVX512F/DQ/CD/BW/VL/IFMA/VNNI（Milan 全量支持，但数据通路 256-bit，AVX-512 指令双发射）
- **SSE** 全系列

### 与豆包旧文档的差异（重要）

| 项 | 豆包旧文档（错） | 实际（对） |
| --- | --- | --- |
| 机型 | c2-standard-4 | c2d-highcpu-8 |
| CPU | Intel Xeon Cascade Lake | AMD EPYC 7B13 (Milan/Zen3) |
| 内存 | 2GB | 1 GiB |
| -march=native | 无（需手动 #pragma target） | **有**（自动启用全部指令集） |
| -static | 有 | **无** |
| 指令集启用方式 | `#pragma GCC target("arch=cascadelake")` | 不需要！-march=native 已自动启用 |

---

## 4. 微架构影响（Zen 3 vs Cascade Lake）

豆包旧文档假设是 Intel Cascade Lake，实际是 AMD Zen 3。两者微架构差异：

| 项 | Cascade Lake (Intel) | Milan / Zen 3 (AMD) |
| --- | --- | --- |
| AVX-512 数据通路 | 真 512-bit（但降频） | 256-bit（双发射执行 512-bit 指令） |
| AVX-512 降频 | 有（AVX-512 heavy 降频明显） | 无（Zen 3 不因 AVX-512 降频） |
| 端口/吞吐 | 不同 | 不同 |
| 向量化偏好 | 256-bit AVX2 通常够用 | AVX-512 256-bit 实际性能跟 AVX2 接近 |

**实践含义**：
- `-march=native` 已启用，代码无需 `#pragma GCC target` 即可用 `__AVX2__` `__AVX512F__` `__BMI2__` `__FMA__` 宏
- 手写 SIMD 时优先 AVX2 (256-bit)，AVX-512 增益可能有限（Zen 3 256-bit 通路）
- 本地 VM（Tiger Lake i7-11370H）微架构跟 LC (Milan) 不同，向量化/指令延迟/端口吞吐有差异，本地测速仅供参考

---

## 5. 源码与运行限制

| 项 | 值 | 来源 |
| --- | --- | --- |
| 源码大小 | **256 KB** | LC 规则（豆包旧文档此条正确） |
| 编译时长 | 未在 langs.toml 明示，社区传闻 30s | 需实测 |
| 输入输出 | stdin/stdout | LC 规则 |
| 文件读写 | 禁止 | LC 规则 |
| 时间统计 | CPU 时间（用户态+内核态） | LC 规则 |
| 时间限制 | 题目独立设置 | LC 题面 |
| 栈空间 | Unlimited | LC /help 页面确认 |
| 内存 | 1 GiB（含栈/堆/代码段） | LC /help 页面确认 |
| 单核 | 1 core | LC /help 页面确认 |
| 进程数上限 | 未在 langs.toml 明示 | 需实测 |

---

## 6. 对 O3 预展开项目的影响

### 6.1 好消息：LC 确实是 O2

`-O2` 在 langs.toml 中确认。O3 预展开项目方向正确——把 O3 GIMPLE IR 回转成 C++ 源码，再用 LC 的 O2 编译，理论上可获得 O3 级别优化。

### 6.2 坏消息：GCC 版本错配

- 你的修改版 GCC：**11.4.0**（加 `-fo3-pre-expand` 开关）
- LC 实际编译器：**GCC 15.2**

GIMPLE IR 在版本间不完全兼容。但用户判断"GIMPLE 回转应该没啥影响"——因为 GIMPLE-TO-CPP 产出的是合法 C++20 源码（不是 GIMPLE IR 本身），所以版本错配只影响"O3 优化的具体 pass 集合"，不影响"C++ 源码能否在 15.2 编译"。

**风险**：GCC 15.2 的 O2 可能已原生包含 11.4 O3 的部分优化（loop interchange、SLSR、if-conversion 等在 12-14 版本持续改进），导致预展开增益被吃掉。

**对策**：用户决定把 `-fo3-pre-expand` 移植到 GCC 15.2（用 diff 扫 11.4 vs 15.2 的 O3 pass 差异，工作量可控）。

### 6.3 关键变量：-march=native

LC 有 `-march=native`（AMD EPYC 7B13 / Milan）。这意味着：
- AVX2/FMA/BMI2/AVX-512 自动启用，无需 `#pragma GCC target`
- 编译器会针对 Zen 3 微架构做指令调度
- 本地 VM（Tiger Lake）和 LC（Milan）的 `-march=native` 产出不同机器码，性能特征不同

### 6.4 pragma O3 无效结论需重新验证

豆包旧文档（2026-07-18）记录："LC 实测 pragma O3 无效，两次提交时间相同"。

**此结论基于错误前提**（豆包假设 GCC 11.4 + 无 -march=native）。实际 LC 是 GCC 15.2 + -march=native。在 GCC 15.2 下 `#pragma GCC optimize("O3")` 优先级仍高于命令行 -O2，理论应生效。

**需重新实测**：提交同一份代码，O2 基线 vs `#pragma GCC optimize("O3,unroll-loops")`，对比时间。如果 GCC 15.2 下 pragma O3 生效，则 O3 预展开项目的"必要性"下降（直接 pragma 即可）；如果仍无效，说明 LC 做了 pragma 过滤，O3 预展开仍是唯一路径。

---

## 7. 待实测验证项

1. **-std=c++23 是否启用 GNU 扩展**：`c++23` vs `gnu++23` 差异（`__int128`、语句表达式、`__builtin` 系列）
2. **pragma O3 在 GCC 15.2 下是否生效**：重新提交 addition_of_big_integers 对比
3. **AVX-512 实际增益**：Milan 256-bit 通路下 AVX-512 vs AVX2 性能对比
4. **编译时长上限**：670KB 预展开源码在 GCC 15.2 下编译是否超时
5. **进程数上限**：未在 langs.toml 明示
6. **AC Library 具体版本**：1.6 版本包含哪些模块

---

## 8. 数据源链接

- LC /help 页面：https://judge.yosupo.jp/help
- langs.toml 原文：https://github.com/yosupo06/library-checker-judge/blob/master/langs/langs.toml
- Dockerfile 目录：https://github.com/yosupo06/library-checker-judge/blob/master/langs/
- 仓库主页：https://github.com/yosupo06/library-checker-judge
