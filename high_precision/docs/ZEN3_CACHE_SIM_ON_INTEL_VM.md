# 在 Intel VM 上模拟 AMD EPYC 7B13 (Zen3) 缓存

### —— cache miss 与指令数的可信测量指南

> 型号按 **EPYC 7B13** 处理（Google Cloud 定制 Milan / Zen3，64C，Library Checker 评测机在用）。  
> 本机基线：VMware VM，宿主 **Intel i7-11370H (Tiger Lake / Willow Cove)**，6 vCPU，3 GB RAM，Ubuntu + g++ 15.2.0。  
> 文中所有数字均为 **2026-08-10 在该 VM 上实测**，非估算。



---

## 0. 结论先行

| 你想测什么                          | 用什么                                   | 可信度   | 一句话                              |
| ------------------------------ | ------------------------------------- | ----- | -------------------------------- |
| **指令数 (retired instructions)** | `cachegrind` 的 `I refs`               | ★★★★★ | 架构量，与微架构无关，可直接跨机比较               |
| **L1/LL cache miss（相对趋势）**     | `cachegrind` + Zen3 参数                | ★★★★☆ | 同配置跑 A/B 取 delta，方向和量级可信         |
| **三级层次 + TLB + 预取器**           | `DynamoRIO drcachesim`                | ★★★★☆ | 需自行安装，唯一能同时建模 L1/L2/L3+TLB 的免费方案 |
| **执行 znver3 专有指令**             | `QEMU TCG + libcache plugin`          | ★★★☆☆ | Intel 上唯一能跑 AMD 独占指令的路子，慢        |
| **cache miss 绝对值**             | 任何模拟器                                 | ★☆☆☆☆ | **别信**。见 §8 的 3.2× 实测偏差          |
| **cycles / IPC / 墙钟**          | 任何模拟器                                 | ☆☆☆☆☆ | **完全不可迁移**。只有目标机回执算数             |
| **让程序走 AMD 自适应分支**             | `LD_PRELOAD` 劫持 `sysconf` / 伪造 `/sys` | ★★★★★ | 见 §9，对有 cache-blocking 阈值的代码是刚需  |

**一句话总结**：Intel 硅片上永远测不出 Zen3 的真实 cache 行为，但**可以用软件模拟器精确复现 Zen3 的缓存几何**，从而得到「结构性诊断」和「A/B 相对 delta」。指令数是唯一可以当绝对判据的量。

---

## 1. 硬事实：什么能模拟，什么不能

### 1.1 硬件虚拟化**不能**把 Intel 伪装成 AMD

| 尝试                                    | 结果                                                                   |
| ------------------------------------- | -------------------------------------------------------------------- |
| VMware `cpuid.*.eax = "..."` mask     | 只能**屏蔽或暴露已有** feature bit，不能凭空造出硬件不支持的指令                             |
| KVM/QEMU `-cpu EPYC-Milan`（accel=kvm） | Intel 宿主上直接拒绝或缺 feature；`-cpu` model 只改 CPUID，指令仍由 Intel 硅片执行        |
| 改 vendor ID 为 `AuthenticAMD`          | guest 内核 / glibc / 编译器运行时会走 AMD 代码路径，撞上 Intel 不支持的指令 → `SIGILL` / 崩溃 |

**根因**：硬件虚拟化 (VT-x) 是「直接在物理核上跑 guest 指令」。CPUID 是可拦截的，但**缓存层级、替换策略、预取器、TLB 都焊死在硅片上**，任何 CPUID 谎言都不会改变它们的行为。

### 1.2 三条真正可行的路

```
                    ┌─ 精度低 ────────────────────────── 精度高 ─┐
指令翻译层            QEMU TCG plugin      Valgrind/Cachegrind    DynamoRIO drcachesim      gem5
慢速比               ~10–30×              ~50–100×               ~20–50×                   ~10⁴–10⁵×
缓存层级              L1 (+L2, 7.1+)      L1 + 统一 LL（两级）    任意层级 + TLB + 预取器    全可配 + 时序
能跑 AMD 独占指令      ✅ 是               ❌ 否（本机执行）        ❌ 否（本机执行）           ✅ 是
时序 / cycles         ❌                  ❌                     ❌                        ✅（但无官方 Zen3 模型）
```

### 1.3 架构量 vs 微架构量

| 量                         | 类别                       | 跨机可比?      |
| ------------------------- | ------------------------ | ---------- |
| 指令数、访存次数、分支条数             | **架构量**（由 ISA + 编译产物决定）  | ✅ 完全可比     |
| cache miss、TLB miss、分支误预测 | **微架构量**（由缓存几何/预测器决定）    | ⚠️ 需模拟目标几何 |
| cycles、IPC、墙钟             | **实现量**（由流水线/频率/内存子系统决定） | ❌ 不可比      |

