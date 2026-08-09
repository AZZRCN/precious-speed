# MUL 优化 Phase 0 剖析报告

**日期**: 2026-08-01
**目标**: 在 LC "Multiplication of Big Integers" 当前 36ms（#385663 / #387374 并列 #1）基础上，定位真正可突破的瓶颈，为 Phase 1 开发提供决策依据。
**方法**: 生产代码 `mul.cpp`（= `best/mul_385663.cpp`，未做任何修改）本地编译运行 + 阶段计时插桩副本 `lc_bench/exe/mul_p3.cpp`（仅加 `MUL_PROFILE` 计时宏，生产代码零改动）。

> **⚠️ 本文档已随 Phase 1 推进更新**（2026-08-01 19:20）：
> - (B) RIRI 打包**已证伪剔除**（本代码 FFT 输入已是复杂打包，RIRI 会翻倍 N → 更差）。
> - (A) radix-4 **已落地并验证**：FFT 1.66×、计算 −14.3%、本地 LC 口径最慢值 93.0→87.6ms（−15.3%）、20/20 逐字节正确、精度余量 7.8%。
> - 详细过程见 **`MUL_PHASE1_DEVLOG_2026-08-01.md`**。下一步：上传 `mul_r4.cpp` 作为 `mul.cpp` 到 LC 实测最慢值是否 < 36ms。

---

## 0. TL;DR（给赶时间的结论）

1. **瓶颈高度集中**：max 用例（2M×2M 位）~80% 的进程内时间花在 **`fft_fwd` = 两次正向 DIF + 点乘(DOT) + 逆向 DIT** 上。其余 parse/write/carry/pack 合计仅 ~5–6ms。
2. **交接文档有两处关键事实错误**（影响方向判断）：
   - 实际 `Base = 10^8`（非 10⁴）；`float_len = 2^19 = 524288`（非文档写的 2^20）。
   - **carry 链仅 ~1ms（~4%），并非 O3 报告假设的「fftMul carry 占 40–50%」**——那是旧血统的数据。
3. **真正杠杆在 FFT 正向变换上**（唯一有效方向 = radix-4）：
   - **(A) radix-4 蝶形**：N=2^19 层数 19→10，扫趟内存访问减半。✅ 已落地验证（见下）。
   - ~~(B) RIRI 打包~~：**已证伪**，本代码 FFT 输入已是复杂打包 `(hi,lo)`（最小 N），RIRI 需翻倍 N → 更差，剔除。
4. **fft_killer_01 是内容/精度构造**，规模与 max_max 同为 2^19，无尺寸层面的救点（已用 case 数据验证）。

> 注：本机（Intel, `-march=native`）墙钟 ~100–127ms 主要由进程启动+管道开销主导，**不代表 LC 等价**；以文档 LC=36ms 为真实目标。进程内阶段拆分可靠。

---

## 1. 阶段实测（max_max_00，2M×2M 位，float_len=2^19，进程内，3 轮）

| 阶段 | 时间(ms) | 占 compute | 说明 |
|------|---------|-----------|------|
| **fft_fwd**（2×DIF + DOT + DIT） | **17.5 – 19.3** | **~80%** | 绝对瓶颈 |
| ├ 正向 DIF ×2 | （含于上） | | **未 RIRI 打包，两次独立** |
| ├ DOT（共轭对称） | （含于上） | | 已半谱优化 |
| └ 逆向 DIT | （含于上） | | 半谱，~5ms |
| parse（读+十进制→limb） | 1.9 – 2.5 | ~9% | SWAR 扫描 + LUT，已较快 |
| write（limb→十进制） | 1.4 | ~6% | 40KB 查找表 memcpy |
| carry（double→limb 进位链） | 0.97 | ~4% | **串行依赖但极快** |
| pack（limb→双精度） | ≈0 | ~0% | 标量 hi/lo 分裂 |
| **compute 合计（a\*=b）** | **21 – 23** | 100% | |
| 进程内总计（parse+compute+write） | ~25 | | |

fft_killer_01（1,999,995×1,999,995）同构：fft_fwd 17–19.6ms，carry 0.94ms，compute 20.6–23.4ms。

---

## 2. 对各阶段 SIMD / 向量化现状盘点（385663 血统）

