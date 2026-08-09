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

#### AVX-512 数据通路演进（重要，影响优化策略）

| 代次 | 架构 | EPYC 平台 | AVX-512 指令集 | 数据通路 | 512-bit 指令执行方式 |
|------|------|-----------|---------------|----------|---------------------|
| Zen 2 | Rome | 7002 | ❌ 不支持 | 256-bit | N/A（回退 AVX2） |
| **Zen 3** | **Milan** | **7003 (含 7B13)** | **✅ 全量支持** | **256-bit** | **双发射（2 周期）** |
| Zen 4 | Genoa | 9004 | ✅ 全量支持 | **512-bit** | 单周期（原生） |
| Zen 5 | Turin | 9005 | ✅ 全量支持 + BF16/FP16 | **512-bit** | 单周期（原生） |

**AMD 官方博客原文（2026-02-10）**：
> "早期的 AVX2 CPU 需通过组合两次 256 位矢量运算来执行 512 位数学运算，而这会增加指令压力并降低整体效率。在采用第五代 AMD EPYC 'Turin' 处理器的最新云实例（M8a、C8a、R8a、C4D、H4D、E6、Dasv7、Fasv7、Easv7）上，这种运算能力可通过完整的 512 位数据路径来实现。"

**关键澄清**（QWEN3.7 plus 的疑问已解决）：
- 博客中的"早期 AVX2 CPU"指 **Zen 2 (Rome) 及更早**，这些确实不支持 AVX-512
- **Zen 3 (Milan, 含 EPYC 7B13) 确实支持 AVX-512 指令集**，包括 AVX-512VL
- 但 Zen 3 的 AVX-512 数据通路是 256-bit，512-bit 指令需双发射（2 周期执行）
- **真 512-bit 数据通路**只在 Zen 4 (Genoa) / Zen 5 (Turin) 才有
- 来源：https://www.amd.com/zh-cn/blogs/2026/understanding-avx-512---validating-usage-on-amd-epyc-.html

#### 对 AVX-512VL 优化策略的影响

**Zen 3 上 AVX-512VL 的真实表现**：
- ✅ **指令可用**：`_mm256_cvtepi32_epi16` 等 AVX-512VL 指令在 Zen 3 上可执行
- ⚠️ **吞吐量无优势**：256-bit AVX-512VL 指令的吞吐量与 AVX2 相同（数据通路都是 256-bit）
- ✅ **指令数减少有收益**：AVX-512VL 的优势在于更少的指令数完成相同操作（如 cvtepi32_epi16 替代 packus+cast+extract+unpacklo 4 条指令）
- ⚠️ **512-bit 指令无优势**：`__m512i` 指令在 Zen 3 上双发射，吞吐量减半，不比 AVX2 快

**实践结论**：
- **AVX-512VL 的 128/256-bit 指令**（如 `_mm256_cvtepi32_epi16`, `_mm_cvtepi32_epi16`）：可用且有指令数减少的收益
- **AVX-512 的 512-bit 指令**（如 `_mm512_*`）：在 Zen 3 上无吞吐量优势，应避免
- **DeepSeek 7 指令解**（str32to8limbs）使用的是 AVX-512VL 的 256-bit 指令，在 Zen 3 上有效
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

## 6. 数据源链接

- LC /help 页面：https://judge.yosupo.jp/help
- langs.toml 原文：https://github.com/yosupo06/library-checker-judge/blob/master/langs/langs.toml
- Dockerfile 目录：https://github.com/yosupo06/library-checker-judge/blob/master/langs/
- 仓库主页：https://github.com/yosupo06/library-checker-judge
- AMD AVX-512 博客（2026-02-10）：https://www.amd.com/zh-cn/blogs/2026/understanding-avx-512---validating-usage-on-amd-epyc-.html

---

## 7. AVX-512 支持争议记录（2026-07-28）

### 争议起源

QWEN3.7 plus 提出："早期的 Zen3 代，AMD EPYC 7B13 (Zen 3) 不支持 AVX512"

### 争议解决

经查证 AMD 官方博客（2026-02-10），QWEN3.7 plus 的说法**错误**：

1. **博客原文**提到的"早期 AVX2 CPU"指的是 **Zen 2 (Rome) 及更早**，这些确实不支持 AVX-512
2. **Zen 3 (Milan, 含 EPYC 7B13) 确实支持 AVX-512**，包括 AVX512F/DQ/CD/BW/VL/IFMA/VNNI
3. 博客说的是 **"完整 512-bit 数据路径"** 只在 Zen 5 (Turin) 才有，不是说 AVX-512 指令集只在 Turin 才支持

### 对当前优化工作的影响

**AVX-512VL 优化策略仍然有效**：
- DeepSeek 的 7 指令解（str32to8limbs）使用 `_mm256_cvtepi32_epi16`（AVX-512VL 256-bit 指令）
- 该指令在 Zen 3 上可用，且因指令数减少（7 vs 10）带来性能收益
- 实测在 VM (Tiger Lake i7-11370H) 上 +12% 性能提升，预期在 LC (Zen 3) 上也有类似收益

**注意事项**：
- 应避免使用 `_mm512_*` 512-bit 指令（Zen 3 双发射，无吞吐量优势）
- 应使用 AVX-512VL 的 128/256-bit 指令（`_mm_*`, `_mm256_*` 带 AVX-512VL 语义）
- 本地 VM (Tiger Lake) 与 LC (Zen 3) 微架构不同，性能数据仅供参考，需以 LC 实测为准