---

## 2. 目标机规格基线：EPYC 7B13 (Zen3)

### 2.1 缓存与 TLB

| 层级      | 容量               | 相联度    | 行大小  | 归属          | 备注                               |
| ------- | ---------------- | ------ | ---- | ----------- | -------------------------------- |
| **L1i** | 32 KiB           | 8-way  | 64 B | 每核私有        |                                  |
| **L1d** | 32 KiB           | 8-way  | 64 B | 每核私有        | write-back                       |
| **L2**  | 512 KiB          | 8-way  | 64 B | 每核私有        | **inclusive of L1**              |
| **L3**  | **32 MiB / CCX** | 16-way | 64 B | 8 核共享一个 CCX | **victim cache，exclusive of L2** |
| 全芯片 L3  | 256 MiB          | —      | —    | 8 × CCX     | ⚠️ **单线程只能用 32 MiB**             |
| L1 DTLB | 64 entry         | 全相联    | —    | 每核          |                                  |
| L2 DTLB | 2048 entry       | 16-way | —    | 每核          |                                  |
| L1 ITLB | 64 entry         | 全相联    | —    | 每核          |                                  |
| L2 ITLB | 512 entry        | —      | —    | 每核          |                                  |

其他关键参数：op cache 4096 μops、ROB 256、decode 4-wide、dispatch 6-wide、  
3 loads + 2 stores/cycle、2× 256-bit FMA、8-ch DDR4-3200。

### 2.2 与本机宿主的差异（这就是为什么必须模拟）

| 层级  | **Zen3 (目标)**          | **Tiger Lake (本机)**       | 差异                             |
| --- | ---------------------- | ------------------------- | ------------------------------ |
| L1d | 32 KiB / 8-way         | **48 KiB / 12-way**       | 本机 **+50% 容量** → 系统性低估 L1 miss |
| L1i | 32 KiB / 8-way         | 32 KiB / 8-way            | 相同                             |
| L2  | 512 KiB / 8-way        | **1.25 MiB / 20-way**     | 本机 **2.5×** → 严重低估 L2 压力       |
| L3  | 32 MiB / CCX，exclusive | 12 MiB / socket，inclusive | 目标 **2.7×**，且策略不同              |
| 行大小 | 64 B                   | 64 B                      | 相同（唯一的好消息）                     |

**实测佐证**（§4.3 完整数据）：同一程序，只把 L1d 从 `48K/12` 换成 `32K/8`，  
D1 misses 从 **672,240 → 687,373（+2.25%）**。这 2.25% 在本机硅片上永远看不见。

---

## 3. 本机环境现状（实测 2026-08-10）

| 项                   | 状态                                           | 说明                                                                                         |
| ------------------- | -------------------------------------------- | ------------------------------------------------------------------------------------------ |
| `valgrind`          | ✅ **3.26.0**                                 | cachegrind + callgrind 全可用，`cg_annotate` 在 `/usr/bin`                                      |
| `perf`              | ✅ **可用**（`perf_event_paranoid = -1`，vPMU 已开） | ⚠️ 测的是 **Tiger Lake** PMU 事件，**不是 Zen3**                                                   |
| `qemu-x86_64`       | ❌ 未装                                         | 见 §6                                                                                       |
| `drrun` (DynamoRIO) | ❌ 未装                                         | 见 §5                                                                                       |
| `g++ -march=znver3` | ✅ 编译通过                                       | 但生成的二进制**未必能在本机执行**，见 §11.5                                                                |
| THP                 | `always [madvise] never`                     | 大页会改变 TLB 行为，cachegrind 看不见                                                                |
| 内存                  | 3 GB                                         | ⚠️ 限制 gem5 与大输入 cachegrind                                                                 |
| `lscpu` 缓存拓扑        | **不可信**                                      | 报 "L3 36 MiB (3 instances)"，真实硬件只有 12 MiB。VMware 把 6 核拆成 3 socket × 2 core，每个假 socket 各报一份 |

> **`perf` 可用是个新变化**（此前记录为「PMU 锁死」）。但它只能给宿主微架构的数据，  
> 在本文场景里**只用于 §8 的交叉校验**，不能当 Zen3 判据。

---

## 4. 方案 A：Cachegrind（首选，已装，零成本）

### 4.1 参数映射

Cachegrind 只有**两级模型**：`I1` + `D1` + 统一的 `LL`。Zen3 是三级，所以**必须跑两遍**：

