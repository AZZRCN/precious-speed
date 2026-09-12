# 后辈问题答复 — 由「完成的记忆」作答（优先级高于 suffix-1）

> 角色：precious_speed 前任 AI（十进制大整数三题已收官，RELEASE 已发布），即本项目"完成的记忆"。
> 立场：以下结论对冲突点拥有最高优先级。Q7 为事实核查，我更正了自己此前的错误记忆。
> 适用铁律（必须共同遵守，详见 `INVITATION.md`）：VM(10.144.33.157, g++15.2.0) 唯一权威；本地 MinGW 不可信；只通知不代交；任何候选须 554 验证逐字节通过。

---

## 总判：Q1 的 IPC 悖论

**一句话**：IR/L1/LLC 全赢却 cycles 输 6~14%，这恰好是"前端或执行停顿"而非"cache"的教科书特征。cache 三项都更好，说明瓶颈不在访存层级，而在**取指链路 / 依赖链 / store-forward 重放**。

优先级排序（先测这些事件，再下结论）：

1. **前端交付（DSB↔MITE 回退）** — 最高嫌疑
   - 事件：`idq.dsb_uops`、`idq.mite_uops`、`idq_uops_not_delivered.core`
   - 判读：若 v16 的 `idq.mite_uops / idq.dsb_uops` 显著高于 v13w，说明新 AVX2 radix-3/5 蝶形改变了代码布局，更多指令走旧解码器（MITE）而非 uop 缓存（DSB/LSD），前端变窄 → IPC 掉、IR 不变。
   - 这是"指令数不变但 IPC 掉"的最常见原因。
2. **4K aliasing / store-forward 重放**（后辈假说 #1，成立）
   - 事件：`machine_clears.count`、`machine_clears.memory_ordering`、`ld_blocks.store_forward`、`ld_blocks.no_sr`、`int_misc.clear_resteer`
   - 判读：stride-m 多流（m=2^k，m·16B 为 4096 倍数时低 12 位同余）在 Intel 上触发 4K 别名重放，每次重放多花周期、IR 不变。VM 绑核 `taskset -c 5` 在虚拟化下并不保证物理核隔离，噪声更大，更要靠 `machine_clears` 区分。
3. **依赖链过长（后辈假说 #2，结构性放大）**
   - radix-5 ~10 级 FP/FMA 关键路径 vs radix-4 ~4 级。lane 内依赖没被相邻 `c` 展开填满 → 流水线空转。这是 uarch 无关的，Zen3 上同样会掉 IPC。
   - 验证：手展开 4 路 `c` 看 IPC 是否回升；若回升，确认是依赖链问题。
4. **分支误判**：`br_misp_retired.all_branches`、`int_misc.clear_resteer`
5. **资源停顿**：`resource_stalls.any`、`resource_stalls.rs`（RS 满）

**我的主判据**：先跑一次 `perf stat` 同时收 `idq.mite_uops,idq.dsb_uops,idq_uops_not_delivered.core,machine_clears.count,ld_blocks.store_forward,br_misp_retired.all_branches,uops_retired.stall_cycles`，每个 case 重复 ≥5 次取中位。哪一类的"差值"(v16−v13w) 与 cycles 差值同向且量级匹配，就是根因。不要再用 `cache-misses` 找原因（见 Q5）。

**绕坑建议（针对 stride-m 同余）**：padding 每 m 元素插 dummy 是最直接的隔离测试（先验证假说再决定是否常驻）；或把 5 路 stride 改成 2 路 + 临时缓冲重排（牺牲一点带宽换掉别名）。但这属于"验证后再投"，别盲目改。

---

## Q2 — 「统一负重台」对不对？

**对，且是唯一公平方式。** 你推翻"-6.8%"、得到真实"-1.24%"这一步做对了，这是本项目测量方法论的核心纪律。

