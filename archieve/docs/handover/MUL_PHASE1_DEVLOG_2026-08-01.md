# MUL 优化 Phase 1 开发日志（radix-4 FFT 融合）

> 用途：实时记录开发过程，抵抗上下文压缩。配合 `MUL_PHASE0_PROFILE_2026-08-01.md` 阅读。
> 最后更新：2026-08-01 20:40 (GMT+8)
> 状态：**VM 实测完成：r4_current 33.53ms vs gold 41.76ms = 1.245×（−19.7%）；4 候选全正确；强烈预期破 LC 36ms，待提交。**

---

## 0. 背景与目标

- 项目：Library Checker "Multiplication of Big Integers" #1，瓶颈是 MUL（历史最佳 36ms，始终无法突破）。
- LC 环境：GCP c2d-highcpu-8，AMD EPYC Zen3 (Milan)，1 核，**CPU-time 计分**，无多线程。`g++ -O2 -std=c++23 -march=native`（GCC 15.2），**仅 AVX2**（AVX-512 被 CE 关掉）。
- 计分口径：**20 个用例取最慢值**。任何 1 例回归即出局。
- Phase 0 结论：瓶颈在 `fft_fwd`（前向变换），重负载用例 max_max 家族 + fft_killer_01 最慢。
- Phase 1 目标：把 19 级 radix-2 融成 ~10 级 radix-4，**手写 AVX2 FMA**，减少内存扫掠与蝶形指令数。

### 关键前提修正（本会话新结论）
- **RIRI 打包被证伪**：原 Phase 0 报告把 "(B) RIRI 打包" 列为杠杆，但本会话确认——本代码 FFT 输入已是复杂打包 `(hi,lo)`（每样本 2 位十进制，最小 N）。对实数序列做 RIRI 会把 N 翻倍 → 4N vs 当前 3N 变换，**更差**。RIRI 是死路，已剔除。
- **（A）radix-4 是唯一有效 Phase 1 方向。**

---

## 1. 第一次尝试（失败）：教科书 radix-4 DIF/DIT

- 复制 `mul.cpp` → `lc_bench/exe/mul_r4.cpp`，注入 `buildRadix4` / `decimationInFrequency4` / `decimationInTime4` + `twiddleFactors4`（每组预存 W¹,W²,W³）。
- 切换 AVX2 平衡/平方驱动到 radix-4。
- **结果：编译过，但 verify_mul 17/20 失败，输出半长乱码。**
- 隔离测试 `test_r4.exe`（标量 `std::complex` 镜像 DIF4/DIT4 vs naive DFT + 往返）证明**算法本身数学错误**：
  - `n=8`: invErr=9.7e0 / dftErr=6.7e0（应为 ~1e-12）
  - `n=128`: invErr=2.8e2 / dftErr=2.7e1
- 两个 bug：
  1. **DIF 蝶形符号反向**：`t1 - i·t3` 配 W¹、`t1 + i·t3` 配 W³（写反）。
  2. **奇指数分级错位**：m 为奇数（N=2¹⁹，m=19）时最小 radix-4 块必须是 8 不是 4（`L=1<<(2s+3)`）。

---

## 2. 根本重新思考：不能套教科书 radix-4

- 原代码的 FFT 用的是**每块常数旋转因子 + 位反转旋转因子表**的变体（不是教科书 radix-4 的 W^k/W²k/W³k）。
- 更重要的是：pointwise 不是普通逐元素乘——它在做 **R2C 实数谱解包/重打包**，配对模式 `(2,3),(4,7),(5,6),(8,15)…` 是 **radix-2 位反转序专属的**。
- 之前那个 `…Full` pointwise 版本从语义上就是错的 → 这才是乱码真因。
- **结论：必须复用本代码自己的两级 radix-2 蝶形做代数融合**，输出顺序与旋转因子表天然兼容，pointwise 完全不动。

