# HEX 大数除法 (#393027) 理论极限冲刺指南 + 交接文档

> 2026-08-17 编写。
> **角色交接**：自此本人转为**专职指导**（advisor）——负责出路线、给公式、定验收门禁、复盘数据；**不再直接改代码**。执行 agent 按本文档推进，一切争议以 VM callgrind 实测收口。
> 前置文档：`HANDOVER_HEX_DIVISION.md`（§1-§13，铁律与已封死路清单，全部继续有效）。

---

## 0. 一句话总纲

**这道题除法链路里的每一次乘法，理论上都只需要"半份乘积"（高位段或低位段），当前实现全部算了全份。** 把每一处乘法降成半长/半输出卷积（packing + middle/short product + Karp–Markstein），再叠加指令层与微架构收尾，就是通往理论极限的路。目标落点：**417M → 270~300M I refs**（-25~35%）。

---

## 1. 现状锚点（不可漂移的基准）

| 项 | 值 |
|---|---|
| 基线源码 | `D:\precious_speed\393027_opt.cpp`（= VM66 `/tmp/opt_src.cpp`，md5 一致） |
| MAX 绑定 case | LC `length_ratio_integer#0`（本地 `cases_hex/length_ratio_0.in`） |
| 基线 I refs | length_ratio_0 = **416,990,433**（VM66 callgrind，`I +refs:` 多空格） |
| 热点分布 | difRec 45.4% / ditRec 20.8% / mulg 11.9% / pointwise 6.7% / 其他 ~15% |
| 判官 | AMD EPYC 7B13 (Zen3, AVX2 无 AVX-512)，~10M instr ≈ 1ms |
| 度量 | **callgrind I refs 是唯一决策真值**（VM66 PMU 不可虚拟化，perf 不可用）；每改动 26 case 字节 SAME + MAX I refs 严格下降，否则回退 |

测试点事实（题库 `D:\library-checker-problems-master\big_integer\division_of_hex_big_integers`）：
- `SUM_OF_CHARACTER_LENGTH = 3,200,002`（全测试 hex 字符总量），单操作数 ≤ 1.6M chars = 400K limbs（base 2^16）。
- MAX case：`length_ratio_integer` seed0 → m=2, t=2：B≈533K chars（~133K limbs），A≈1067K chars（~266K limbs），na=2nb → **newton_divide 路径**。
- 输入格式 `A B` 空格分隔同一行；`rnear_0` 基线自身崩溃（rc=136），非回归信号。

---

## 2. 核心洞察：除法链的乘法账本

对 `newton_divide` 全链逐乘法盘点（执行者第一步先在源码里画出**乘法调用 DAG**，标注每个乘积实际被消费的位段）：

| 乘法 | 产物实际消费 | 本质 |
|---|---|---|
| invertappr 内 `d × (B^n + xh·B^l)` → W | `E = B^{2n} − W` 只取**高段** | **middle product** |
| invertappr 内 `Ehi × xh` | `floor(Ehi·xh / B^h)` 只取**高段** | **middle product** |
| 商估计 `an × v` → qe | 商高位估计，只取**高段** | **middle product** |
| 余数修正 `qe × dn` → t，`A − t` | r = A mod B^n 只需**低 nb 位** | **short product** |
| invertappr 递归层 | 同构缩小 | 同上两类 |

**结论：除法链里没有任何一处真正需要 full product。** 当前实现全部按 full（FFT 长 lm ≥ 2·total、全长 IDFT）计算——这就是与理论极限的差距来源。参考对标：`E:\gmp-6.3.0` 的 `mpn/generic/invert.c`（GMP Newton 求逆）与 `sbpi1_div_qr.c`（预逆商修正），理论账本同源。

### 理论成本对照（记 C(n) = 长度 n 变换成本，∝ n·log n）

| 形态 | 每路 mulg 成本 | 除法首层合计 |
|---|---|---|
| 当前（full，2 fwd + 1 inv） | 3·C(lm) | ≈ 6·C(lm)（几何尾部 ×2） |
| + packing（fwd 2→1） | 2·C(lm) + O(n) | ≈ 4·C(lm) |
| + 半输出 inv（剪枝/negacyclic） | 1·C(lm) + ½C(lm) + O(n) | ≈ 3·C(lm) |
| + Karp–Markstein 省末路乘法 | 再省 ~1 路 | ≈ 2.5·C(lm) |