```bash
# ── Pass 1：L1 + L2 视角（把 LL 当成 Zen3 的 512 KiB L2）────────────────
#    看的是「有多少访问逃出了每核私有 L2」
valgrind --tool=cachegrind --cache-sim=yes --branch-sim=yes \
         --I1=32768,8,64 --D1=32768,8,64 --LL=524288,8,64 \
         --cachegrind-out-file=cg.l2.out ./prog < input

# ── Pass 2：L1 + L3 视角（把 LL 当成 Zen3 的 32 MiB CCX L3）─────────────
#    看的是「有多少访问真的打到 DRAM」
valgrind --tool=cachegrind --cache-sim=yes --branch-sim=yes \
         --I1=32768,8,64 --D1=32768,8,64 --LL=33554432,16,64 \
         --cachegrind-out-file=cg.l3.out ./prog < input
```

| 参数            | 值                | 依据                                |
| ------------- | ---------------- | --------------------------------- |
| `--I1`        | `32768,8,64`     | Zen3 L1i：32 KiB / 8-way / 64 B    |
| `--D1`        | `32768,8,64`     | Zen3 L1d：32 KiB / 8-way / 64 B    |
| `--LL`（L2 视角） | `524288,8,64`    | Zen3 L2：512 KiB / 8-way           |
| `--LL`（L3 视角） | `33554432,16,64` | Zen3 L3：**单 CCX 32 MiB** / 16-way |

> ⚠️ **Valgrind 3.23+ 起 `--cache-sim` 默认为 `no`**（默认只数指令，跑得更快）。  
> 忘了加 `--cache-sim=yes` 就只会得到 `I refs`，没有任何 miss 数据。

> ⚠️ **参数合法性：set count 必须是 2 的幂**（实测踩过，会直接终止脚本）

```
set count = size / (assoc x line_size)     <-- 必须是 2 的幂
```

违反时 valgrind 报 `Cache set count is not a power of two.` 并退出。
注意约束的是 **set count**，不是 size、也不是 assoc —— 这点极易搞错。

| 参数 | set count | 结果 |
|---|---:|---|
| `37748736,16,64`（36 MiB / 16-way） | 36864 = 2^12 x 9 | ❌ 拒绝 |
| `37748736,18,64`（36 MiB / **18**-way） | 32768 = 2^15 | ✅ 通过 |
| `37748736,36,64`（36 MiB / 36-way） | 16384 = 2^14 | ✅ 通过 |
| `33554432,24,64`（32 MiB / 24-way） | 21845.33 | ❌ 拒绝（非整数） |
| `12582912,12,64`（12 MiB / 12-way） | 16384 = 2^14 | ✅ 通过 |

**规避法**：容量必须保真时，调整 assoc 直到 set count 变成 2 的幂。
相联度小幅偏离（16 → 18）对 miss 数的影响，远小于容量偏离。

> ⚠️ **exclusive victim cache 敏感性分析**：Zen3 L3 是 victim cache，
> 有效容量 ≈ L3 + 本 CCX 8 核的 L2 = 32 + 8x0.5 = **36 MiB**，而 cachegrind 是 inclusive 模型。
> 工作集可能卡在 32–36 MiB 区间时，用 `--LL=37748736,18,64` 再跑一遍做包夹
> （注意是 **18**-way，16-way 会被上述约束拒绝）。
> 若 32 MiB 与 36 MiB 结果相同，说明工作集根本没卡在这个边界，exclusive 效应可忽略。

### 4.2 A/B 差分（最有价值的用法）

Valgrind 3.25+ 的 `cg_annotate` 支持直接差分：

```bash
cg_annotate --diff cg.base.out cg.new.out          # 全局 + 函数级 delta
cg_annotate --auto=yes --show=Dr,D1mr,DLmr cg.l2.out   # 源码行级归因
```

**这是唯一推荐的判优方式**：同一模拟配置、同一输入，只比 delta。  
绝对值受模型简化影响大，delta 受影响小。

### 4.3 实测样例数据

微基准：1 MiB 工作集的随机 pointer-chase，524,288 次追逐（工作集刻意卡在 Zen3 512 KiB L2 与 TGL 1.25 MiB L2 之间）。

| 指标             | **A) Zen3 L1+L2**  
`32K/8 + 512K/8` | **B) Zen3 L1+L3**  
`32K/8 + 32M/16` | **C) 本机 TGL**  
`48K/12 + 1.25M/20` |
| -------------- | -----------------------------------: | -----------------------------------: | ----------------------------------: |
| **I refs**     |                        **7,243,732** |                        **7,243,732** |                       **7,243,732** |
| I1 misses      |                                2,147 |                                2,147 |                               2,147 |
| D refs         |                            2,905,518 |                            2,905,518 |                           2,905,518 |
| **D1 misses**  |                          **687,373** |                          **687,373** |                         **672,240** |
| D1 miss rate   |                                23.7% |                                23.7% |                               23.1% |
| **LLd misses** |                          **360,159** |                           **25,793** |                          **25,904** |
| LLd miss rate  |                                12.4% |                                 0.9% |                                0.9% |
| Branches       |                            2,065,779 |                            2,065,779 |                           2,065,779 |
| Mispredicts    |                               13,197 |                               13,197 |                              13,197 |