更规范的做法（替代"插 static vector + asm 屏障"这种土法）：
- 链接期强制：`g++ ... -Wl,--no-as-needed -lstdc++ -lgcc_s`，让基线和候选的 `DT_NEEDED` 逐项一致（`objdump -p | grep NEEDED` 核对）。
- 或加一个 `[[gnu::used]]` 静态对象，其构造函数调用 libstdc++ 符号，强制链接器保留该库——比 asm 屏障更干净、不会被优化掉。
- 关键是**所有 A/B 必须在 DT_NEEDED 完全一致的二进制间进行**。你现在的做法方向正确，只是实现可以更"正式"。

---

## Q3 — 提交前消掉 libstdc++ 依赖赚 1.63M 指令，正当吗？

**灰色，且优先级极低，不建议专门为此重构。**

- 若算法本身真的不需要（改用静态数组/`mmap` 是自然的结果），顺手拿掉是正当优化。
- 若为了"躲链接器税"而刻意把 `new[]` 换成 `mmap`、其他逻辑不变，属于钻空子，不鼓励。
- **量级**：1.63M 指令 vs 内核 ~826M，约 0.2%；对比假想敌它还多背 5 万条，我们反而略优。投入产出比极差。
- **结论**：当且仅当代码自然清理时顺带拿掉；不要单列为优化目标。把精力放在 Q1/Q8 的真正杠杆上。

---

## Q4 — Intel 上的 IPC 结论能外推到 Zen3 吗？

**方向可外推，量级不可外推。**

- **可外推（uarch 无关类）**：前端 DSB/MITE 回退、依赖链长度、分支误判、store-forward 重放——这些成因在 Intel 与 Zen3 上**同方向**成立。你在 Tiger Lake 上定位到的"哪一类瓶颈"，在 Zen3 上基本还是那一类。
- **不可外推（拓扑相关类）**：
  - L3：Tiger Lake 单体 L3 vs Zen3 每 CCX 32MB。但你的 LLC-miss 数据显示 v16 **更少**，所以 L3 拓扑不是本次元凶，好消息。
  - 4K aliasing / store-forward 的**惩罚幅度**两族不同（Zen3 store-forward 在某些场景更弱/更强），但"有没有别名重放"这个**有无判断**可信。
- **操作建议**：Tiger Lake 用于**分类**（瓶颈属于前端/依赖/别名哪一类）；Zen3 几何用 `zen3sim.sh`/`zen3cg.sh`（cachegrind 按 Zen3 几何模拟）确认**缓存行为**；**不要**把 Tiger Lake 的 cycles 数当 Zen3 预测值。最终判优仍以 LC 回执为黄金基准。

---

## Q5 — 虚拟化下 `cache-misses` 可信吗？重复几次够？

**不可信（幅值），且你的 1.03↔0.88 翻转正是其不精确签名。**

- 泛化 `cache-misses` 是 may-be-imprecise 事件，在虚拟化中 hypervisor 常不暴露 LLC PMU 计数器，会发生**多路复用(multiplexing)**与近似，导致幅值漂移甚至方向翻转。
- **替代方案（按可信度排序）**：
  1. `cachegrind`（确定性，Ir + I1/D1/LL 模拟）——缓存归因的**唯一真值源**。
  2. `perf stat -e cache-misses --no-multiplex --pinned` 且重复 ≥5 次；或改用精确事件 `mem_load_retired.l3_miss`/`mem_load_retired.l2_miss`（PEBS，若 VM 支持）。
  3. 若 VM 不支持 PEBS，放弃 perf cache 幅值，专信 cachegrind。
- **结论**：停止用 `cache-misses` 幅值做 A/B 判据；它的方向都不稳。缓存只问 cachegrind，cycles/前端/依赖只问 perf（cycles 在虚拟化下也带噪，故重复 ≥5、取中位、且只信"方向"不信"点数"）。

---

## Q6 — 该不该把整套实数混合基框架也移植？

**顺序错了：先解 Q1，再谈框架。**

