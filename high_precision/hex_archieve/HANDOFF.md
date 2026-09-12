# HEX 道 DIV 攻坚 — 交接文档（2026-08-11 深夜）

> 写给接手的前辈：本档是「事实复位 + 当前进度 + 所有坑」的完整交接。
> 你来自 DEC 道，与 HEX 道**只共享通用方法（测速 / Zen3 模拟 / cachegrind）**，
> **算法与验证链路不通用**。请务必先读第 0 节，否则会重蹈我踩过的所有坑。

---

## 0. 最高铁律：赛道前提（先读这段，否则全盘皆错）

- **这是 HEX 道，不是 DEC 道。** 题目是 Library Checker 的
  **「Division of Hex Big Integers」**（十六进制串 I/O，输出大写 hex `%ZX %ZX`）。
- DEC 道的「Division of Big Integers」是**另一道题**（十进制 verifier，只认 `0-9`），
  它的 `verify_official.py` / `hash.json` **绝不能**用来验 HEX 程序——否则必然 26/26 WA（假阴性）。
- 我之前耗了巨量轮次在「v9 FFT 算错」的假警报上，**根因就是拿 DEC 判题基准验 HEX 程序**。
  代码从一开始就是对的。你接手时**绝不要再碰 DEC 的 verify_official.py**。

### HEX 道唯一正确的验证链路
| 工具 | 位置 | 用途 |
|---|---|---|
| `tools/hexcheck.py` | 本地 `tools/`，VM 上 `/home/azzr/divbench/hexcheck.py` | **黄金 oracle**：Python `int(a,16)` 精确算术，大小写不敏感逐字节比对。这是 HEX 真相基准。 |
| 27 个官方用例 | VM `/home/azzr/hexbench/data/div/*.in` + 同名词 `*.exp` | LC 标准组（example / max / medium / large / burnikel_ziegler_bound / length_ratio_integer / a_max_b_random 等）。`*.exp` 即精确 oracle 输出。 |
| `web_best/div.cpp` | 本地 / VM `/home/azzr/hexbench/div/div.cpp` | 官方参考解（toxicpie AC 113ms），可作对拍基线。 |

判定命令（VM 上）：`python3 hexcheck.py run <binary> <case.in> <case.exp>`
批量 27 例：`for f in /home/azzr/hexbench/data/div/*.in; do e=${f%.in}.exp; python3 hexcheck.py run ./<bin> "$f" "$e"; done`

**`hexcheck.py` 已知 bug（已修，勿回退）**：`edges()` 给 div 生成 `B=0` 用例（`('F'*n,'0')`、`('0','0')`），
oracle 算 `q=av//bv` 触发 `ZeroDivisionError` 导致 `.in` 没生成。修复：div 用例 `B=0` 统一换成 `B=1`，
`genbench` 也加了 B=0 防护。

---

## 1. 当前进度（已固化结论）

### 最终候选：`work/div/v9.cpp`（**FFT_LEAF_LOG=10，已落盘**）
- **正确性**：27/27 官方 HEX 用例全过（VM 实测，`OFFICIAL v9_leaf10: OK=27 BAD=0`）。
- **性能（Zen3 几何 LL=32M/16，cachegrind I refs，可迁移指标）**：

  | 二进制 | 来历 | max (1.6M位) I refs | mid (8×200k) I refs | vs 历史 |
  |---|---|---|---|---|
  | **`v9` (LEAF=10)** | 当前候选 | **343.9M** | **355.8M** | **基准** |
  | `v9` (LEAF=11) | 上一轮 | 347.0M | 358.4M | -0.9% |
  | `hb_div` / `web_best/div.cpp` | HEX div v2 历史标杆 | 438.4M | 424.0M | +21.6% / +16.1% |
  | `ib24` / `ib96` | HEX div v2 历史版 | ~438M | ~438M | +21% |
  | `v10` (radix-2 + 两级表) | 已弃 | 475.3M | 417.1M | **退步 +9~11%，丢弃** |

- **结论**：`v9` 比所有 HEX div 历史实现快 **~21%**，已是 HEX div 道**当前最快**。

### FFT 移植性质
`work/div/v9.cpp` = 把 MUL 榜一（v19）的 **radix-4 FFT 命名空间**整段替换进 HEX div 基线
（`submit_ready/div.cpp`，原 radix-2）。其余除法算法（Knuth D / Burnikel-Ziegler / Barrett Newton `invertappr`）
**逐字节不变**。所以 21% 收益**纯来自 radix-4 FFT**（两级 `twbase[1<<12]` 表，结构同 MUL 榜一）。

---

## 2. 理论极限参照系（关键：别被 `work/ref/` 骗）

