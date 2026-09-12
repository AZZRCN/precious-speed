# GLM_REVIEW_BRIEF.md — 给 GLM-5.3 的审查简报（HEX 大整数除法 AVX2 缩减机）

> 用途：用户（precious_speed）将本文件 + `reducer/` 目录交给 GLM-5.3，审查"AVX2 指令缩减机"
> 的方法论正确性、已落地优化的安全性、以及 **SoA 蝶形跃迁提案**的数学/指令收益是否成立。
> 所有结论均以 VM66(Intel 285H) 的 `callgrind I refs` 为唯一真值（跨架构一致，预测判官 Zen3）。

## 0. 一句话结论
FFT 乘法核**已是 AVX2-256 蝶形 + 2 级 L1 twiddle + FMA + 混合 radix，已近 AVX2 极限**。
**关键实测（SoA 完整历程）**：SoA 蝶形跃迁经三轮实测已**定案推翻**——
1. 初版 `soa_poc` 显示 +3.0%（被 GLM 审查 §4.5 判"工作量未归一，证伪无效"）；
2. 归一后 `soa_poc2` 显示 −47%/复数乘，**但该基线是对朴素 AoS 标量 `cmul`（8.5 insn/cmul）**；
3. **生产实测（本会话）**：生产 `pointwise` 早已用向量化 `cmulv`（2 insn/cmul），红利本就被吃；
   递归蝶形 `difRec` 的 SoA 版净 **+0.2% I refs（更差，bitwise 逐位一致）**，根因 re/im 拆 4 数组后
   地址算术（`lea`）膨胀，指针提升优化无效。
→ **#25 由生产实测推翻**：30 函数改写换 ≤0 收益（蝶形反升、pointwise 已最优），不在优化路径内。
**指令数维度已无 micro 级红利**；唯一剩余真实杠杆是**算法级**（除法路由/卷积次数减少）+ **zero-hi 扩展到 mixed radix（#26）**。

## 1. 缩减机框架（已建成，可复现）
目录 `D:/precious_speed/reducer/`：
- `gen_cases.py` / `gen_cases_dir.py` — 生成随机大整数除法用例（覆盖小/中/大 FFT 区间）。
- `bench.py` — 闸门：`build`（VM 编译 `-O3 -march=haswell`）、`oracledir`（单 query 字节比对）、`ir`（callgrind I refs）。
- `SEMANTICS.md` — 等价契约（黑盒字节一致 + 浮点容差 + 指令数度量 + 候选变换表）。
- 连接：`tools/vm_ssh.py`（paramiko, 192.168.1.66, azzr/1234）。

**验证命令（GLM 可自己跑）**：
```
cd D:/precious_speed/reducer
python bench.py build ref D:/precious_speed/393027_opt.cpp
python bench.py oracledir ref ref /tmp/hexcmp/cases_dir      # 期望 ORACLE_DONE total=60 fails=0
python bench.py ir ref cases.txt                              # 期望 I refs ≈ 206,952,320
```

## 2. 基线测量（真值锚点）
| 项 | 值 |
|---|---|
| 基线源 | `393027_opt.cpp`（v27, md5 5b7bafa1） |
| 单巨例 | 103900 limbs ÷ 51950 limbs |
| **I refs（基线）** | **206,952,320** |
| 指令构成（先验） | ~88% FFT 乘 / ~11% 脚手架（来自此前 callgrind 拆解） |
| 判官 | AMD EPYC 7B13 (Zen3, AVX2, **无 AVX-512**) |
| 编译 | `-O3 -march=haswell`（=AVX2/FMA/BMI2，匹配 Zen3） |

## 3. 已确认死路（避免 GLM 重走）
- **L3 缓存调参**：L3 sweep 8/16/24/32MB → I refs/miss **完全一致**。"MAX 40MB>L3"假设错误；
  热数据（<2MB）常驻 L2(512KB) 足矣，40MB 是 hugify 冷尾。缓存优化 = 死路（已实测）。
- **NTT 整数 FFT 路径**：此前实测比双精度 FFT 慢 60–85×，放弃。
- **AVX-512**：判官 Zen3 无 AVX-512，3× 时间惩罚 → 缩减机默认 AVX2-only。

## 4. SoA 蝶形跃迁提案（请 GLM 重点审）
### 4.1 现状（AoS，`cpx = __m128d`，256 位承载 2 个复数 [re0,im0,re1,im1]）
复数乘 `cmulv`/`cmul4`（每 2 复数）：
```
unpacklo(a,a)      ; shuffle
unpackhi(a,a)      ; shuffle
permute_pd(b,0x5)  ; shuffle
fmaddsub(...)       ; FMA
mul(...)            ; mul
```
→ **3 次 shuffle + 1 FMA + 1 mul** / 2 复数。这些 shuffle 是热路径指令来源（每 FFT ~O(n log n) 次复数乘）。