### 融合推导（关键）
设stage A（blockSize=2q，块旋转 wA）、stage B（blockSize=q，两子块 wB0/wB1），融合后：
```
a[k]     = (A + C·wA) + (B + D·wA)·wB0
a[k+q]   = (A + C·wA) - (B + D·wA)·wB0
a[k+2q]  = (A - C·wA) + (B - D·wA)·wB1
a[k+3q]  = (A - C·wA) - (B - D·wA)·wB1
```
块 b 取 `wA=tw[b]`、`wB0=tw[2b]`、`wB1=tw[2b+1]`。由位反转表性质可证 `wB0=tw[2b]=w`、`wA=tw[b]=w²`、`wB1=tw[2b+1]=i·w`（其中 `w=tw[2b]`）。代入展开得**每组仅 3 次复乘**（radix-2 两级要 4 次）：
```
令 X=A, Y=B·w, Z=C·w², T=D·w³：
a[k]     = (X + Z) + (Y + T)
a[k+q]   = (X + Z) - (Y + T)
a[k+2q]  = (X - Z) + i·(Y - T)
a[k+3q]  = (X - Z) - i·(Y - T)
```
- w、w²、w³ 每组只算一次，复用现有 `twiddleFactors` 表，**无需新表**（省 ~8MB）。
- **精度优化（最终采用）**：`w²` 直接查表 `tw[b]`（由恒等式 `tw[b]=tw[2b]²`），比 `w·w` 连乘少一次复乘且更准。

---

## 3. 验证路径（全部绿灯）

1. **标量等效测试** `test_r4.cpp`：融合 radix-4 与现有 radix-2 输出逐元素一致，误差 ~1e-13（纯浮点舍入）。→ pointwise / 输出顺序都不用动。
2. **直接替换 AVX2 本体**：把 `decimationInFrequency` / `decimationInTime` 的 AVX2 实现原地替换为融合 radix-4，8 个调用点（平衡×2 + 平方 + NEON/标量路径不含 AVX2 本体故不受影响）自动生效。NEON/标量路径完全不动。
3. **逐字节正确性** `verify_mul.py mul_r4.exe cur_mul.exe`：**20/20 PASS**（cur_mul.exe 是 36ms 金标准二进制）。
4. **FFT 微基准** `fft_micro.cpp`（进程内、交错 min、含舍入余量测量）：
   | N | radix-2 | radix-4 | 加速 | r4 舍入误差 | 预算占用 |
   |---|---|---|---|---|---|
   | 2¹⁹（max_max 实际尺寸）| 11.25 ms | 6.78 ms | **1.66×** | 3.9e-2 | **7.8%** |
   | 2²⁰ | 43.9 | 23.2 | 1.89× | 9.4e-2 | 18.8% |
   | 2²² | 211.7 | 109.6 | 1.93× | 4.6e-4 | — |
   - 无任何尺寸回归（r4 永远 ≤ r2）。
   - 2¹⁹ 下舍入误差仅用掉 0.5 安全阈值预算的 **7.8%（≈12 倍余量）**，精度无忧。
5. **端到端精确基准** `ab_min_bench.py`（交错 min-of-12，扣除 ~57.9ms 进程启动 floor）：
   - LC 计分口径（最慢用例）= max_max_00：**93.0 → 87.6 ms，−5.4ms（−15.3%）**
   - 总计算 −14.3%，CUR 胜 168/240 轮（70%）
   - 重负载用例一致 −14%~−21%；medium 家族 ±0.1ms 属噪声且不影响最慢值。

---

## 4. 当前产物（在制品）

| 文件 | 角色 |
|---|---|
| `lc_bench/exe/mul_r4.cpp` | **优化源码（Phase 1 交付物）** —— mul.cpp 的 AVX2 变换本体替换为融合 radix-4，其余不变 |
| `lc_bench/exe/mul_r4.exe` | 编译产物，验证用 |
| `lc_bench/exe/test_r4.cpp` | 标量融合 radix-4 vs radix-2 等效测试 |
| `lc_bench/exe/fft_micro.cpp` | 进程内 FFT 微基准（含精度测量） |
| `lc_bench/ab_min_bench.py` | 交错 min 精确 A/B 基准 |
| `cur_mul.exe` / `best_mul.exe` | 金标准 oracle（36ms 二进制），**未改动** |