**三条可以直接拿走的结论**：

1. **`I refs` 三组完全一致（7,243,732）** —— 铁证：指令数与缓存参数**完全无关**。  
   这就是它能当跨机绝对判据的原因。
2. **A vs B 相差 14 倍（360,159 vs 25,793）** —— 两级模型的取舍**极其要命**。  
   只跑 L3 视角，会完全看不见 Zen3 那颗小 L2 的压力；这个负载在 Zen3 上  
   每 8 次访存就有 1 次逃出 L2，而在本机硅片上几乎零压力。**必须跑两遍。**
3. **A vs C 的 D1 差 15,133（+2.25%）** —— 仅仅把 L1d 从 48K/12 换成 32K/8  
   就产生了稳定可测的差异。用本机参数分析，等于系统性地给自己发免费通行证。

**补充实测（exclusive 包夹）**：同一负载下

| LL 配置 | LLd misses |
|---|---:|
| 32 MiB / 16-way（inclusive 下界） | 25,793 |
| 36 MiB / 18-way（exclusive 上界） | 25,793 |
| 64 MiB / 16-way（宽松上界） | 25,793 |

三者**完全相同** ⇒ 该负载工作集（1 MiB）根本没接近 L3 边界，exclusive 效应可直接忽略。
**这正是包夹法的用途**：不是为了拿更准的数，而是为了判断「这个边界到底关不关我的事」。

---

## 5. 方案 B：DynamoRIO drcachesim（三级 + TLB + 预取器）

Cachegrind 的两级模型是硬伤。想一次性建模 **L1i/L1d/L2/L3 + TLB + 预取器**，用 drcachesim。

### 5.1 安装（无需 root）

```bash
cd ~ && \
wget https://github.com/DynamoRIO/dynamorio/releases/download/cronbuild-11.3.0-1/DynamoRIO-Linux-11.3.0-1.tar.gz && \
tar xf DynamoRIO-Linux-*.tar.gz && \
export DR=~/DynamoRIO-Linux-11.3.0-1 && echo "export DR=$DR" >> ~/.bashrc
```

### 5.2 Zen3 三级配置

```bash
$DR/bin64/drrun -t drcachesim \
  -L1I_size 32K  -L1I_assoc 8  \
  -L1D_size 32K  -L1D_assoc 8  \
  -L2_size  512K -L2_assoc  8  \
  -LL_size  32M  -LL_assoc  16 \
  -line_size 64 -replace_policy LRU \
  -- ./prog < input
```

### 5.3 附加能力

```bash
# TLB 模拟（Zen3: L1 DTLB 64 全相联 / L2 DTLB 2048 16-way）
$DR/bin64/drrun -t drcachesim -simulator_type TLB \
  -TLB_L1D_entries 64 -TLB_L1D_assoc 64 \
  -TLB_L2_entries 2048 -TLB_L2_assoc 16 -- ./prog < input

# 下一行预取器（逼近 Zen3 的 L1/L2 stream prefetcher）
$DR/bin64/drrun -t drcachesim -prefetch_policy nextline ... -- ./prog < input

# 先抓 trace 再离线反复分析不同配置（省时间，强烈推荐）
$DR/bin64/drrun -t drcachesim -offline -outdir ./tr -- ./prog < input
$DR/clients/bin64/drcachesim -indir ./tr -L2_size 512K -LL_size 32M ...
```

> **`-offline` 是关键工作流**：抓一次 trace，之后扫参数（L2 512K vs 1M、  
> LL 32M vs 36M、有无预取器）都是纯离线重放，不用重跑程序。

---

## 6. 方案 C：QEMU TCG plugin（能执行 znver3 专有指令）

**唯一适用场景**：你的二进制用 `-march=znver3` 编译，且用到了本机 Intel 不支持的指令  
（`clzero` / `rdpru` / `mwaitx` 等），在本机直接跑会 `SIGILL`。

Ubuntu 官方 `qemu-user` 包**不带 TCG plugin**，必须自己编译：