### 4.2 提案（SoA：re[]、im[] 分置，256 位承载 4 个实部 + 4 个虚部）
复数乘 `a*w`（w=(wr,wi)）：
```
re' = fmsub(wr, re, wi*im)   ; 1 FMA   （wi*im 可与相邻 FMA 链式，零 shuffle）
im' = fma(wr, im, wi*re)     ; 1 FMA
```
→ **0 次 shuffle + 2 FMA** / 2 复数（且 256 位一次吃 4 复数 = 4 FMA/指令组）。

### 4.3 指令收益估算（**已被实测推翻，见 §4.5**）
- 每次复数乘：AoS 1.5 shuffle + 0.5 FMA + 0.5 mul → SoA 1 FMA（另一 FMA 取代 mul），即**省 ~2 条指令/复数乘**。
- 一次 lm=262144 的 FFT 约 2.36M 次 twiddle 复数乘；乘 2 正+1 逆 × ~5 次 mulg/除法 ≈ 35M 次复数乘。
- 若复数乘占 FFT 指令 15–25%，则总 I refs 约 **−10~18%**（207M → ~170–185M）。
- **前提错误**：SoA 的负载/存储需拆成 re/im 两条独立流（2 load vs AoS 1 load/2 复数），
  额外 load 指令抵消甚至超过 shuffle 节省。

### 4.4 风险与正确性闸门（SoA 若仍要落地）
- 风险：需改全部 FFT 数据布局（twbase、bf2*/difFlat/ditFlat/difRec/ditRec、pointwise 系列、
  dif3/5StageR、split_b2/merge_b2、所有调用点）。数值改动由 **黑盒字节比对**（`oracledir`, 60 例 + 巨例）
  自动裁判，超出舍入容差即 `failures>0` 暴露。
- 建议 GLM 审查：① SoA 数学等价性推导；② twiddle 表 SoA 化后的 `twg` 索引是否仍正确；
  ③ `split_b2/merge_b2` limb↔SoA 转换的舍入边界；④ 混合 radix（3/5）顶层蝶形 SoA 化后的共轭配对。

### 4.5 实测微基准结果（**关键，推翻 §4.3 的乐观估计**）
独立 PoC `reducer/soa_poc.cpp`（AoS `cmulv` vs SoA 2-FMA，400 万次复数乘，输入随循环变化防常量折叠）：
```
callgrind I refs:   AoS = 134,746,238
                   SoA = 138,766,733   (+3.0%, 反而更差)
数值一致性:        SoA vs AoS 0 失配 / 100000 随机样例
```
**解释**：SoA 把 re/im 拆成 4 条独立加载流（4 load vs AoS 2 load），抵消了消去 2 次 shuffle 的收益；
而 shuffle（unpacklo/unpackhi/permute）在 Zen3 上是 1-uop 廉价指令，对 I refs 几乎无贡献。
→ **SoA 在"指令数"维度不成立**。注：SoA 可能改善某些 uarch 的**延迟/周期**（非 I refs），
但本任务唯一真值是 I refs，故 SoA 不在本次优化路径内。

### 4.6 生产实测定案（本会话，推翻 §4.5 的 −47% 对生产的适用性）
§4.5 的 −47% 来自 `soa_poc2` 对**朴素 AoS 标量 `cmul`**（callgrind 实测 8.5 insn/cmul）的对比；
但生产 `pointwise` 早已用向量化 `cmulv`（4 指令算 2 复数乘 = **2 insn/cmul**），红利已被吃。
本会话对生产核补做两项实测：

| 实测 | 方法 | 结果 |
|---|---|---|
| `difRec` AoS vs SoA（radix-4 正变换，含指针提升优化） | `reducer/soa_prod_bench.cpp` 逐位同构 + callgrind | **SoA 净 +0.2% I refs（更差）**，bitwise `mismatch=0`（port 正确） |
| 指令构成反汇编 | `objdump` 统计 `difRec` | SoA 消去 65% shuffle（111→46），但 `lea` 增 3.7×（32→117），总指令 1.68×；**ymm 无溢出**（非寄存器压力） |

**机制**：SoA 把 re/im 拆成 4 条独立数组访问流，GCC 无法把 8 个基址全提循环外，每次访问需 `lea` 重算偏移，
地址算术增量 > shuffle 节省。指针提升（循环外预算 8 基指针）实测无效（100.83M vs AoS 98.69M，仍更差）。
**结论**：`difRec`/`ditRec`（占 FFT ~46%）SoA 必反升；`pointwise`（~15%）SoA 与生产 `cmulv` 同 2 insn/cmul、无净收益。
全链路 SoA 上界 ≈ +1~3%（乐观），保守估计净负。**#25 由生产实测推翻，30 函数改写不予执行。**