**文献锚点（Hanrot–Quercia–Zimmermann 2000, RR-3973；及 Kwon–Monagan 讲义）**：Middle-Product 版 Newton 求逆 = **2M(n)+O(n)**；若把 y_{k−1} 的 forward DFT 在迭代内复用一次 = **5/3·M(n)+O(n)**。这是 invertappr 的理论极限账本。
⚠️ **重要修正**：5/3 账本中的"y-DFT 复用"与上次实测证伪的 xh-DFT 复用**不矛盾**——上次死在 full-product 语境（两路乘积长度结构性失配，reuse2=0）；MP 语境下 y_{k−1}·mp 的长度结构由 HZ 公式保证匹配。**允许按论文公式重新实验，禁止直接套用旧 xform_b/mulg_fixb 代码**。

---

## 3. Cyclic/Negacyclic 评估（回答：能不能用？必须吗？）

**能用。测试点层面零障碍**：limb 基底 2^16 是 2 的幂，加权 twist 因子（ω^j 形式）与进制完全解耦，negacyclic/加权变换是纯实现层技巧，对任何进制输入等价成立，HEX 无特殊惩罚。

**是否必须**：分两个变体回答——

- **Variant A（推荐先行）：谱折叠 + 剪枝 IDFT**。不做真正的 cyclic 卷积，而是利用"长度 2n 的 IDFT 只取高 n 位输出 = 先 O(n) 蝶形折叠谱 + 一次半长 IDFT"（DIF 最后一级的逆用，精确等价、零精度损失变化）。改动只在 inv 端，正交于 fwd 端一切优化。**理论最优不必须它，但它是性价比最高的半输出路径。**
- **Variant B（理论极限形态）：真 negacyclic（mod x^n+1 加权变换）**。fwd 端也折半（lm → lm/2，twist 加权/去加权 O(n)），成本再砍半。**若目标是理论极限，inv 端半长化（A 或 B）是必须的；B 在 A 落地且稳定后再上。**

**必须性判定：packing（fwd 端）与半输出（inv 端）二者叠加才能逼近 2.5·C(lm) 账本，缺一不可。单独任一项都到不了理论区间。**

### 3.1 Cyclic 工作指导（Variant B 具体步骤）

数学基础：长度 h 的 negacyclic卷积 `N = a·b mod (x^h + 1)` 满足 `N_j = c_j − c_{j+h}`（c 为 full 积系数）。middle product 段 `Σ_j c_{j+h}·x^j` 通过对输入加权 `θ^j`（θ = h 次单位根的合适幂）平移后，恰好由 negacyclic 折叠量还原（Hanrot–Zimmermann, *The Middle Product Algorithm*；Crandall–Pomerance §9.4）。Newton 迭代需要的修正项天然是这种折叠形式，因此整个 invertappr 可用半长 negacyclic 实现，`I(n) = I(n/2) + 2·MP(n/2)`，总成本逼近 `~2M(n)` 量级。

实施步骤：
1. **先做 Variant A**（见 §4 P1.2），拿到半输出的谱折叠基础设施（`fold_spec_hi()`：输入谱 C 长 lm，输出折叠谱长 lm/2 + twist 表）。
2. 独立 harness（仿 `sr_compat.cpp` 模式）：小尺寸（n = 2^6..2^12）随机 limb 向量，对照 full FFT 乘法的高段输出，验证 middle product 精度 maxdiff < 1e-13，再上 26-case 字节门禁。
3. negacyclic fwd：split 后对 G/F 系数乘 `θ^j`（θ 表预生成，注意与 2 级 twiddle 表的共存，勿触发 §4 已知的 resize 污染坑）；变换长度 lm/2。
4. inv 端去加权 `θ^{−j}`，还原目标位段。
5. **精度预算重算**：折叠后系数幅值上界变化（low − high 符号抵消，幅值 ≤ max 而非 sum），`pick_k` 档位可能可以 +1 bit（白赚精度）或需 −1（保守），以 harness 实测定档。
6. 门禁：26 case SAME + I refs 严格下降，否则回退。

已知风险：
- 与 `difRecZeroHi`（zero-hi 剪枝）交互——pack/加权后高半不再全零，部分剪枝失效，**净收益以实测为准**。
- `pointwise_mixed` 会就地写坏 G 操作数（HANDOVER §13.2），新路径持独立副本。
- 任何独立变换前必须 `fft::resize` 对应尺寸（HANDOVER §13.1，递归会把 twiddle 表缩半）。

