# 测量方法论（Zen3 口径）— HEX 三题

> 建立于 2026-08-10。所有数字均为 `10.144.33.157` 实测。
> 上游依据：`D:\precious_speed\ZEN3_CACHE_SIM_ON_INTEL_VM.md`（DEC 先辈文档）

---

## 0. 铁律（本轮新增，覆盖前任口径）

| # | 铁律 | 依据 |
|---|---|---|
| **M1** | 本地编译**必须** `-march=znver3 -mtune=znver3`。**禁用 `-march=native`。** | LC 用 `-march=native`，评测机是 Zen3 → native==znver3。本机是 Tiger Lake，native 会生成 **AVX-512(EVEX)** 指令 —— LC 上根本不存在 |
| **M2** | 每个二进制入测前跑 **EVEX 扫描**，必须为 0 | 见 M1。`objdump -d bin \| grep -cE "^\s+[0-9a-f]+:\s+62 "` |
| **M3** | **指令数判优只用 cachegrind `I refs`**，差异 >0.01% 才算真变化 | cachegrind 逐位确定性（0.00% 抖动）；`perf instructions` 有 +24.8% 系统偏差 + 1.6% 抖动 |
| **M4** | cache miss **只看 A/B delta 的方向与量级**，绝对值不可信 | 模拟器 vs 真硅片 LLC miss 实测差 3.2× |
| **M5** | wall-time 判据取 **MIN**，不取中位数 | 噪声只会让程序变慢。本 VM med/min 可达 1.5× |
| **M6** | wall / cycles **不可跨机迁移**，只作本地趋势；终裁永远是 LC 回执 | — |
| **M7** | **Ir 降低 ≠ 变快。** 任何"变快"必须有 cachegrind L2esc/DRAM 或 wall MIN 佐证 | 实测 v3→v5 Ir −10%，wall MIN **0%** |
| **M8** | **cachegrind 是内核态盲区。** 它只模拟用户态访存，**看不到 page fault / TLB miss / 内核时间**。凡涉及大数组首触、mmap、THP 的改动，cachegrind 会报"零差异"而实测 −10~−20%。此类改动**必须**用 loopbench + `/usr/bin/time -v` 的 minor-fault 计数佐证 | THP 巨页化实测 add −10%、div −18%、mul −8%，cachegrind Ir/L2esc/DRAM **全部不变** |
| **M9** | **短程序（<15 ms）禁用 zbench wall。** `subprocess` spawn 固定开销 ≈6 ms 会淹没信号。改用累计循环计时（单进程外 N 次循环 / N） | zbench wall 对 7 ms 的 add 分辨率 ≈±40%；loopbench 同点复现 ±1% |
| **M10** | **墙钟判优五项强制条件**（缺一即结论作废）：① **R≥20 轮**（R=12 功效不足，见证据）；② **ABBA 蛇形交替**（偶数轮反序），消除"先跑者"一阶偏袒；③ 主判据是**同轮配对比值 median + IQR + 符号检验**，不是全局 MIN；④ **漂移诊断**（最小二乘 %/round + 前后半均值差），\|slope\|>1%/rd 或 \|half-delta\|>3% 则墙钟绝对值作废，**退回 cachegrind 判优**；⑤ **墙钟分辨率地板 ≈1.5%**，低于此的差异墙钟无权发言。工具：`tools/loopbench2.py`（`loopbench.sh` 已废弃：4 轮、单向、无漂移检测、无显著性闸门） | 实测漂移曾达 **−0.85 %/round**（越跑越快：频率 boost 爬坡 + 页缓存冷）。旧 loopbench 单向 4 轮报 v9 −2.7%；ABBA R=12 报 med 0.9814/0.9847 但 **IQR 跨 1.0 → NO_DIFF**；R=21 才收窄到 IQR=[0.9745,0.9901]、wins 18/21、p=0.0015 → **FASTER**。真值 **−1.7%** |
| **M11** | **burn-in 必须是完整 block**（`ZB_BURN`，默认 3 个完整轮），不是每个 bin 单跑几次。半温状态下开测 = 给第一个 bin 灌噪声 | 每 bin 各跑 3 次的旧法：drift **−0.85 %/rd**；改为 3 个完整 block 后：drift **+0.06 %/rd**，漂移基本消失 |
| **M12** | **IQR 跨 1.0 ≠ 无差异，只是功效不足。** 必须同时看**符号检验**（R 轮里的胜负方向一致性，精确二项双尾 p）。`p<0.05` 且方向一致即为真实效应，此时若 IQR 仍跨 1.0，须由 cachegrind 佐证幅度 | v8→v9 在 R=12 时 IQR 跨 1.0 被误判 NO_DIFF，但 R=21 符号检验 18/21 (p=0.0015) + cachegrind LL rd miss −20.3% 双证，确认真实 |
| **M13** | **参数扫描（leaf size / 阈值等）一律先用 cachegrind，禁止用墙钟。** 这类改动幅度通常 <1%，落在墙钟地板以下，墙钟给出的排序**连符号都可能是错的** | `FFT_LEAF_LOG` 旧墙钟扫描报"L9 优 1.5%"；cachegrind 逐位显示 L9 比 L11 **多** 0.32% Ir、多 0.80% D1 miss（且 L9≡L10、L11≡L12 完全同码路）。**符号相反，旧结论作废** ——但见 M14：该次 cachegrind 用的是**本机 Tiger Lake 几何**，换 Zen3 几何后结论再次反转（L7 才是最优） |
| **M14** | **cachegrind 必须用 Zen3 几何（`tools/zen3cg.sh`），且 Ir 与 D1/LL 要分开读（双视角）。** 用本机 48 KiB/12-way L1D 的默认几何跑出来的 miss 数，对 32 KiB/8-way 的 Zen3 **无参照价值**；同一组候选换几何后排序会反转 | `FFT_LEAF_LOG` 三次翻案：墙钟报 L9 优 → 本机几何 cachegrind 报 L11 优 → **Zen3 几何报 L7 优**（D1 miss −16.34%），生产墙钟复核 −2.33%（p=0.0072）三方一致 |
| **M15** | **微基准与生产口径冲突时，一律以 `loopbench2` 生产口径为准。** 孤立微基准会把被测片段整个放进 L1，抹掉真实的 cache 竞争，系统性高估收益 | twiddle 消融微基准报 −4.31% 上界，生产实测 −3.52%（82% 兑现率）；早期若干微基准曾报 >10% 而生产 0% |
| **M16** | **本项目 mul 的瓶颈是 L1D→L2 访存延迟，不是端口/发射。** 因此 **Ir 不是判优主指标**，D1 miss 才是。经验换算：**每降 1% D1 miss ≈ 换 0.143% 墙钟**。凡"Ir 上升但 D1 miss 下降"的改动，不得据 Ir 直接否决，必须上墙钟 | perf 实测 FP 端口 p0/p1/p5 各仅 **39%** 占用；29M stall cycles 中 **19.8M 是 mem_any**。反例：L7 比 L11 多 1.39% Ir，却快 2.33% |
| **M17** | **正确性闸门必须前置于性能测量：任何新候选先跑 `stress_*.py` 400 轮，PASS 之后才允许上 cachegrind / loopbench2。** "官方测试点逐字节一致"**不等于**正确 —— 固定测试点的规模分布极窄，覆盖不到小/中等规模的分支 | v10b 对全部官方点逐字节一致、cachegrind 与墙钟双证 −3.52%，随后 400 轮压测 **bad=3 FAILED**：twiddle 高位分量循环外提在 `2·bc > halfSize` 时跨块失效，只在小 FFT（n≤2^11）触发。**先测速后验正确 = 白测一轮** |
| **M18** | **A/B 两个二进制必须用完全相同的编译命令，且必须是 LC 口径 `g++ -O2 -march=x86-64-v3 -std=c++23`。** 入测前核对 `stat -c%s` + EVEX 扫描（M2）。`-march=native` 双重违规：既生成 Zen3 不存在的 AVX-512，又让 valgrind 崩"非法指令" | 本轮真实事故：基线 v9L7 用 `-O2`、候选 v10cL7 误用 `-O3 -march=native`，测出 −3.46%；统一口径重测只剩 **−1.12%**。**近 2.4 个百分点是编译选项差异冒充的收益** |
| **M19** | **隔代收益必须与直接前驱对拼。** 候选 vN 只跟老基线 v(N−k) 比是无效的：中间版本的收益会被算进来，热漂移也会被算进来。**流程：vN vs v(N−1) 直接 ABBA 配对，NO_DIFF 即弃。** 同时凡出现 `THERMAL_DRIFT` 警告的结论一律不采信，必须重跑 | v10d vs 老基线 v9_u 报 −3.06%（p=0.0009，但带 DRIFT 警告）；与直接前驱 v10c_u 对拼：med=**1.0094**、wins=11/25、p=0.6900 → **NO_DIFF，v10d 反而略慢**。−3.06% 全是 v10c 的收益 + 热漂移伪影 |
| **M20** | **系统层（THP / syscall / I/O）在动手前先实测占比，别凭直觉优化。** 用 `smaps_rollup` 查 `AnonHugePages` 覆盖率、`strace -c` 查 syscall 分布 | 本项目实测：THP 覆盖 **24576/27836 kB = 88%**（`hugify_bss` 在 [madvise] 下确已生效）；全程仅 **49 次** syscall / 0.95 ms，其中 read 4 次占 91.7%（纯 I/O 不可省）。**系统层三条路全部闭合，剩余 kernel cycles 是首次触碰的 page fault，已被 THP 压到底** |