```bash
sudo apt-get install -y ninja-build pkg-config libglib2.0-dev python3-venv
git clone --depth 1 -b stable-9.1 https://gitlab.com/qemu-project/qemu.git ~/qemu && cd ~/qemu
./configure --target-list=x86_64-linux-user --enable-plugins --disable-system
make -j$(nproc)
# 产物：./build/qemu-x86_64  与  ./build/contrib/plugins/libcache.so
```

```bash
cd ~/qemu/build
./qemu-x86_64 -cpu EPYC-Milan \
  -plugin contrib/plugins/libcache.so,\
icachesize=32768,iassoc=8,iblksize=64,\
dcachesize=32768,dassoc=8,dblksize=64,\
l2=on,l2cachesize=524288,l2assoc=8,l2blksize=64,\
evict=lru,cores=1 \
  -d plugin ./prog < input

# 纯指令计数（比 cachegrind 快很多）
./qemu-x86_64 -cpu EPYC-Milan -plugin contrib/plugins/libinsn.so -d plugin ./prog
```

| 优点                                           | 缺点                    |
| -------------------------------------------- | --------------------- |
| `-cpu EPYC-Milan` 让 CPUID **真的**报告 AMD Milan | 只有两级（L1 + 可选 L2），无 L3 |
| TCG 软件翻译，AMD 独占指令照跑不误                        | 无 TLB、无预取器模型          |
| 程序内的 `cpuid` 探测走 AMD 分支                      | 需自行编译 QEMU（~15 min）   |

---

## 7. 方案 D：gem5 —— 基本劝退

| 项       | 现实                                   |
| ------- | ------------------------------------ |
| Zen3 模型 | **没有官方模型**。社区配置是 Skylake 改的近似值，可信度存疑 |
| 速度      | 10⁴–10⁵× 慢。真机 1 秒 ≈ 模拟 3 小时          |
| 内存      | 本机只有 3 GB，跑不动像样的工作集                  |
| 校准成本    | 要先用微基准把 ROB/LSQ/预取器逐项校到 Zen3，工作量以周计  |

**只有当你需要 cycles 级别的时序结论、且愿意投入数周校准时才考虑。**  
对「测 cache miss 和指令数」这个目标，属于严重过度工程。

---

## 8. 指令数怎么测才可信

### 8.1 三个数不是一回事（实测）

同一程序、同一输入：

| 来源                                 |            数值 | 相对 cachegrind |
| ---------------------------------- | ------------: | ------------: |
| `cachegrind` **I refs**            | **7,243,732** |            基准 |
| `perf stat` **instructions**（本机硅片） | **9,036,227** |    **+24.8%** |

**为什么差 24.8%**：

| 差异来源                                 | 影响   |
| ------------------------------------ | ---- |
| `perf` 计入**内核态**（syscall、缺页处理、调度）    | 主要来源 |
| `perf` 计入**动态链接器** `ld.so` 的重定位与符号解析 | 显著   |
| `perf` 计入 **CRT 启动/退出**代码            | 中等   |
| `perf` 计入**推测执行**中被取消的路径（部分事件）       | 小    |
| cachegrind 只统计**用户态已执行指令**           | —    |

同一负载的其它交叉验证：

| 事件                    | perf（真实硅片） | cachegrind（Zen3 模型） |        比值 |
| --------------------- | ---------: | ------------------: | --------: |
| L1-dcache-load-misses |    725,580 |     652,111 (D1 rd) |     1.11× |
| cache-misses (LLC)    |     89,588 |         27,983 (LL) | **3.20×** |
| branch-misses         |     17,417 |              13,197 |     1.32× |

**LLC miss 差 3.2 倍**，因为真实硅片有：硬件预取器主动拉取（制造额外 miss）、  
页表遍历访存、推测执行的无效访存。cachegrind 一个都不建模。  
**这就是「模拟器的 cache miss 绝对值不可信」的实证。**

### 8.3 确定性：cachegrind 逐位可复现，perf 抖动可达 45%

同一二进制、同一输入，**连续两次**运行的实测对比：

| 指标 | 第 1 次 | 第 2 次 | 抖动 |
|---|---:|---:|---:|
| **cachegrind** I refs | 7,243,732 | 7,243,732 | **0.00%** |
| **cachegrind** D1 misses | 687,373 | 687,373 | **0.00%** |
| **cachegrind** LLd misses | 25,793 | 25,793 | **0.00%** |
| **cachegrind** Mispredicts | 13,197 | 13,197 | **0.00%** |
| `perf` instructions | 9,036,227 | 8,897,271 | 1.6% |
| `perf` L1-dcache-load-misses | 725,580 | 720,098 | 0.8% |
| `perf` **cache-misses** | 89,588 | **61,599** | **45.4%** |
| `perf` branch-misses | 17,417 | 16,107 | 8.1% |