---

## 5. 结论与下一步

- radix-4 融合是**正确的、更快的、精度安全的** Phase 1 实现。
- 本地 LC 口径最慢值 −15.3%（FFT 1.66× 带动整体计算 −14%）。
- **是否足以突破 36ms 需实际提交 LC**（本地机器≠LC 环境，但 FFT 是单线程 CPU 瓶颈，相对加速应可迁移）。
- 下一步：
  1. 上传 `mul_r4.cpp`（作为 `mul.cpp`）到 LC 跑分，确认最慢值 < 36ms。
  2. 若过线：把 `mul_r4.cpp` 提升为 `mul.cpp`，归档本报告。
  3. 若不够：Phase 2 候选——(a) AVX-512 路径（若 LC 放开 CE）；(b) carry/schoolbook 小尺寸阈值调优；(c) float_len 2¹⁹→2²⁰ 分摊大数用例。
  4. 修正 `MUL_PHASE0_PROFILE` 报告：删除 (B) RIRI 杠杆，标注已证伪。

---

## 6. 关键坑位速查（防重复踩）

- 不要套教科书 radix-4 的 W¹/W²/W³ 蝶形 → 与现有旋转因子表 + R2C pointwise 不兼容。
- 必须融合本代码的**两级 radix-2**，输出顺序才对。
- 奇指数 N（2¹⁹）最小 radix-4 块是 8，不是 4。
- `w²` 查表 `tw[b]` 比 `w·w` 更准更快（恒等式 `tw[b]=tw[2b]²`）。
- AVX2 双复数打包用 `__m256d`，数据以 `double*` 步长 ×2 访问（`bg + 2*(k+...)`），勿用 `__m128d*` 直接偏移。

---

## 7. 权威化实测：GEN 重建用例 + VM 单线程计时（本会话续）

> 用户分级权威度：**本地 Windows（最弱） < 本地 VM 实测（SSH 192.168.1.55, Ubuntu 26.04, i7-11370H）< LC 实测（最权威）**。
> 并要求：多轮、单线程、高精度、热身 + 交替运行。参与测速：BEST 文件夹 MUL 系列 + 当前产物。

### 7.1 旧本地用例是「退化/损坏」的（关键纠偏）
- 旧 `lc_bench/cases/mul/` 的 max_max 家族经 gold 跑出 `"0"`（2 字节），medium/small/zero 产出 0 字节——**不是算法 bug，是用例本身损坏**（CRLF + 疑似截断/错传）。
- 因此上一轮 VM 基准的 max_max 行（~3.2 vs ~3.3ms 近平）**无参考价值**，且「best_cpp / r4_387374 与 gold 在 medium/small/zero 不一致」也是该退化用例造成的假象。