---

## 1. 工具链

| 工具 | 路径 | 用途 |
|---|---|---|
| `tools/zbench.py` | VM: `~/hexbench/zbench.py` | **主力**。三口径统一台：`wall` / `perf` / `cg` / `all` |
| `tools/zen3sim.sh` | VM: `~/hexbench/zen3sim.sh` | 单二进制 Zen3 三视角 cachegrind |
| `tools/lab.py` | 本地 | 全量 22/24/27 点正确性 + perf（**旧口径，待迁移到 znver3**） |
| `tools/loopbench.sh` | VM: `~/hexbench/loopbench.sh` | **短程序主力**。累计循环计时，摊薄 fork 抖动（见 M9） |
| `tools/stress_mul.py` `stress_div.py` `stress_add.py` | VM | 400 轮随机对拍闸门 |
| `tools/maxprec_mul.py` | VM | 满规模（3.2 MB）精度压测 |

### loopbench 用法

```bash
# 每个二进制连跑 N 次取总时长/N，交替 4 个 block，报全局 MIN
ZB_CPU=3 ./loopbench.sh data/mul/max_max_01.in 12 /tmp/v8 /tmp/v9
```

### zbench 用法

```bash
# 三口径全跑
ZB_CPU=3 python3 zbench.py all data/mul max_max_01,large_01 mul/v1.cpp mul/v5.cpp --reps=25

# 只跑 cachegrind（最可靠，零抖动）
python3 zbench.py cg data/mul max_max_01 mul/v3.cpp mul/v5.cpp --tag=xxx
```