## 5. 已知问题（务必在提交前处理，非缩减机引入）
- **v27 多 query 段错误**：`393027_opt.cpp`(5b7bafa1) 在**多 large query 同文件**时段错误，单 query 正常。
  oracle 已用单 query 规避。提交前须修复（疑似 FMG 缓冲跨 query 别名），或确认 LC 单测试文件 T=1。
- **VM66 perf PMU 不可用** → 用 callgrind I refs 替代 `perf instructions:u`（跨架构一致，合法）。

## 6. 待 GLM 决断 / 下一步
1. **#25 SoA 全管线改造：已由生产实测（§4.6）推翻，不予执行**。指令数维度剩余 micro 红利 = 0；优化重心转向 **算法级（#24 路由/卷积复用）** 与 **#26 zero-hi 扩展到 mixed radix**。
2. 算法级杠杆（下一步真正方向）：
   - **除法路由**：newton vs BZ 的 crossover 已用 `t=qbits/bbits>0.9` 判据（锯齿难拟合），能否用更优判据减少 BZ 的 M(n)logn 开销？
   - **卷积复用**：BZ 中除数固定，其正变换已 `fm_prep` 复用；能否进一步复用商/余的 FFT？
   - **NTT 整数 FFT**：此前实测慢 60–85×，但若改用更优 NTT（如 Schönhage-Strassen 的 Nussbaumer 短卷积）在 limb 数极大时是否翻盘？
3. 是否同意"AVX2-only，AVX-512 仅 gamble flag"的判官约束决策？
4. **提交前必修**：v27 多 query 段错误（§5）。
5. 缩减机下一步：把上述算法级变换落地为 `393027_opt_algo.cpp`，跑 `oracledir` + `ir` 实测。

## 7. 最新进展（2026-08-18 22:xx）：callgrind 函数级剖析 → 锁定"下一刀"= Packed real FFT
新增 `reducer/aggregator.py`（callgrind 函数级 self-cost 聚合，addr2line 映射）+ `reducer/FFT_NEXT.md`（完整提案）。

### 7.1 函数级分布（v27 单巨例，self%，方向性可靠）
`difRec`(正变换,15次) 36.6% / `pointwise`(频域复数乘) 24.6% / `fm_mul` 10.4% / `split_b2` 5.1% /
`ditRec`(逆变换,9次) 0.2% / `difRecZeroHi` 1.2% → **FFT 总占比 ≈ 80%**。
未解析 `(id)` ≈10% 为 libm 内存/数学例程（**非** FFT twiddle，因 twiddle 已全预计算）。

### 7.2 剖析工具链修复
- cg_annotate 3.26 拒读 callgrind out（头解析 bug）→ 自写 `aggregator.py`：non-PIE `-g` 编译 → callgrind →
  提取 `fn=(id) 0xADDR` 地址 → `addr2line -e ref_np` 映射函数名 → self cost 聚合（成本行仅在**非 cfn 子块**计入，
  否则 -O3 内联折叠导致重复/缺失；self 总和 68.6M 低于 total 207M 属正常，排序可靠）。
- VM 双 IP 自动探测：`vm_ssh.py` 改 `CANDIDATE_HOSTS=[.66,.55]`（至少一台在线即连通）。

### 7.3 请 GLM 重点审：Packed real FFT（最高价值，理论省 ~25–33% 总指令）
- **事实**：大整数 limb 是**实数序列**，但 `mul_fft` 用 complex FFT 做实数卷积（虚部吞吐浪费）。
- **优化**：把两实序列 A、B packed 成 `C=A+iB`，一次 complex forward FFT 后由 **Hermitian 对称**提取 `A_dft`/`B_dft`，
  pointwise 仍复数乘（频域复数乘不可避免），**省一次 forward FFT**（现 15 forward / 9 inverse）。
- **正确性闸门**：26-case oracle（0 failures）+ callgrind I refs 不增。需重写 `mul_fft` forward/inverse/pointwise 接口。
- **GLM 待审**：① Hermitian 提取公式的逐元素等价推导；② `cmulv` pointwise 在 packed 下的改写；
  ③ `cyc_mul_fixed`/`fm_mul` 路径的对应改造；④ 浮点舍入是否仍在 oracle 容差内。

### 7.4 已排除的微红利（再确认）
twiddle **全部预计算**（`twbase[4096]` 64KB 表 + 启动期 `std::sin/cos`；运行时 `twg(i)` 查表 `cmul`，零即时 trig）
→ 无 twiddle 优化空间。AVX2 宽度已极致（§0）。SoA 已证伪（§4.5）。