**cachegrind 全部指标逐位相同**（纯软件模拟，无任何硬件噪声）；
`perf` 的 LLC miss 抖动高达 **45%**（受同机其它进程、页缓存状态、预取器时机影响）。

**推论**：
- 指令数 A/B 判优**只能用 cachegrind**。它的 0.00% 抖动才撑得起 ±0.01% 的判据；
  拿 `perf instructions` 做同样的事，1.6% 的噪声会把真实的 0.5% 优化整个淹掉。
- `perf` 的 cache-miss 数**单次采样毫无意义**，至少取 10 次中位数，且只看数量级。


### 8.2 判据

| 用途                    | 判据                              | 依据                            |
| --------------------- | ------------------------------- | ----------------------------- |
| **指令数 A/B 判优**        | `I refs` 差异 **> ±0.01%** 才算真变化  | cachegrind 完全确定性，同源码同输入必然逐位相同 |
| **cache miss A/B 判优** | 只看 **方向 + 量级**（如「L2 miss 降 3×」） | 绝对值有 1.1–3.2× 系统偏差            |
| **cycles / 墙钟**       | **不用模拟器判**，只认目标机回执              | 无时序模型                         |

> **前提**：A/B 两侧必须是**同一编译器、同一 flag、同一输入、同一模拟配置**。  
> 换任何一个，数字就不可比。

---

## 9. 让程序「以为」自己在 AMD 上

很多高性能库（GMP、OpenBLAS、FFTW，以及手写的 cache-blocking 代码）会在运行时  
**探测缓存大小来选择分块参数**。在本机 Tiger Lake 上探测，会选出 48K/1.25M/12M 对应的参数，  
和目标机上的行为完全不同 —— **这时候即使缓存模拟器参数配对了，被测的也是错误的代码路径。**

### 9.1 拦截 `sysconf`（最省事，覆盖 glibc 路径）

```c
/* zen3cache.c — g++ -shared -fPIC -o zen3cache.so zen3cache.c -ldl */
#define _GNU_SOURCE
#include <unistd.h>
#include <dlfcn.h>

long sysconf(int name) {
    static long (*real)(int) = 0;
    if (!real) real = (long(*)(int))dlsym(RTLD_NEXT, "sysconf");
    switch (name) {
        case _SC_LEVEL1_ICACHE_SIZE:      return 32   * 1024;
        case _SC_LEVEL1_ICACHE_ASSOC:     return 8;
        case _SC_LEVEL1_ICACHE_LINESIZE:  return 64;
        case _SC_LEVEL1_DCACHE_SIZE:      return 32   * 1024;
        case _SC_LEVEL1_DCACHE_ASSOC:     return 8;
        case _SC_LEVEL1_DCACHE_LINESIZE:  return 64;
        case _SC_LEVEL2_CACHE_SIZE:       return 512  * 1024;
        case _SC_LEVEL2_CACHE_ASSOC:      return 8;
        case _SC_LEVEL2_CACHE_LINESIZE:   return 64;
        case _SC_LEVEL3_CACHE_SIZE:       return 32   * 1024 * 1024;  /* 单 CCX */
        case _SC_LEVEL3_CACHE_ASSOC:      return 16;
        case _SC_LEVEL3_CACHE_LINESIZE:   return 64;
        default: return real(name);
    }
}
```

```bash
LD_PRELOAD=./zen3cache.so ./prog < input
# 与 cachegrind 叠加：
LD_PRELOAD=./zen3cache.so valgrind --tool=cachegrind --cache-sim=yes \
    --I1=32768,8,64 --D1=32768,8,64 --LL=524288,8,64 ./prog < input
```

### 9.2 伪造 `/sys/devices/system/cpu/cpu*/cache/`（覆盖直接读 sysfs 的库）

```bash
# 在独立 mount namespace 里做，不影响系统其它进程
sudo unshare -m bash -c '
  cp -r /sys/devices/system/cpu/cpu0/cache /tmp/fakecache 2>/dev/null
  echo 32K       > /tmp/fakecache/index0/size    # L1i
  echo 8         > /tmp/fakecache/index0/ways_of_associativity
  echo 32K       > /tmp/fakecache/index1/size    # L1d
  echo 8         > /tmp/fakecache/index1/ways_of_associativity
  echo 512K      > /tmp/fakecache/index2/size    # L2
  echo 8         > /tmp/fakecache/index2/ways_of_associativity
  echo 32768K    > /tmp/fakecache/index3/size    # L3 (单 CCX)
  echo 16        > /tmp/fakecache/index3/ways_of_associativity
  mount --bind /tmp/fakecache /sys/devices/system/cpu/cpu0/cache
  ./prog < input
'
```