输出列含义：

| 列 | 含义 | 可信度 |
|---|---|---|
| `Ir` | cachegrind I refs，用户态已执行指令 | ★★★★★ 绝对判据 |
| `Dref` | 数据访问次数 | ★★★★★ 架构量 |
| `D1m` | L1d miss（Zen3 32K/8） | ★★★★☆ delta |
| `L2esc` | 逃出 Zen3 私有 512K L2 的访问数 | ★★★★☆ delta，**mul 的主战场** |
| `DRAM` | 逃出 32 MiB CCX L3 的访问数 | ★★★★☆ delta |

### Zen3 缓存几何（硬编码，勿从 lscpu 读）

```
--I1=32768,8,64  --D1=32768,8,64
--LL=524288,8,64        # L2 视角
--LL=33554432,16,64     # L3 视角（单 CCX 32 MiB，非全芯 256 MiB）
--LL=37748736,18,64     # exclusive 上界 36 MiB 包夹（18-way 规避 set-count 约束）
```

---

## 2. 环境降噪

VM 噪声源（2026-08-10 实测）：

| 源 | 占用 | 处置 |
|---|---|---|
| `gnome-system-monitor` | **22% CPU / 300 MB** | ✅ 已 kill |
| `gnome-shell` | 12% CPU / 275 MB | ⚠️ 仍在运行，建议 `systemctl isolate multi-user.target` |
| `localsearch` / `tracker` | 零星 | ✅ 已 kill |
| swap 已用 539 MB | 3 GB 内存偏紧 | 大点测试前 `drop_caches` |