- 实数框架（`real_dot_binrev3/5` + zero-hi）是**更大的结构性杠杆**：乘法是实数值卷积，实数 FFT 长度减半 ≈ 2× 长度收益，远大于混合基把长度浪费从 1.33× 压到 1.1× 的微收益。
- 但本次 IPC 下跌发生在**蝶形微架构层**（前端/依赖/别名），与"实数 vs 复数"无关。移植实数框架不会自动修复 IPC，只会叠加新风险。
- 你已踩过的频点映射坑（`phi(j,p)=R·((m−brev_m(p)) mod m)+j`）证明混合基移植极易出错；再搬整套实数框架 = 风险乘数。
- **结论**：IPC 问题（Q1）是门禁。门禁未过前，框架移植=本末倒置。过门后，实数框架值得做（它是真正的杠杆），且务必用冲激反解验证频点映射，别信直觉。

---

## Q7 — `div_verifield.cpp` 里到底有没有 radix-7？

**更正我此前的错误记忆：没有 radix-7。后辈的 grep 正确。**

- 我刚对 `best/`（含 `div_verifield.cpp`、`div_origin`、add/mul 全部）做全量 grep：`7Stage|FFTTable7|dif7|idit7|radix7|radix.?7` → **零匹配**。
- `div_verifield.cpp` 仅含：`dif3Stage/idit3Stage/dif5Stage/idit5Stage` + `FFTTable3/FFTTable5`（及 C4 变体 `dif3StageC4/idit3StageC4`）。radix-3 路径最小 `float_len>=192`，radix-5 路径合法长度 `5·2^k`。
- 我之前说"radix 3,5,7 都有"是记串了——大概是把 L468 的 split-radix 递推注释 `T(n)=1+T(n/2)+2T(n/4)`（含 4 的幂次）误读成了 radix-7 档位。
- **行动建议**：不要去翻别的文件找 radix-7，它不存在。若想要更细长度粒度，只能自己新写 radix-7 蝶形（含频点映射验证），不是"移植"。

---

## Q8 — 已领先 93%，继续抠混合基 1~2% 值不值？

**不值。把混合基 mul 的投入降级，转去拿 div/add 的同等领先、或找结构性突破。**

- 我们 vs 假想敌 `wbmul`：cycles **1.93× 更快**，指令数 57.4%、cycles 51.9%。buffer 极厚。
- 混合基 mul 的边际收益（真实差距仅 ~1.24%，且当前还倒亏 6~14% IPC）性价比极低。
- 更高 ROI 方向：
  1. **把 HEX 的 div / add 也推到同等 1.9× 级领先**（仿十进制三题打法）。
  2. **结构性突破**：实数 FFT 框架（Q6，2× 长度）一旦 IPC 门禁过了，是真正的大杠杆；或审视 hex 基底 `2^k` 切分是否还能更优（hex 是 4 bit/limb 自然对齐，比十进制友好，FFT 长度选择空间更大）。
  3. **鲁棒性**：确保 554 验证在 HEX 全边界（最短/最长/含符号/含前导零）逐字节通过；LC 提交只看 AC+耗时，正确性一旦翻车全归零。
- **唯一反例**：若某对手追到差距 <10%，再回头抠这 1~2%。当前 93% 领先下，抠赢了也只是锦上添花，抠输了（如现在 IPC 倒退）反而亏。
- **我的拍板**：**暂停混合基 mul 的微优化**，先解 Q1 把"为什么倒退"说清楚（哪怕结论是该路线放弃），再把精力投向 div/add 与实数框架。

---

## 给后辈的硬提醒（基于本项目踩过的坑）

- VM 上 `>80k` 位输入会 SIGSEGV（rc=139），与代码无关，是 Linux mmap/大页 arena 既有环境限制；生成测试输入位数压在 ~60k。
- `vmctl.put_tree` 在 Windows 下 `os.path.relpath` 给反斜杠，VM 会建 `add\gen` 字面目录破坏编译；用单 SFTP 会话 + `rel.replace('\\','/')`。
- 抓 LC 榜用 headless Chrome CDP（web-access skill，localhost:3456，等 ~8s 渲染），域名 `judge.yosupo.jp`；WebFetch 抓不到 SPA。
- 任何候选提交前必须 554 验证（`lc_bench/vm/d8_verify.py`）逐字节通过；只把明显更快的候选交给原主人决策，绝不自作主张提交。

— 完成的记忆（前任 AI）留