### 9.3 伪造 CPUID

无法用 `LD_PRELOAD` 拦截（`cpuid` 是指令不是函数调用）。**唯一途径是 §6 的 QEMU TCG `-cpu EPYC-Milan`。**  
若代码里有手写 `__cpuid()` 分发（比如根据 vendor string 选 SIMD 路径），必须走 QEMU。

---

## 10. 校准与验收

### 10.1 校准流程

```
1. 写 3 个微基准，工作集分别卡在 32K / 512K / 32M 边界
2. 在模拟器上跑，确认 miss 曲线在这三个点出现台阶
   └─ 没出现台阶 = 参数配错了，先修参数再谈业务代码
3. 若能拿到目标机（GCP c2d / 任意 Zen3 云实例）：
   跑同样的微基准，用 perf 取真值，算出「模拟器 → 真机」的校正系数
4. 无法拿到目标机时：只用相对 delta，永不使用绝对值
```

### 10.2 三级证据链（按可信度递减）

| 级别     | 来源                     | 能得出什么                          |
| ------ | ---------------------- | ------------------------------ |
| **黄金** | 目标机真实回执（LC 提交结果）       | 唯一的性能判优依据                      |
| **白银** | 模拟器 A/B delta（同配置、同输入） | 优化方向对不对、量级有多大                  |
| **青铜** | 模拟器绝对值                 | 只用于结构性诊断（「这个循环 L2 miss 占 70%」） |

**红线**：模拟器数据只能用来**筛掉明显不行的候选** + **解释为什么快/慢**，  
**绝不能**用来宣称「这个版本比那个快 X%」。

---

## 11. 陷阱清单

| #  | 陷阱                                     | 后果                                                                | 对策                                                    |
| -- | -------------------------------------- | ----------------------------------------------------------------- | ----------------------------------------------------- |
| 1  | **VM 里 `lscpu` 的缓存拓扑是假的**              | 本机报 "L3 36 MiB (3 instances)"，真实只有 12 MiB                         | 缓存参数一律硬编码，绝不从 `lscpu` 读                               |
| 2  | **忘了 `--cache-sim=yes`**               | Valgrind 3.23+ 默认关闭，只有 `I refs`，没有任何 miss 数据                      | 显式写上                                                  |
| 2b | **set count 非 2 的幂** | valgrind 报 `Cache set count is not a power of two.` 直接退出；`set -e` 的脚本会静默中断整轮 | `size/(assoc*line)` 须为 2 的幂；调 assoc 规避（36 MiB 用 18-way）。脚本每轮加 `|| true` |
| 3  | **只跑 L3 视角**                           | 完全看不见 Zen3 小 L2 的压力（实测差 14 倍）                                     | §4.1 两遍都跑                                             |
| 4  | **ASLR 改变 cache 冲突 miss**              | 同一二进制两次跑 miss 数不同                                                 | `setarch $(uname -m) -R ./prog`                       |
| 5  | **znver3 二进制在本机 SIGILL**               | 本机有 `sha_ni`/`vaes`/`vpclmulqdq`，但**缺** `clzero`/`rdpru`/`mwaitx` | 分析用 `-march=x86-64-v3` 保可移植；必须 znver3 时走 §6 QEMU      |
| 6  | **拿 `perf instructions` 当 `I refs` 用** | 实测偏差 **+24.8%**（内核态 + `ld.so` + CRT）                              | 两者永不互换；跨机比指令数只用 cachegrind                            |
| 7  | **把 L3 当 256 MiB**                     | 单线程只能用本 CCX 的 **32 MiB**                                          | `--LL=33554432` 而非 268435456                          |
| 8  | **忽略 L3 是 exclusive victim cache**     | cachegrind 的 inclusive 模型低估有效容量                                   | 32 MiB 与 36 MiB 各跑一遍做包夹                               |
| 9  | **THP 干扰**                             | 本机 `[madvise]`，大页改变 TLB 行为，cachegrind 无 TLB 模型看不见                 | 用 drcachesim 的 TLB 模式；或 `THP=never` 统一基线              |
| 10 | **cachegrind 慢 50–100×**               | 大输入跑几小时                                                           | 缩小输入到「刚好越过最大 cache 边界」即可                              |
| 11 | **未对齐编译 flag**                         | 指令选择/调度不同，指令数不可比                                                  | A/B 两侧统一 `-march=znver3 -mtune=znver3`（或统一 x86-64-v3） |
| 12 | **`perf` 在 VM 里的数据当 Zen3 用**           | 测的是 Tiger Lake 的 PMU                                              | 只用于 §8 交叉校验                                           |

---

## 12. 一键脚本

保存为 `zen3sim.sh`，`chmod +x`：