### 7.2 用 GEN 重建 20 个权威用例
- GEN 仓库：`E:\library-checker-problems-master\big_integer\multiplication_of_big_integers\`（info.toml 分布：example×1 small×1 medium×3 large×3 max_max×8 zero×1 fft_killer×2 large_small×1）。
- 各 generator 为 `seed = argv[1]` 的独立可执行；本地 Windows `.exe` 跑 `max_max_gen.exe 0` 产出与旧 `max_max_00.in` **逐字节一致**（确认旧用例本是 GEN 产物、种子 0..N，只是传输中损坏）。
- 在 VM 上编译 7 个 generator（`-O2 -std=c++23 -march=x86-64-v3`，需 `common/random.h` + `base.hpp` + `params.h`），按 info.toml 分布以种子 0..(number-1) 重建 20 个 `.in`：
  - max_max_00.in = 4,000,004 B（= 真正的 2M×2M，非退化）；fft_killer ~4.0MB；large ~3.9MB；medium ~4.0MB；small 4.4MB(T=200000)；zero 4.0MB；large_small 4.0MB。
  - VM 生成版用 LF（规范）；Windows 版多 2 字节 CRLF，对解析无影响（mul 按空白切分）。

### 7.3 正确性：4 候选全绿（VM，混合校验）
- **Oracle 层（12 例）**：Python `int(a)*int(b)` 直算（小/中/大/零/large_small 全部 + max_max_00、fft_killer_00 两个重载点）。
  - 注意：CPython 的 `int("2M位串")` 十进制解析是 O(n²)，2M 位单例约 6s；故重载用例只取 2 个做定点，其余靠共识层。
- **共识层（8 例）**：max_max_01..07 + fft_killer_01，4 候选输出逐字节相同即判通过。
- **结果**：`gold_385663 / best_cpp / r4_387374 / r4_current` 全部 `oracle 12 / consensus 8 / BAD=0`。
- **关键**：`r4_current` 在 max_max_00（最重用例）与 `gold_385663` **逐字节相同**（4,000,000 B 真实乘积，非 0）。即融合 radix-4 在最重用例上正确，且 == LC #1 金标准。

### 7.4 VM 计时基准（**已完成**，权威中间层）
- 命令：`vm_bench.py`，单线程 `taskset -c 0`，CPU-time 用 `resource.RUSAGE_CHILDREN`，热身 4 + 30 轮交错，per-case MIN，LC 口径 = max over cases of per-case MIN。
- 编译：`g++ -O2 -std=c++23 -march=x86-64-v3`（= AVX2+FMA+BMI2，无 AVX-512，镜像 LC Zen3）。
- 候选：gold_385663(best/mul_385663.cpp) / best_cpp(best/mul.cpp) / r4_387374(best/mul_387374.cpp) / r4_current(lc_bench/exe/mul_r4.cpp)。
- **正确性全绿**：4 候选均 `oracle 12 / consensus 8 / BAD=0`；`r4_current` 在全部 20 例与 gold 逐字节相同（`r4_eq_gold=True`）。

#### 结果（LC 口径最慢值 = max over cases of per-case MIN，单位 ms）
| 候选 | worst(ms) | 相对 gold |
|---|---|---|
| gold_385663（36ms 金标准）| **41.756** | 1.000× |
| best_cpp (best/mul.cpp) | 38.199 | 1.093× |
| r4_387374 (best/mul_387374.cpp) | 37.464 | 1.115× |
| **r4_current（融合 radix-4）** | **33.530** | **1.245×** |

- **r4_current 在每一个重载用例（max_max / fft_killer / large / large_small）上都快于 gold**，worst 由 max_max_05 决定（gold 40.236 → r4 33.530，该例 1.20×）。max_max 家族 r4 约 1.27–1.30×；fft_killer 约 1.18–1.27×；large 约 1.20–1.31×。
- 唯一例外是零值小用例 zero_00（r4 4.297 vs gold 4.178，差 0.1ms，永不决定 LC 最慢值，无碍）。

#### 相对本地 Windows 旧估计
- 旧 Windows 微基准：FFT 1.66×、端到端最慢值 −15.3%。
- VM 实测：端到端最慢值 **−19.7%（比 Windows 估计还更好）**；FFT 微基准的 1.66× 高估了「FFT 单核增益」，但端到端因 FFT 占主导，实际提速更可观。
- 即：**用户「GLM/DeepSeek 也没做出好效果」的疑虑，在本论 GEN 真重载 + 单线程高精度计时下被证伪**——融合 radix-4 确实带来可复核的 ~20% 提速。

#### 向 LC 的预测（最权威层，待实跑确认）
- VM 用 i7-11370H（Tiger Lake），单核弱于 LC 的 EPYC Zen3：gold_385663 在 VM=41.76ms、在 LC=36ms（比值 1.16×）。
- 若相对加速可迁移（FFT 为单线程 AVX2 瓶颈，应可迁移）：r4_current@LC ≈ 33.53/1.16 ≈ **29ms**，明显低于 36ms → 有望夺榜一。
- 即便保守（只取 1.15×）：≈31ms，仍 < 36ms。
- **结论：r4_current 强烈预期破 36ms；最终以 LC 实跑为最权威判决。**

### 7.5 坑位补充
- Windows Python 脚本里本地路径要用 `D:/...` `E:/...`，**不要写 `/d/`、`/e/`**（那是 Git Bash 映射，Windows 原生 Python 不认 → FileNotFoundError）。
- VM 上后台长任务要用 `setsid ... < /dev/null` 真正脱离 SSH 会话，否则 paramiko 的 `stdout.read()` 会一直等通道 EOF 而超时。
- generator 编译还需 `../base.hpp`（fft_killer 用），别漏传。

---

## 8. LC 实测结果 + 微架构翻车诊断（Phase1 提交 #389863）

> 用户提交 `mul_r4.cpp` → **AC 39 ms，落后 36 ms 榜首 3 ms**（全部 20 例 AC；correctness 绿）。

### 8.1 关键诊断：融合 radix-4 在 Zen3 上「翻车」
- **VM 冷跑（fresh process per case，镜像 LC 单进程口径）实测**：
  | 候选 | worst-mean | worst-max |
  |---|---|---|
  | gold_385663 | 46.45 | 48.40 |
  | r4_current | 38.06 | **39.54** |
  | best_cpp | 42.15 | 43.32 |
  | r4_387374 | 40.52 | 41.68 |
- r4_current 的 VM worst-max = 39.54ms，**与 LC 实跑 39ms 几乎吻合** → 39ms 不是运气差，是真实水平。
- **翻车根因**：gold 是 **AVX2 radix-2（shuffle-free 蝶形，`fmsubadd` 融合共轭）**；我的融合 radix-4 每 4 点做 3 次复乘 + `rotate90x2` 置换，靠**减少趟数**取胜。趟数只在「8MB 变换放不进缓存」时有用——VM 的 Tiger Lake（12MB L3）如此，故 VM 上我快；但 **LC 的 Zen3（32MB+ L3）整个 8MB FFT 都在缓存里，趟数变便宜，而融合核更重的置换/乘成为瓶颈** → 我在 Zen3 反而更慢。gold 在 Zen3 提速 0.78×（46→36），我的核在 Zen3 无收益（39→39）。VM 与 LC 排名反转，正是此因。
- **结论**：融合 radix-4 是「对 Tiger Lake 对、对 Zen3 错」的优化；LC 目标机是 Zen3，故该方向在 LC 不成立。

### 8.2 gold（36ms 榜首）已近最优，浮点微优化空间极小
- gold 蝶形用 `_mm_fmaddsub_pd` 即 **3-复乘等价**（非朴素 4 乘），pointwise 同样已最优。
- 朴素「pointwise 5 复乘有冗余」是误判：`productB = mul(secondEven,firstOdd)+mul(firstEven,secondOdd)` 两项**不同对**，不可借交换律合并。
- 故在纯浮点 FFT 域内继续微优化（pointwise/蝶形）几乎无油水；36ms 榜首是调得很狠的浮点 FFT。

### 8.3 真正能赢的路只有两条（且都难）
1. **结构性浮点 FFT 改动**：split-radix / radix-8（更少趟数）。但 radix-4/8 天生带 `*i` 置换（shuffle），在 Zen3 shuffle 端口受限下可能仍不如 shuffle-free 的 radix-2 → 未必赢。
2. **NTT（算法创新）**：3-prime + AVX2 int32/int64，整数精确、无精度开销。但本规模下浮点用 base-1e8 仅 2^19 点；NTT 用小素数→基数仅~30→点数反而更多(2^21)，用 64 位大素数→int64 模乘更重/点；**此规模下 NTT 未必比浮点快**，且改写大、正确性风险高。
- **诚实结论**：当前 39ms（AC）已是正确的次优解；要在 LC 稳定破 36ms 极难，需赌结构性改写且未必回报。下一步方向需用户拍板（见对话）。