降噪后 max_max_01 的 med/min 从 1.53 降到 1.44。**仍不理想，wall 只能当粗筛。**

---

## 3. 已确立的物理事实（mul, max_max_01, 3.2 MB 输入）

```
version   Ir        Dref     D1m      L2esc      DRAM
v1       189.6M    53.6M   6434.9K   3600.2K    487.4K
v3       146.8M    41.6M   4479.2K   2879.9K    487.4K
v4       136.4M    41.4M   4479.2K   2879.9K    487.4K
v5       131.8M    42.0M   4478.6K   2879.9K    487.4K
web      235.2M    56.5M   5680.5K   5646.6K    491.2K
```

**F1. 不是 DRAM bound。** 所有版本（含 web）DRAM miss 完全相同 = 487.4K × 64 B ≈ **31 MB**，正好等于"把数据过一遍"。之前"33 MiB RSS 超 Zen3 32 MiB L3 → 打 DRAM"的假设 **证伪**。

**F2. 是 L2 逃逸 bound。** v5 的 L2esc = 2879.9K × 64 B = **184 MB** 的 L2↔L3 流量。
FFT 数组 = 2^20 复点 × 16 B = 16 MB，双 buffer 32 MB。**184 / 32 ≈ 5.75 遍**。
与 "radix-2² 递归下降到住进 512 K L2 之前需要 3 次四路分裂 = 6 层全数组遍历" 完全吻合。

→ **Phase 1 主攻方向：降低 L2 逃逸次数。**

**F3. v3 之后两刀（gather / AVX2 pointwise）是账面收益。**

| | Ir | wall MIN |
|---|---|---|
| v1 | 189.6M (1.000) | 34.40 ms (1.000) |
| v3 | 146.8M (0.774) | 31.41 ms (0.913) |
| v4 | 136.4M (0.720) | 31.48 ms (0.915) |
| v5 | 131.8M (0.696) | 32.00 ms (0.930) |

v3→v5 指令降 10%，时间 **0 收益**（甚至微升）。原因：Dref / D1m / L2esc 三项完全不变，瓶颈在访存不在发射。
**保留 v5（Ir 更低无害），但不再沿这条线投入。**

**F4. LC 评测机噪声 ±36%。** `max_max_00..07` 八个点结构完全同构（T=1，3.2 MB，两个 1.6 M hex digits），LC 实测 26/37/27/26/28/28/**38**/27 ms。
→ 我们 mul 的真实水平是 **~27 ms**，回执上的 38 ms 是一次不幸抖动。
→ **推论：要坐实第一，不能只快一点点，必须快到即使 +36% 抖动仍压过对手。**

**F5. `web_best/mul.cpp` 各项指标全面落后我方**（Ir 1.24×、L2esc 1.57×、small_00 Ir **6.26×**、wall MIN 1.37×）。
→ **它不可能是 mul 榜一。对标物有误，需重新获取。**

**F6. 炸弹 #5：page fault 吃掉近一半墙钟（三题通用）。**
VM 上 THP = `madvise`，内核不主动给 2 MiB 页；BSS 大数组用 4 KiB 页首触 → 数千次 minor fault，`sys` 时间 ≈ `user`。
修复 = `main()` 首行对整段 BSS 显式 `madvise(MADV_HUGEPAGE)`：