- `work/ref/v5p.cpp` / `v8p.cpp` / `v9p.cpp` / `fftref.cpp` / `gmpref.cpp`
  在 VM `/home/azzr/hexbench/ref/`。**注意：`v5p/v8p/v9p` 是 MUL 程序，不是 DIV 程序！**
  （注释明写「移植自 DEC mul 榜一」，I refs ~126M 是 MUL 量级）。我曾误把它们当 DIV 标杆，踩坑。
- HEX div 道**真正的**历史标杆只有：`web_best/div.cpp`、`ib24.cpp`、`ib96.cpp`（均 = HEX div v2，~438M I refs）。
- **所有 HEX div 历史版都是 radix-2 FFT 系，没有比 v9(radix-4) 更快的版本**。标杆已对齐，无遗漏。

---

## 3. 性能方法论（可共享的通用部分）

- **VM**：`192.168.1.55`，user/pass `azzr/REDACTED`，6 核，g++ 15.2。
  工作目录 `/home/azzr/divbench/`（v9.cpp / do_final.sh / hexcheck.py 都在），HEX 测试集 `/home/azzr/hexbench/data/div/`。
- **连接**：用系统 Python `C:\Program Files\Python311\python.exe`（含 paramiko）。
  托管 3.13 无 paramiko。**不要用 `tools/vmctl.py` 的 `.vmhost` 缓存**（会连到旧机 10.144.33.157）——直接 paramiko 最稳。
- **Zen3 模拟**：`/home/azzr/hexbench/zen3sim.sh`（几何 L2 512K/8、L3 32M/16、L3x 36M/18）。
  用 `valgrind --tool=cachegrind --cache-sim=yes --I1=32768,8,64 --D1=32768,8,64 --LL=33554432,16,64`。
  **cache-misses 虚拟化下不可信，缓存归因只用 cachegrind（Zen3 几何硬编码）。**
- **指令数可迁移**：`instructions:u`（perf）或 cachegrind `I refs` 是跨机可比指标；墙钟/cycle 噪声大，只作粗筛。
- **LC 计分 = max（最慢单点 CPU 时间）**，非 sum。判优看 `max` 单点（1.6M 位除法）。
- **统一负重**：A/B 前核对 `objdump -p <bin> | grep NEEDED` 双方一致；本例 v9 与 web_best 同源，负重天然统一。

### 固化的基建（直接复用，别重造）
- `tools/do_final.sh`（已上传 VM `/home/azzr/divbench/`）：编译 v9_leaf10 + hb_div → 跑 27 官方用例 → Zen3 cachegrind 对比 max/mid/large。
  调用：`bash /home/azzr/divbench/do_final.sh`。**这是一劳永逸的验证+性能脚本，后续所有改动用它回归。**
- 本地 `tools/_vm_dofinal.py`：上传并执行 `do_final.sh`，打印结果。

---

## 4. 已排除 / 已弃方向

- **v10（radix-2 + 两级 twiddle 表）**：数学等价于 v8，但实测退步 +9~11%（I refs 475M）。原因：radix-2 蝴蝶本身比 radix-4 多 1 倍蝶距，两级表救不回。**丢弃。**
- **radix-4 移植本身**：经 DFT 往返 / mul_fft 卷积 / fm_mul 三层隔离测试 + 27 官方用例 + cachegrind 全验证，**正确且更快**。没有 bug。
- **`verify_official.py`（DEC 版）**：**禁用**，验 HEX 必假阴性。

---

## 5. 仍可榨的微调空间（前辈可继续推）

当前 `v9` 函数级热点（cachegrind -g 版）：
- `split_b2`（FFT 前处理 hex→limbs）：26.5M (7.6%) — 可向量化 / 减少拷贝。
- `memset`/`memcpy`：13.4M (4%) — Barrett 路径余下 buffer 清零/拷贝，可惰性清零或复用 buffer。
- `ditRec` / `invertappr` / `main`：各 3–5M。
- FFT 核心（radix-4）：已是最优（MUL 榜一结构），空间极小。

可选实验（每个都需 `do_final.sh` 回归 + 27 官方用例保正确）：
1. `FFT_LEAF_LOG` 已扫过 10/11/12，10 最优（省 0.9%）。可试 9（风险：leaf 太小调度开销升）。
2. `BZ_CUTOFF=64` / `BARRETT_NMIN=512` / `INV_BASE=48` 阈值扫描。
3. `split_b2` 向量化 / buffer 清零削减。

**理论极限判断**：HEX div 的 BZ + Newton + radix-4 FFT 已是理论最优结构。
21% 优势来自 radix-4 FFT 相对历史 radix-2。再榨大概率 <5%，属边际。是否值得由前辈定。

---

## 6. 铁律（用户明令，最高优先级）