### 3.2 Cyclic 实现深坑清单（本轮补充，执行者逐条核对）

1. **负系数域（最大翻车点）**：negacyclic 折叠 `N_j = c_j − c_{j+h}` 产生**负中间值**。`split_b2`/`merge_b2` 全按非负 limbs 设计；FFT 中间值可负无妨，但 **`merge_b2` 的 round+进位累积只对非负谱成立**——去加权后必须先保证回到非负域（数学上 E ≥ 0、商段 ≥ 0，但取整误差可能给 −1），merge 前做条件修正（borrow 传播），或对最终 limb 向量跑一次 normalize。
2. **精度预算重推**：折叠幅值 ≤ max(lo,hi) 而非 lo+hi → 理论上可 +1 bit 档位；但 twist 乘引入新舍入误差项。`pick_k` 档位**必须 harness 全尺寸扫描实测**（2^6..2^20 maxdiff < 1e-13），不许笔算定档。
3. **对齐**：negacyclic 长度 h 与槽宽 k 的组合必须使折叠边界落在 64-bit limb 边界（h·k ≡ 0 mod 64 或显式位级修正）；不对齐则折叠跨界，修正逻辑复杂度爆炸。建议 h 直接取整 limb 对应谱长。
4. **mixed-radix 兼容（先查地形！）**：谱折叠蝶形是 radix-2 假设；`fft_ceil_tiers` 的 3·2^k/5·2^k 混合档需要推广公式或回退 full。**动手前先确认 MAX case 实际走的 path**——若走混合档，评估强制 2 幂档的长度损失 vs 半长化收益（小损失换大赚，实测定夺）。
5. **θ 表与 twbase 隔离**：twist 表独立静态存储，**绝不进 `fft::resize` 管理的全局表**（§13.1 污染坑重演风险）。
6. **Newton 递推照论文重推导**：negacyclic/MP 域的迭代公式以 HZ 论文 RMP 版为准，逐项对应；**禁止照抄现 full 域 `invertappr` 的递推结构换名字**。
7. **5/3·M(n) 追加实验（Variant B 之后）**：按 HZ/Kwon–Monagan 公式在 MP 语境复用 y_{k−1} 的 DFT（见 §2 修正），独立验收。
8. **harness 先行**：任何谱折叠/negacyclic 代码先进独立对照 harness（仿 `sr_compat.cpp` 流程），全尺寸 maxdiff 达标 + 26-case SAME 双门禁后才进主文件。

---

## 4. 分阶段路线（按性价比严格排序）

### P0：免费扫尾（≤半天，先做）
1. **small_0 构成解剖**：small_0=209M vs bzbound_0=14.7M 差距悬殊。callgrind 下钻，确认是否存在可削减的固定成本（表构建、首次 resize 全量重建）。若 MAX case 含同源固定成本，砍掉即白捡。
2. **阈值重扫**：`BZ_MIN / KD_QMAX / MULBF_MAX` 对 length_ratio_0 的 I refs 敏感性（复用 `tools/` 下 sweep 脚本模式）。
3. **I/O 占比下钻**：hex parse/print（3.2M chars）在 417M 中占比；>3% 则 pshufb SIMD 查表化（参考任何 SSE hex 转换惯用法）。

### P1：主攻——砍变换次数与无效输出（核心战场）

**P1.1 双实序列 packing（fwd 端，最大单项）**
- 改造 `mul_fft`：`pack = a + i·b` → 1 次 fwd → O(n) 共轭对称解包分离 A、G 两谱 → pointwise → 1 次 inv。
- 变换 3→2，预期总指令 **-12~18%**（difRec 份额 45.4% 的一半弱）。
- 注意：解包蝶形与 mixed-radix（3/5 路）路径的适配；zero-hi 剪枝部分失效需实测净收益。
- 注意：middle product 场景 G 段半零——packing 与 P1.2 的折叠正交，先后落地分开验收。

**P1.2 半输出 IDFT（inv 端，Variant A）**
- 新增 `fold_spec_hi()`：长度 2n 谱折叠成半长谱（O(n) 蝶形 + twist），再半长 IDFT 出高段输出。
- 落点：§2 账本中所有 middle product 乘法的 inv 端。
- 预期再 **-8~10%**。之后视收益决定是否上 Variant B（§3.1）。