```cpp
extern "C" char __bss_start[], _end[];
static inline void hugify_bss() {
#ifdef MADV_HUGEPAGE
    constexpr uintptr_t HP = (uintptr_t)1 << 21;
    uintptr_t lo = ((uintptr_t)__bss_start + HP - 1) & ~(HP - 1);
    uintptr_t hi = (uintptr_t)_end & ~(HP - 1);
    if (hi > lo) madvise((void*)lo, hi - lo, MADV_HUGEPAGE);
#endif
}
```

| 题 | 点 | minor fault | loopbench 前 | 后 | 比 |
|---|---|---|---|---|---|
| add | small_00 | 1466 → 87 (−95%) | 8228 us | 7404 us | 0.90 |
| add | max_max_00 | — | 5231 us | 4164 us | **0.80** |
| div | small_00 | 1963 → 390 (−82%) | 13884 us | 12449 us | 0.90 |
| div | max_00 | — | 6155 us | 5051 us | **0.82** |
| mul | max_max_01 | — | (v7) 20856 us | (v8) 19051 us | 0.91 |

**cachegrind 对此完全零反应**（见 M8）。dTLB miss add −66% / div −45%。

**F7. GMP 标尺已建立 —— 我方乘法核是 GMP 的 2.07× 速。**
`gmpref` harness 直调 `mpn_mul`，同规模（100 000 × 100 000 limbs ≈ 1.6 M × 1.6 M hex）：

| 阶段 | GMP (ms) | 我方 v8 (ms) |
|---|---|---|
| read | 1.8 | 0.62 |
| parse / split | 5.1 | 2.24 |
| **乘法核** | **25.2** | **12.2** |
| fmt / emit | 4.5 | 0.54 |
| **TOTAL** | **38.9** | **15.0** |

→ **结论：mul 已显著超过"世界最优大数库"。** 继续在 mul 上抠常数收益递减；结构性收益只可能来自算法层（三模 NTT / Bailey four-step）。

**F8. v9 zero-hi FFT（−2.7%）。** forward FFT 的输入上半永远是零填充（`coeffs <= lm/2`），故：
1. 顶层 radix-4 蝶形只读下半两个 quarter（`difZeroHiTop`），省 1/2 读流量；
2. `split_b2` 只需清零到 `lm/2`，省 1/2 memset。
max_max_01：v8 19079 us → v9 18568 us（0.973）。400 轮对拍 ALL OK。

**F9. `FFT_LEAF_LOG` = 7（~~L11~~，2026-08-10 二次翻案）。**
旧结论"L9–L11 差异 <1.5% → 保留 L11"**作废**，原因见 M14：那次 cachegrind 用的是本机 Tiger Lake 几何（48 KiB/12-way L1D），对 Zen3（32 KiB/8-way）无参照价值。

改用 Zen3 几何（`tools/zen3cg.sh`）重扫：

| | Ir | D1 miss | 生产墙钟 (loopbench2 R=25) |
|---|---|---|---|
| L11 | 基准 | 基准 | 基准 |
| **L7** | **+1.39%** | **−16.34%** | **−2.33%**（p=0.0072）|

**Ir 更高却更快** —— 这是 M16（访存受限）的直接证据。L7 叶子 = 128 cpx = 2 KiB，整个递归工作集压得更深进 L1。400 轮压测 ALL OK。**L7 已采纳为新基线。**

**F10. 瓶颈性质判定：L1D→L2 访存延迟受限，非端口/发射受限。**（推翻此前"计算受限"判断）

| 证据 | 数值 |
|---|---|
| FP 端口 p0 / p1 / p5 占用 | 各 **39%**（远未饱和）|
| stall cycles 总计 | 29.0 M |
| 其中 `mem_any` 引起 | **19.8 M（68%）** |
| 经验换算 | 每降 1% D1 miss ≈ 换 **0.143%** 墙钟 |

→ 优化方向从"减浮点运算"转为"**减数据往返**"。这是 radix-16 融合蝶形（`supopt/TASK_radix16_butterfly.md`）的立论基础。

**F11. 两级 twiddle 表（v10 → v10b → v10c，最终 −3.5%）。**
原 4 MiB 平表 `twbuf[LMMAX>>1]` → 16 KiB `twbase[1<<12]`（常驻 L1），查表改为 `twg(i) = cmul(twbase[i&mask], twbase[halfSize|(i>>halfLog)])`。