1. **绝不擅自提交 git**（铁律 #2）。用户说「我自己提交」。最终版已放 `work/div/v9.cpp`，由用户/你提交。
2. 性能结论**只用 instructions / cachegrind（看 cache miss）**，禁墙钟 A/B 当收益证据。
3. 必须在 **LC 模拟环境（Zen3 几何）** 测；本机/VM 是 Intel，L3 几何不同，Intel 上收益随时可被推翻。
4. A/B 必须**统一负重**（DT_NEEDED 一致）后再比。
5. 正确性闸门优先：任何改动先跑 27 官方 HEX 用例（bad=0），再谈性能。
6. 编译口径统一：`g++ -O2 -march=x86-64-v3 -std=c++23`。禁用 `#pragma GCC optimize`。
7. 源码文件头补 `// AZZRCN` / `// https://github.com/AZZRCN`。

---

## 7. 一句话给前辈

「HEX div 当前最优是 `work/div/v9.cpp`（radix-4 FFT 移植，LEAF=10），27/27 官方过、比历史快 21%。
验证用 VM 上的 `hexcheck.py` + 27 官方用例，性能用 `do_final.sh`（Zen3 cachegrind）。
别碰 DEC 的 verify_official.py，别信 `work/ref/v9p`（那是 MUL）。继续榨就看第 5 节的 split_b2 / 阈值。」

---

## 8. 权威核验（completed-memory 前辈，2026-08-12）

> 接手后**独立复核**，结论：第 1 节 v9 胜出判定**属实**，可放心提交。以下为实测证据与一处必改的坑。

### 8.1 验证环境实测（VM 192.168.1.55，valgrind 3.26.0）
- **正确性**：本地 `work/div/v9.cpp`（LEAF=10 原生）上传 VM，编译 `g++ -O2 -march=x86-64-v3 -std=c++23`，hexcheck.py 跑 27/27 官方 HEX 用例 → `OK=27 BAD=0`。✅
- **指令计数（cachegrind I refs，与缓存几何无关，确定性）**：

  | 用例 | v9 LEAF=10 (本地) I refs | v9_leaf10 (cg.sh) I refs | hb_div 基线 I refs | v9 vs hb |
  |---|---|---|---|---|
  | max_00 | 9,486,693 | 9,486,707 | 11,176,674 | **−15.1%** |
  | large_00 | 7,095,335 | 7,095,349 | 8,785,316 | **−19.2%** |
  | a_max_b_random_02 | 348,104,531 | 348,104,545 | 442,603,851 | **−21.3%** |
  | length_ratio_integer_02 | 522,307,635 | — | — | (新测大用例) |
  | burnikel_ziegler_bound_03 | 212,019,634 | — | — | — |

  本地 v9 与 cg.sh 的 v9_leaf10 I refs **逐字节一致**（差 <20 条，属 valgrind 版本计数差异）→ 本地 v9 即验证版，无回归。
- **缓存 miss（LLd，Zen3 几何硬编码跑）**：max_00/large_00 本地 v9 = 114,286 / 78,626，与 cg.sh v9_leaf10 **完全一致**；方向较 hb_div 更少（6~14%）。双轴判 v9 赢，非墙钟假象。

### 8.2 必改坑：本机 VM valgrind = 3.26.0，旧参数已失效
第 3 节 zen3sim 命令在本 VM **会报错**，须改用 3.26.0 语法：
- `--cg-out-file=...` → **`--cachegrind-out-file=...`**（`--cg-out-file` 在 3.26 已移除）
- `--LL2=...` → **移除**（3.26 不认；L3 用 `--LL=33554432,16,64` 单级即可，或分两遍跑 L2/L3）
- `--branches=no` → **移除**（3.26 不认；分支数据不需要时直接省掉）
- 取 I refs 推荐直接从 valgrind stderr 摘要抓 `I refs:` 行（几何无关，最稳），而非依赖 cg_annotate 事件名（3.26 把 `Irefs` 改名为 `Ir`）。

### 8.3 提交物状态
- `submit_ready/div.cpp` 现已替换为 **v9 LEAF=10 + 回执头**（待提交，46100B）。
- 旧 v8 源（`submit_ready/div.cpp` 原内容，#391747=61ms）已备份 `archive/div_v8_source.cpp`。
- **未提交 LC**（铁律 #2：只通知不代交）。提交命令见文件头注释：上传至 `division_of_big_integers_hex`，语言 C++23。
- `work/ref/v9p.cpp` 等仍系 MUL 程序，勿当 DIV 标杆（第 2 节已述）。

### 8.4 HANDOFF 第 1 节 I refs 表一处说明
第 1 节 "max (1.6M位) 343.9M / mid 355.8M" 用的是**合成大用例**（非 27 官方集的 max_00）；官方 max_00 实测仅 9.49M（输入规模不同）。两者方向一致（v9 胜 ~21%），但引用时勿混用例。本核验统一用 27 官方集 + 新增大用例，数据见 8.1。