**P1.3 short product（低位段乘法）**
- 余数修正 `qe × dn` 只需低 nb 位：谱折叠的 low 版本（对偶操作），复用 P1.2 基础设施。

**P1.4 Karp–Markstein 商修正融合**
- v 的最后一轮 Newton 迭代与 qe 商计算合并，省一路完整 mulg（~3-5%）。参考：Karp–Markstein 1997；GMP `sbpi1_div_qr.c` 的 mu 修正结构。
- 放在 P1.1/P1.2 稳定后做（依赖商估计误差界重推）。

### P2：指令层（Zen3 / AVX2，穿插进行）

| 杠杆 | 说明 |
|---|---|
| `vfmaddsubpd` | 若蝶形/twiddle 用 xor 符号翻转 trick（如 `pointwise_cross` 的 `_mm_xor_pd(cscale, cjm)`），换 vfmaddsub 系列省指令 |
| 叶核扩容 | `FFT_LEAF_LOG` 8→9/10 扫描（`sweep_leaf.py` 现成模式），更多 trivial butterfly、更少 twiddle 读 |
| twiddle 预旋转 | packing/negacyclic 的 twist 与蝶形融合（twist-per-pass），消 O(n) 独立遍历 |
| 转换指令 | `merge_b2` 的逐 double `(int64_t)(g+0.5)` 圆整是否可向量化（vroundpd + vcvtpd）——mulg 11.9% 的下钻项 |
| 存储端口 | Zen3 每 cycle 1×256b store；蝶形 store 密集处布局重排（先算齐再连续写） |

### P3：微架构收尾（用户指定最后做）

- **缓存**：MAX case 工作集 FB+GB ≈ 16MB（lm=2^20 doubles）vs Zen3 单 CCX L3 32MB——驻留可行但紧张；实验：THP 透明大页确认开启、`_mm_prefetch(T0)` 预取下一递归块的距离扫描、FB/GB 隔离页着色避免同 set 冲突。
- **跳转/分支**：difRec 递归基例分支转无分支调度；热点循环 32B 对齐；确认无 JCC 跨界（Zen3 无 SKL erratum，但对齐仍有益）。
- **验证方式注意**：callgrind **不模拟缓存/分支**——P3 的收益 callgrind 看不到，须以指令数不劣化为约束（callgrind 门禁照过），最终收益以 LC 提交回执的 wall-time 兑现。决策顺序：先保 I refs 不升，再赌缓存收益。

### 验收门禁（每一步，不变）
1. `cp 393027_opt.cpp 393027_opt_orig.cpp` 留原件；
2. VM66 构建（`g++-15 -O3 -std=c++20 -march=znver3 -mtune=znver3 -w`）；
3. 26 case 字节 SAME（`/tmp/vl.sh`）；
4. callgrind MAX I refs **严格 < 当前基线**，否则回退。

---

## 5. 资产与工具清单

| 资产 | 位置 | 用途 |
|---|---|---|
| 基线源码 | `D:\precious_speed\393027_opt.cpp` / VM66 `/tmp/opt_src.cpp` | 不可漂移的对照 |
| 死路实验存档 | `393027_opt_xfdft_experiment.cpp`、`393027_opt_flat.cpp` | 勿复活 |
| VM66 脚本 | `/tmp/vl.sh`（26-case 门禁）、`/tmp/cg.sh`（callgrind I refs）、`/tmp/pm.sh`（批量对比） | 直接复用 |
| 测试集 | VM66 `/home/azzr/precious_speed/cases_hex/*.in`（26 文件） | 门禁输入 |
| 题库官方 | `D:\library-checker-problems-master\big_integer\division_of_hex_big_integers` | gen/params/verifier 真源 |
| E 盘参考 | `E:\gmp-6.3.0`（invert/sbpi1_div_qr/mul_n 理论对标）、`E:\fftw-3.3.11`（cache-aware 变换布局参考）、`E:\library-checker-problems-master`（题库副本） | 算法参考 |
| 已知坑位 | `HANDOVER_HEX_DIVISION.md` §10、§13（resize 依赖 / pointwise 写坏 G / callgrind 多空格 / MinGW 对齐 / rnear_0 假信号 / 输入格式） | 必读 |