| 阶段 | 现状 | 是否已向量化 | 备注 |
|------|------|------------|------|
| **DIF / DIT 蝶形** | 手写 AVX2 `__m256d`，一次 2 复数，`fmaddsub` FMA；radix-2，19 层 | ✅ AVX2+FMA | `_mm256_loadu_pd`（**非对齐**加载）；radix-2 层数多 |
| DOT | `frequencyDomainPointwiseMultiply` 利用共轭对称（前向/后向配对） | ✅ | 已半谱，~2.6ms |
| pack（limb→double） | `digits[i]/10000` + `_mm_set_pd` 逐 limb 标量 | ❌ 标量 | 极快，非瓶颈 |
| carry | `realPart+0.5` 标量浮点→整 + `carry%Base,/Base` | ❌ 标量串行 | 仅 ~1ms，无需动 |
| parse | `readFromCursor`：8 字节 SWAR 找数字边界 + `construct` 8 字符/LUT→limb | ⚠️ 半向量 | 已较快，~2ms |
| write | `writeUnsigned`：首位逐位 + 其余每 limb 两次 `memcpy(4B)` 取自 **40KB 输出表** `detail::O` | ❌ 标量表查 | 40KB 表占 L1 压力（O3 报告 P1） |

**关键代码位置**：DIF/DIT 在 `mul.cpp:217-295`（`__m256d` 蝶形）；正向变换调用 `mul.cpp:1346`（两次独立 `decimationInFrequency`）；carry `1372-1378`；pack `1338-1345`；parse `readFromCursor` `1750`；write `writeUnsigned` `1766`。

---

## 3. 与交接文档 / O3 报告的偏差（重要）

| 文档说法 | 实测/核实 | 影响 |
|---------|----------|------|
| `Base = 10^4` | **Base = 10^8**；float_len=**2^19** | 规模估算全错；radix-4 层数按 19 而非 20 计 |
| fft_killer 是「尺寸卡点」专项 | 规模 1,999,995 位 = 同 2^19，与 max_max 一致 | 「fft_killer 专项」方向**无救点**（内容构造） |
| 「结合 385663+387374 优势」提分 | 两者 max case 均 36ms，medium_02(27vs18) 不制约成绩 | **对提分无效** |
| O3 报告：carry 链占 fftMul 40–50%，P2 优先级高 | carry 仅 **~1ms(4%)** | P2 **严重高估**；旧血统数据 |
| 「FFT 核心占 82%，非 FFT 仅 18%」 | 进程内 FFT≈70–80%，非 FFT≈20%（parse+write+carry≈5–6ms） | 方向对；但非 FFT 里 **carry 不是大头**，是 parse/write |
| 声称已用「RIRI 打包 / 三次变两次」 | 代码实际做**两次独立正向 DIF**（a、b 各一次） | **存在未实现的真实收益**：正向 DIF 可减半 |

---

## 4. 20 case 本地基线（`daytime_bench.py` 实测，10 轮中位数）

> 重要前提：本机墙钟被进程启动+管道主导（max case 100–140ms），绝对值**不能**当 LC 等价；且本轮 `CUR = BEST = 生产 385663 同一份二进制`（二者 376.2 KB、**+0 字节**），故这是一次 **A/A 自洽校验**，目的是确认测量工装可信、并锁定「最慢 case 排序」这一定性信号。

| case | CUR_med | BEST_med | diff | % | wins |
|------|--------:|---------:|-----:|----:|------:|
| example_00 | 64.2 | 67.5 | −3.3 | −4.8% | 3/10 |
| small_00 | 111.0 | 113.2 | −2.2 | −1.9% | 2/10 |
| medium_00 | 97.2 | 102.0 | −4.8 | −4.7% | 8/10 |
| medium_01 | 95.0 | 100.9 | −5.9 | −5.9% | 4/10 |
| medium_02 | 104.1 | 106.2 | −2.1 | −2.0% | 2/10 |
| large_00 | 102.4 | 104.4 | −2.0 | −2.0% | 10/10 |
| large_01 | 99.7 | 101.0 | −1.2 | −1.2% | 8/10 |
| large_02 | 106.5 | 98.4 | +8.1 | +8.3% | 0/10 |
| large_small_00 | 112.0 | 117.8 | −5.8 | −5.0% | 8/10 |
| max_max_00 | 126.0 | 125.0 | +1.0 | +0.8% | 3/10 |
| max_max_01 | 95.8 | 95.7 | +0.1 | +0.1% | 1/10 |
| max_max_02 | 107.6 | 121.5 | −13.9 | −11.4% | 10/10 |
| max_max_03 | 120.3 | 137.1 | −16.7 | −12.2% | 6/10 |
| max_max_04 | 140.7 | 116.0 | +24.7 | +21.3% | 0/10 |
| max_max_05 | 114.8 | 110.9 | +3.9 | +3.5% | 0/10 |
| max_max_06 | 124.7 | 124.5 | +0.2 | +0.2% | 7/10 |
| max_max_07 | 127.7 | 128.9 | −1.2 | −0.9% | 8/10 |
| fft_killer_00 | 119.4 | 120.4 | −1.0 | −0.8% | 4/10 |
| fft_killer_01 | 131.7 | 125.4 | +6.4 | +5.1% | 3/10 |
| zero_00 | 92.5 | 77.3 | +15.3 | +19.8% | 0/10 |
| **TOTAL** | **2193.5** | **2194.0** | **−0.5** | **−0.0%** | **87/200** |