| 版本 | 改动 | Ir | D1 miss | LLd | 墙钟 | 压测 |
|---|---|---|---|---|---|---|
| v10 | 朴素两级表 | **+10.66%** | −11.37% | −25.85% | NO_DIFF（Ir 吃光收益）| — |
| v10b | +高位分量循环外提 +`mulI` 恒等式 | +2.71% | −11.09% | −25.81% | **−3.52%** (p=0.0002) | **bad=3 FAILED** |
| **v10c** | v10b + 跨块保护 | 同上 + 每层 1 分支 | 同上 | 同上 | 复测中 | **1600 轮 ALL OK** |

消融实验（把 twiddle 索引截断到 1024 项、结果错但访存全 L1）给出**理论上界 −4.31%**，v10b/v10c 兑现 82%。

**F12. twiddle 表两条恒等式（dump 逐位验证）。**
1. `tw[2i+1] = tw[2i] · i` —— 纯 90° 旋转，用 `shuffle_pd + xor` 实现（`mulI`），**省 1 次 load + 1 次复乘，零误差**（只交换 re/im 并翻符号位）。
2. `tw[2i] = sqrt(tw[i])` —— 用于建表递推。

**F13. 高位分量循环外提的跨块陷阱（v10b 事故根因）。**
`twhi(base)` 提到 j 循环外的前提是 `(base+j) >> halfLog` 在整个 `j < bc` 内恒定。`base = bb·bc` 天然对齐 `bc`，故：
- `hiA`（索引 `base+j`，跨度 `bc`）→ 要求 **`bc ≤ halfSize`**
- `hiB`（索引 `base2+2j`，跨度 `2·bc`）→ 要求 **`2·bc ≤ halfSize`**

而 `halfSize = 1 << ((31−clz(n)) >> 1)`，**随 FFT 规模缩小**：n=2^20→1024（安全），n=2^11→32（越界）。
⇒ 只有**小 FFT 才会取错 twiddle**。大输入官方点全部逐字节一致，随机压测中 63×513 / 97×64 / 152×513 limb 三例暴露。
修法：层循环内一次 `if ((bc<<1) <= twHalfSize)` 判定，越界层退回完整 `twg()`（分支在层级、非蝶形级，开销可忽略）。
**注意 `halfLog` 不可自由抬高** —— `a0 = π/halfSize, a1 = a0/halfSize` 把角度分辨率绑死在 `halfSize²`，改 halfLog 会改变 `twg(i)` 的语义。

---

## 4. GMP 参照系（E:\gmp-6.3.0）

GMP 6.3 带 **`mpn/x86_64/zen3/gmp-mparam.h`** —— AMD Zen3 官方调优阈值：

```
MUL_TOOM22_THRESHOLD    20      MUL_TOOM6H_THRESHOLD   303
MUL_TOOM33_THRESHOLD    89      MUL_TOOM8H_THRESHOLD   418
MUL_TOOM44_THRESHOLD   130      MUL_FFT_THRESHOLD     3264   (limbs)
SQR_TOOM8_THRESHOLD    592      SQR_FFT_THRESHOLD     2624
INV_NEWTON_THRESHOLD   107      BINV_NEWTON_THRESHOLD  312
```

`MUL_FFT_THRESHOLD = 3264 limbs = 208,896 bits = 52,224 hex digits`。
我们的 max_max 点每个操作数 1.6 M hex digits ≈ 100 K limbs —— **深在 FFT 区**，方向正确。

待办：把 GMP 编到 VM，写 harness 直接调 `mpn_mul`，测同规模 wall MIN，作为"世界最优大数库"的理论参照线。
（GMP 不能提交到 LC，仅作标尺。）

---

## 5. 待澄清

1. **mul 真实榜一是谁、多少 ms？** `web_best/mul.cpp` 已证伪。
2. add / div 的对标物是否也需要复核？
3. 是否允许 `systemctl isolate multi-user.target` 关闭 VM 图形界面（再省 12% CPU + 275 MB）？