SSH：`ssh -i ~/.ssh/id_ed25519 azzr@192.168.1.66`（备用机 .55 同 key）。LC 提交头部保留 `// 喵喵喵~ https://space.bilibili.com/620657947`。

---

## 6. 给执行 agent 的执行序

```
P0.1 small_0 解剖 ──┐
P0.2 阈值重扫      ├─ 半天内收口，正收益即冻结
P0.3 I/O 下钻      ─┘
P1.1 packing（主战场）→ P1.2 半输出 IDFT → P1.3 short product
                     ↘ P2 指令层（穿插等编译/等门禁的空档）
P1.4 Karp–Markstein → Variant B（真 negacyclic，理论极限形态）
P3 微架构收尾（最后）→ LC 提交拿 wall-time 回执
```

每步产物：实验源码 + callgrind 数字表 + SAME/DIFF 记录，写回 `HANDOVER_HEX_DIVISION.md` 新 §。本人（指导）随时可出公式复盘、误差界推导、卡点会诊。

---

## 7. 参考资料清单（2026-08-17 定向检索，按优先级）

### 必读（理论 + 公式）
| 资料 | 位置 | 用途 |
|---|---|---|
| **Hanrot, Quercia, Zimmermann, "Speeding up the Division and Square Root of Power Series" (RR-3973, 2000)** | https://inria.hal.science/inria-00072675 （PDF 332KB 直下） | RMP（Recursive Middle Product）原始论文：MP 版 Newton 求逆/除法公式，inversion ~2M(n)（Karatsuba 基准 0.904K(n)）。**Variant B 公式唯一真源** |
| **Kwon–Monagan 讲义 (SFU, 2024)** | http://newweb.cecm.sfu.ca/CAG/talks/HyukhoLALO60.pdf | Newton inversion with Middle Product 的推导 slide 版（比论文易读）：y_{k−1} DFT 复用 → 5/3·M(n) 的出处；含 reciprocal polynomial 快除定理 |
| **Harvey, "A cache-friendly truncated FFT" (2008)** | https://arxiv.org/html/0810.3203v1 | Bailey 式 4-step/TFT 分解——**P3 缓存收尾的理论支撑**：difRec 递归只有最深层用 cache，改行/列分块让每层都吃到 |
| **Harvey & Roche, "An in-place truncated Fourier transform" (2010/2021)** | https://arxiv.org/html/1001.5272v1 | TFT/ITFT（前向+**逆向**剪枝）——P1.2 剪枝 IDFT 的算法骨架 |

### 工程内幕（省一周踩坑）
| 资料 | 位置 | 用途 |
|---|---|---|
| **FLINT `src/fft/README`** | http://wap.fossies.org/linux/flint/src/fft/README （本地可 pip/git 取 flint） | Bill Hart 手写实现笔记：truncation 两 case 处理、**"twiddles 融合进蝶形最底层 = 几乎零成本"**（P2 twist-per-pass 的实证）、IFFT 端跳过尾部 pointwise、revbin 时机 |
| **GMP 6.3.0 源码（本地）** | `E:\gmp-6.3.0`：`mpn/generic/invert.c`、`mpn/generic/sbpi1_div_qr.c`、`mpn/generic/mul_n.c` | Newton 求逆 + 商修正的工业实现；KM 修正结构对照 |
| **FFTW 3.3.11（本地）** | `E:\fftw-3.3.11` | cache-oblivious/递归切分布局参考（P3） |

### 竞速对标（同赛道）
| 资料 | 位置 | 用途 |
|---|---|---|
| **yuygfgg/Integer** | https://github.com/yuygfgg/Integer | LC 十进制多项记录保持库——看它 division 的路径调度常数 |
| **With-Sky/HyperInt** | https://github.com/With-Sky/HyperInt | 单头文件，FFT/NTT+Newton 除法+Karatsuba 调度；2^32 原生存储 |
| LC 排行榜 top 提交 | `D:\hex_precious_speed\web_best\div.cpp`（已扒） | 当前对手真身 |
| 洛谷 | — | 检索过，无超越上述资料的价值，低优先 |

### 检索建议（执行者后续）
- 优先把 RR-3973 PDF 下载到 `D:\precious_speed\refs\` 本地存档（避免链接失效）。
- FLINT 源码 `src/fft/` 整目录值得 clone 到本地对照（nmod_fft/mul_trunc 系列与 P1.2/P1.3 同构）。