**读图**：diff 列正负混杂、TOTAL≈0、CUR 胜率 ~44%（≈噪声 50%）→ 确认 A/A 无系统性偏差。定性结论成立：**max_max 家族与 fft_killer_01 是本地最慢档**（与文档一致），正是 fft_fwd 瓶颈所在。后续 Phase 1 只需把原型放进 `CUR`（`cur_mul.exe`），用同脚本对比 `BEST`（生产）即可读出真实增益。

---

## 5. 结论与 Phase 1 建议

### 唯一真杠杆 = `fft_fwd`
非 FFT 开销本地仅 ~5–6ms，即使全清零也只省这点；**要突破 36ms，必须砍 fft_fwd（~18ms 本地 / LC 占比同构）**。

### 两个具体攻击点（均落在正向 DIF）
- **(A) radix-4 DIF/DIT（文档首选未试方向）** ✅ **Phase 1 已落地并验证**：N=2^19 → 19 层 radix-2 融成 ~10 层 radix-4，手写 AVX2 FMA。实测 FFT **1.66×**（2¹⁹：11.25→6.78ms），端到端计算 −14.3%，本地 LC 口径最慢值 93.0→87.6ms（−15.3%），20/20 逐字节正确，精度余量 7.8%（≈12×）。详见 `MUL_PHASE1_DEVLOG_2026-08-01.md`。
  - 注意：387374 用 radix-4 **auto-vec** 与 385663 radix-2 **手写 AVX2** 同得 36ms → 必须用**手写 radix-4 + FMA（`fmaddsub` 融合）+ 对齐加载 + 良好访存**，而非依赖 GCC 自动向量化。（本实现满足）
- ~~**(B) RIRI 打包（已证伪，剔除）**~~：本代码 FFT 输入**已是复杂打包 `(hi,lo)`**（每样本 2 位十进制，最小 N）。对实数序列做 RIRI 需把 N 翻倍 → 4N vs 当前 3N 变换，**更差**。RIRI 对本代码是死路，不再是候选杠杆。

### 次要（安全、低风险，O3 报告 P1/P3）
- write：40KB 表 → AVX2 纯算术（~28 指令），释放 L1 给 FFT 数据（~1–2ms LC）。
- parse：移植 `str32to8limbs` 风格 SIMD（~1–2ms LC）。
- 二者合计约 2–4ms LC，可作为 radix-4 之外的保险垫。

### 明确放弃
- 结合 385663+387374、fft_killer 尺寸专项、NTT/SSA/WFTA/TFT/six-step/Karatsuba/DCT/查找表/矩阵（交接文档已证无正收益）。
- carry 链优化（实测仅 ~1ms，O3 报告高估）。

### Phase 1 推荐执行顺序（进度更新）
1. ✅ **手写 radix-4 DIF（融合两级 radix-2）**：已落地、验证 20/20 全过、本地 fft_fwd 1.66×、计算 −14.3%。产物 `lc_bench/exe/mul_r4.cpp` + `mul_r4.exe`。
2. ❌ ~~叠加 RIRI 打包~~：已证伪（见上），不再进行。
3. ⏳ 视余量上 write/parse SIMD（次要保险垫，~2–4ms LC）。
4. ⏳ **提交 LC 实测**：把 `mul_r4.cpp` 作为 `mul.cpp` 上传跑分，确认最慢值 < 36ms。本地机器≠LC 环境，相对加速应可迁移（FFT 为单线程 CPU 瓶颈）。
5. 每步遵守：verify_mul 全过 + interleaved-min 基准 + 警惕 fft_killer 精度。

---

## 附：插桩副本（可复现）
- `lc_bench/exe/mul_p3.cpp`：`-DMUL_PROFILE` 编译，阶段计时打到 stderr（`MULPROF` 行）。生产 `mul.cpp` 未改动。
- 编译：`g++ -std=c++20 -O2 -march=native -DMUL_PROFILE -I. lc_bench/exe/mul_p3.cpp -o lc_bench/exe/mul_p3.exe -lpthread`
- 运行：`cat cases/mul/max_max_00\ .in | mul_p3.exe 2>prof.txt`（prof.txt 含阶段表）
