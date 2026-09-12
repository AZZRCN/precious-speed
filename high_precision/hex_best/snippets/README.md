# HEX Big-Int Division — 超优化候选 snippet 包

目标平台：AMD EPYC 7B13 (Zen3, LC 判题机)，1 核 / 1 GiB。经验换算：**~10M 指令 ≈ 1ms 墙钟**。
指令数在「同 x86-64 二进制」下 Intel/AMD 一致，故本机 Intel VM 的 callgrind Ir 对 AMD 方向有效；cycles 才需 LC 真值。

## 实测基准（callgrind --cache-sim，AMD 几何，重题 length_ratio_2.in）
- 提交 #393027（含 B=0 守卫，未删 memset）= **382.8M 指令**（≈38.3ms，与 LC 39ms 吻合）。
- 当前 div.cpp（已删 fm_prep/fm_mul 冗余 memset）= **381.4M 指令**（≈38.1ms）。
- 即：该 memset 删除**实测仅省 1.4M 指令（0.37%）**，并非先前估算的 25M。先前 25M 估算为误判，以本次实测为准。

## 当前热点（callgrind, 381.4M 总指令, length_ratio_2.in）
| 占比 | 组件 | 说明 |
|---|---|---|
| ~68.8% | FFT 蝶形（内联 +O3） | `difRec/ditRec/difFlat/ditFlat/pointwise` 四函数各 14–16%（102M+70M+51M+39M）。已 radix-4 递归 + 扁平叶 + AVX2/FMA。无安全常量优化空间（FMA 全局 / 环形半尺寸 FFT 会因精度触发余数错误，见 DEC 注释）。**这是唯一大头，但属算法核心（DEC 同为 #1 也用此结构）** |
| 6.2% | `memset`（23.6M） | fm_prep/fm_mul 的冗余 memset 已删；剩余来自 split_b2 尾部零填（L880，正确性关键：高 limb 须当 0 补零）、结果/进位缓冲清零。无可安全删除项 |
| 1.2% | `memcpy`（4.6M） | 块循环 `memcpy(Z+n,Z,n)` / `memcpy(Q+off,Qi,lim)`；候选 SWAR 复制 |
| <2% | parse / output / libc | 单次巨数解析+输出，内存带宽主导，<1ms |

## CACHE 层实测结论（--cache-sim=yes）
- **L3(末级) 缺失仅 57,607 次**：33MB 工作集（WORK[4000000] + FFT 双缓冲）完全装得进 32MB L3 → 页表/大页(hugify/THP) 诉求已满足，**L3/页级无优化空间**。
- **L1d 缺失 5.68M，100% 落在 FFT 四函数**：为 FFT stride 访问固有（首层跨度 = n/2）。属算法访问模式，非缓冲布局问题；减小 FFT_LEAF_LOG 的扫参已确认 LEAF=8 最优，无收益。
- twiddle 表 `twbase[1<<12]`（64KiB，注释误写 16KiB）= 两级查表，常驻 L1；`resize()` 有 `if(n==twN)return` 守卫，**跨块零冗余重建**。
- **结论：CACHE 不是瓶颈。剩余杠杆全在 FFT 的 68.8%（算法级），须 FFT 重写（如 4-step / cache-oblivious）才有实质收益，风险高，建议交外部 LLM 在 snippet 层迭代。**

## 已完成的优化（本轮「做B」）
- **删 fm_prep/fm_mul 冗余 memset**：`split_b2` 完整写入 `g[0..zlim)`（含尾部补零），前置 `memset(FFT缓冲)` 100% 冗余。实测省 1.4M（382.8→381.4）。
- **统一内存**：静态大缓冲 `WORK[4000000]` + hugify(MADV_HUGEPAGE)，单分配单释放，33MB 工作集免 TLB 抖动（callgrind 已证 L3 缺失极小）。
- **DEC 手法审计**：radix-3/5 档位、SSE2 解析、AVX2 split_b2、扁平叶 codelet、两级 twiddle 表均已继承；cyclic/全局 FMA 因精度 RE 证实不可用。

## 交给外部 LLM 超优化的 snippet（均自包含、可直接编译）
| 文件 | 组件 | 合同 | 优化方向 |
|---|---|---|---|
| `parse_limbs.cpp` | hex 串 → u64 limbs（SSE2） | `int parse_limbs(const char* s,int n,u64* out)`；`n` hex 字符(big-endian)→`out[]` base-2^64 limbs(little-endian)，返回 `(n+15)/16` | 升级 AVX2（32B/次）、`vpternlogd` 单指令 a2n、避免 `_mm_loadu` 越界 |
| `put_big.cpp` | u64 limbs → hex 串（SSE2） | `char* put_big(char* out,const u64* V,int n)`；返回终点指针，去前导零 | 升级 AVX2（32B/次）、查表法 nib→ascii |
| `split_b2.cpp` | limb → double 数字打包（AVX2 gather） | `size_t split_b2(const u64* src,double* g,size_t n,int k,size_t zlim)`；base-2^64 → base-2^k 数字，FFT 输入 | gather 替代、减少 `cvtepi32_pd`、k 选择 |
| `merge_b2.cpp` | double 数字 → limb 进位链（**标量**） | `void merge_b2(u64* f,const double* g,size_t n,int k)`；FFT 输出、进位传播 | **SWAR/AVX2 进位链**（参考 DEC D19 `absMul1` 的 `_pext`+SWAR 涟漪进位） |

## 参考（未抽，但也是候选）
- FFT 基例蝶形 `difFlat`/`ditFlat`（div.cpp ~427/461）：AVX2 `bfFwd/bf2Fwd` 已向量化；可试 radix-4 展开、4-step 重排降 L1 缺失。
- 块循环 `bz_divide`（div.cpp ~1459）：每项 `qhat*BS` 仅低 n limbs 进入下一轮；环形半尺寸 FFT 精度护栏仅允许 n≲150（大题不可用），DEC 已因 RE 关闭——高风险，勿轻易动。

## 验收标准
任何 snippet 改动必须：(1) callgrind 指令数下降；(2) `tools/verify_self.py` 全 26 组（过滤 B=0 后）`TOTAL_FAILS=0`。