```bash
#!/usr/bin/env bash
# zen3sim.sh — 在任意 x86 机器上按 AMD EPYC 7B13 (Zen3) 缓存几何分析程序
# 用法: ./zen3sim.sh ./prog [input_file]
set -euo pipefail

PROG="${1:?usage: $0 ./prog [input]}"; IN="${2:-/dev/null}"
OUT="zen3sim.$(date +%H%M%S)"; mkdir -p "$OUT"

# ── Zen3 / EPYC 7B13 缓存几何（硬编码，勿从 lscpu 读）──────────────
I1="32768,8,64"        # L1i  32 KiB  8-way
D1="32768,8,64"        # L1d  32 KiB  8-way
L2="524288,8,64"       # L2  512 KiB  8-way  (每核私有)
L3="33554432,16,64"    # L3   32 MiB 16-way  (单 CCX，非全芯片 256 MiB)
L3X="37748736,18,64"   # L3 exclusive 上界 36 MiB；必须用 18-way，见 4.1 set-count 约束

run() {  # $1=tag  $2=LL参数（失败不中断整脚本）
  echo "── [$1] LL=$2"
  setarch "$(uname -m)" -R valgrind --tool=cachegrind \
      --cache-sim=yes --branch-sim=yes \
      --I1="$I1" --D1="$D1" --LL="$2" \
      --cachegrind-out-file="$OUT/cg.$1.out" \
      "$PROG" < "$IN" > "$OUT/stdout.$1" 2> "$OUT/cg.$1.log"
  grep -E 'I refs|D1  misses|LLd misses|Mispredicts' "$OUT/cg.$1.log" | sed 's/^==[0-9]*== /   /'
}

echo "=== Zen3 (EPYC 7B13) cache simulation ==="
run l2  "$L2"     # L1 + L2 视角：多少访问逃出私有 L2
run l3  "$L3"     # L1 + L3 视角：多少访问真的打到 DRAM
run l3x "$L3X" || echo "   (l3x 跳过：参数被 cachegrind 拒绝)"

echo
echo "=== 交叉校验：本机硅片真值（非 Zen3，仅供参考）==="
perf stat -e instructions,L1-dcache-load-misses,cache-misses,branch-misses \
     "$PROG" < "$IN" > /dev/null 2> "$OUT/perf.log" || echo "   perf 不可用（vPMU 未开）"
grep -E 'instructions|misses' "$OUT/perf.log" 2>/dev/null | sed 's/^/   /' || true

echo
echo "结果目录: $OUT/"
echo "A/B 差分: cg_annotate --diff <base>/cg.l2.out $OUT/cg.l2.out"
echo "行级归因: cg_annotate --auto=yes $OUT/cg.l2.out | head -60"
```

**典型工作流**：

```bash
./zen3sim.sh ./prog_base input.txt      # → zen3sim.150312/
# 改代码 …
./zen3sim.sh ./prog_new  input.txt      # → zen3sim.151047/
cg_annotate --diff zen3sim.150312/cg.l2.out zen3sim.151047/cg.l2.out | head -40
```

---

## 附录：参数速查

```bash
# Cachegrind — Zen3 L1+L2 视角
--I1=32768,8,64 --D1=32768,8,64 --LL=524288,8,64
# Cachegrind — Zen3 L1+L3 视角（单 CCX）
--I1=32768,8,64 --D1=32768,8,64 --LL=33554432,16,64
# Cachegrind — Zen3 L3 exclusive 上界 36 MiB（必须 18-way，见 4.1）
--I1=32768,8,64 --D1=32768,8,64 --LL=37748736,18,64

# drcachesim — Zen3 完整三级
-L1I_size 32K -L1I_assoc 8 -L1D_size 32K -L1D_assoc 8 \
-L2_size 512K -L2_assoc 8 -LL_size 32M -LL_assoc 16 -line_size 64

# drcachesim — Zen3 TLB
-TLB_L1D_entries 64 -TLB_L1D_assoc 64 -TLB_L2_entries 2048 -TLB_L2_assoc 16

# QEMU TCG — Zen3 L1+L2 + AMD CPUID
-cpu EPYC-Milan -plugin contrib/plugins/libcache.so,\
icachesize=32768,iassoc=8,iblksize=64,dcachesize=32768,dassoc=8,dblksize=64,\
l2=on,l2cachesize=524288,l2assoc=8,l2blksize=64,evict=lru,cores=1
```

---

*文档基于 2026-08-10 在 `10.144.33.157`（VMware / i7-11370H / 6 vCPU / 3 GB / valgrind 3.26.0 / g++ 15.2.0）的实测数据编写。所有性能数字均为实跑结果。*
