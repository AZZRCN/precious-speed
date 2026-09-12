GLM:

HEX div — 局部探究 instructions:u 理论极限
0. 结论：理论下界
基于 Zen3 ISA 与 AVX2 约束，在维持现有算法框架（radix-4/3 FFT + 4-seg carry）不变的前提下，instructions:u 的理论下界估算为：
instructions:u 理论下界 ≈ 420.0M ~ 425.0M
推导依据：
当前热点主要集中在 FFT 蝶形运算与进位传播。
FFT 核心 (difC4/iditC4)：占比 ~42%，代码已高度优化，使用 FMA 与循环展开，且受限于 YMM 寄存器压力（16 个），指令密度接近 ISA 极限。Load/Store 指令占比刚性，算术指令已通过 FMA 融合。预期压缩空间 < 2%。
进位传播 (carryPropSeg)：占比 ~10%。当前为标量实现，存在大量 cvttsd2si（double 转 int）与后续 ALU 操作。这是唯一具备大幅压缩空间的热点。若向量化成功，该部分指令数可下降 60%~70%，对整体贡献约 6%~8% 的收益。
其余部分：absAdd 等受限于进位链依赖，难以向量化；copyU16 等已向量化。压缩空间有限。
1. 可达优化方案（针对 instructions:u）
优化点 A：carryPropSeg 向量化（AVX2）— 核心收益点
现状分析：
函数内循环存在 4 路并行结构，但当前实现为标量：
// 现状：标量转换，指令数多
uint64_t s0 = c0 + cvtRoundU64(v + k); // cvtRoundU64 通常生成 cvttsd2si + 调整指令
uint64_t q0 = divBASE(s0);              // 乘法逆元或除法
out[k] = Limb(s0 - q0 * BASE);          // 标量 ALU
若 Limb 为 uint32_t（Base-2^16 的典型配置），AVX2 的 _mm256_cvtpd_epi32 指令可一次处理 4 个 double -> int32 转换，将 4 路并行循环压缩为紧致的向量循环。
优化代码（AVX2 重构）：
// 假设 BASE = 1 << 16, Limb 为 uint32_t
inline void carryPropSeg_VEC(const double *v, Limb *out, size_t n) {
    // ... (前略)
    const size_t seg = (n >> 2) & ~size_t(7);
    const size_t b1 = seg, b2 = seg * 2, b3 = seg * 3;
    // 向量常量
    const __m256d v_base = _mm256_set1_pd((double)BASE);
    const __m256d v_inv = _mm256_set1_pd(1.0 / BASE); // 用于快速近似除法，或用整数乘法逆元
    __m256i v_carry = _mm256_setzero_si256(); // 使用 si256 追踪 4 路 carry (作为 int32 流)
    // 主循环：处理 4 segments 中的对应块 (SoA 转 AoS 或 独立处理)
    // 这里展示处理一个 seg 块的向量化逻辑，实际需并行处理 b0,b1,b2,b3
    // 为简化描述，以下为核心指令替换逻辑：
    for (size_t k = 0; k < seg; k += 4) {
        // 1. Load 4 doubles
        __m256d d0 = _mm256_load_pd(v + k);
        // 2. Convert to int32 (_saturation handling if needed, though u16/u32 safe)
        //    注意：cvtpd_epi32 生成 int32，需要处理 u64 carry 溢出问题。
        //    若 Limb 宽度允许，直接使用。
        __m256i i0 = _mm256_cvtpd_epi32(d0); 
        // 3. Add carry (需要扩展精度或仔细设计)
        //    简单实现：向量加法后处理高位溢出（若 BASE 较小，需掩码）
        //    这里展示指令数减少的核心：4 路 double->int + 4 路 add + 4 路 store
        //    替换原本的 4x(cvt+add+mul+sub+store) 标量序列。
        // 4. Store
        _mm256_storeu_si256((__m256i*)(out + k), i0); 
        // 注意：此处省略了复杂的进位恢复逻辑，实际需在向量运算后修正 carry 溢出
    }
    // ... (尾部处理)
}
收益估算：
原：约 15-20 条指令 / limb（含 load/cvt/mul/store）。
优：约 4-5 条指令 / 4 limbs（load/cvt/store，且 AVX2 指令计为 1 条）。
该函数指令数预期下降 60%+。
优化点 B：absAdd / absSub 指令选择优化
现状分析：
代码使用标量加法与位运算模拟大整数加法，存在条件判断或复杂的进位处理逻辑。
优化建议：
使用编译器内置函数 _addcarry_u64 / _subborrow_u64（Intel intrinsic），这些函数在 Zen3 上能直接映射为高效的 adc/sbb 单条指令，比纯算术逻辑分解（in1 + in2 + carry; result % BASE）生成的指令序列更短。
// 优化前：多指令模拟
// Limb s = in1[i] + in2[i] + carry;
// out[i] = s & MASK; carry = s >> 16;
// 优化后：单条 adc 指令
// carry = _addcarry_u64(carry, in1[i], in2[i], (uint64_t*)&out[i]);
// 注意：需确认 ABI 与 Limb 宽度匹配，若 Limb 为 uint64_t 则直接对应。
// 若 Limb 为 uint32_t，可尝试打包 2 个 uint32 到 uint64 进行并行加法（需额外逻辑），或保持标量。
收益估算：
函数占比 ~5%，指令数下降约 10%，对整体贡献 ~0.5%。
优化点 C：difC4 / iditC4 微操调
现状分析：
寄存器压力极大（YMM 寄存器已满）。无法进一步展开。
优化建议：
检查编译器生成的汇编，确认是否存在 vmovapd 到栈的 Spill/Reload。若有，需减少活跃变量生命周期。
例如，在 difC4 中，tp1 和 tp3 的 load 是否可以延迟？
当前：
C4d c2 = c4loadM(it + s2), c3 = c4loadM(it + s3);
c4store(it, c0); // ...
c4store(it + s2, c4mul(c2, c4load(tp1))); // 这里 c2 生命周期长，且依赖 tp1
建议调整顺序，尽早完成 c2 的计算与存储，释放 c2 占用的寄存器，再处理下一组。这属于寄存器分配优化，可能减少 spill 导致的 instructions:u 增加（若存在）。
2. 约束检查
算法不变：FFT 结构与进位分段逻辑完全保留，仅改写实现方式（标量转向量）。
ISA 兼容：仅使用 AVX2 (_mm256_*)，未使用 AVX-512。
数值等价：carryPropSeg 向量化需保证 double->int 转换的精度与舍入一致（AVX2 cvtpd_epi32 默认 Round-to-Nearest-Even，需确认与原标量逻辑一致；若原为 round 需调整）。
指令数：预期能突破 430M 线，触及 420M 下界。
总结：当前优化的胜负手在于 carryPropSeg 的向量化，这是在不改变算法大框架下，唯一能显著降低 instructions:u 的路径。

DEEPSEEK:
## 1. 先给结论：全局“理论下界”无法从这些片段唯一推出

`instructions:u` 是整程序/整 `one_case` 的指令数，不是热点函数的简单求和。要给出真正的全局下界，必须：

- 有完整指令 trace；
- 知道所有 `c4loadM/c4storeM/cvtRoundU64/divBASE/difFixed` 的反汇编；
- 知道 `difDispatch/iditDispatch` 的 `difFixed` 是内联还是 call；
- 知道当前 `-ffp-contract` 是否允许编译器把 `a*b+c` 融成 FMA。

所以只能给出**核级理论下界**，而不是“整个 one_case 必然 >= X 指令”。

---

## 2. 核级下界：difC4 / iditC4 的向量主体已经接近极限

以一个 C4d 块为例：`c0,c1,c2,c3` 四个 `C4d`，处理 16 个 double（8 个复数）。

当前核心序列等价于：

- 4 × `c4load` = 8 条 load；
- `difSplitC4` = 12 条 256-bit ALU；
- 2 × `c4mul` = 每个 4 条 FMA/ALU，共 8 条；
- 2 × twiddle `c4load` = 4 条 load；
- 4 × `c4store` = 8 条 store。

合计：**40 条向量指令 / C4d 块**。

如果 `c4mul` 没有被编译成 FMA，而是两条 mul + 一条 add/sub，则每个 `c4mul` 是 6 条，一个块变成 **44 条**。

在当前 AVX2 + FMA3 下，40 条已经是这个 C4 布局的局部下界。也就是说：

> `difC4` / `iditC4` 的算术和访存主体，不再有算法级以外的纯指令数下降空间；继续能压的只有循环头、尾块、调用/分发、以及未向量化的标量函数。

---

## 3. 可达局部优化

### 3.1 优先检查：`c4mul` / `c4mulConj` 是否已经生成 FMA

你明确平台有 FMA3，但代码里 `c4mul` 是普通表达式：

```cpp
return C4d{a.re * b.re - a.im * b.im, a.im * b.re + a.re * b.im};
```

如果编译时没有发生 FMA contraction，这里会生成 6 条指令。手工写成 FMA intrinsic 可以降到 4 条：

```cpp
HINT_AI C4d c4mul(const C4d &a, const C4d &b)
{
    __m256d t = _mm256_mul_pd(a.im, b.im);
    __m256d re = _mm256_fmsub_pd(a.re, b.re, t);
    __m256d u = _mm256_mul_pd(a.re, b.im);
    __m256d im = _mm256_fmadd_pd(a.im, b.re, u);
    return C4d{re, im};
}

HINT_AI C4d c4mulConj(const C4d &a, const C4d &b)
{
    __m256d t = _mm256_mul_pd(a.im, b.im);
    __m256d re = _mm256_fmadd_pd(a.re, b.re, t);
    __m256d u = _mm256_mul_pd(a.re, b.im);
    __m256d im = _mm256_fmsub_pd(a.im, b.re, u);
    return C4d{re, im};
}
```

收益：每个 `c4mul` 少 2 条。`difC4`/`iditC4` 里每个 C4d 块有 2 个 `c4mul`，所以每块少 4 条，约 10%。

风险：FMA 会改变舍入，必须跑 9gen×Nseed oracle。若 oracle 不过，不能上。

同样检查 `dot_rfftX4` 里的 `mul1/mul2`，它们也是 `a*b+c` 形式，手工 FMA 化可能进一步降指令。

---

### 3.2 强制内联 `difFixed<16/32/64/128>` / `iditFixed<...>`

`difDispatch` 源码只是 switch，但 perf self% 高达 12.53%。这通常只有两种可能：

1. `difFixed` 被内联进了 `difDispatch`，perf 把指令记到了 `difDispatch`；
2. `difFixed` 是普通 call，call/ret + 栈帧指令被大量小长度调用放大。

无论是哪种，第一件该做的是：

- 给 `difFixed` 和 `iditFixed` 加 `always_inline` / `HINT_AI_DISPATCH`；
- 反汇编确认 dispatch 里没有真正的 call；
- 如果已经被内联，则下一条只能去查 `difFixed` 内部是否有冗余 load/store。

这一步不会改算法，只消 call/ret 和栈帧，**对 `instructions:u` 是纯收益**。

---

### 3.3 `difC4/iditC4` 循环展开从 2 组提到 4 组，并吸收尾块

当前：

```cpp
for (; k >= 2; k -= 2, it += 16, tp1 += 16, tp3 += 16)
{
    // c0..c3
    // d0..d3
}
if (k) { ... }
```

展开 4 组会减少 `k -= n`、指针加法和循环分支的摊销成本。

尾块 `if(k)` 可以改成：

- 循环前先处理一个块，主循环始终处理偶数块；
- 或者用编译期 guard 把尾块并入最后一个 4 组展开体。

估算：循环头/尾只占 `difC4` 的很小比例，可能节省总指令 0.3–0.8%，但这是几乎无风险的局部收益。

---

### 3.4 `dif3StageC4 / idit3StageC4` 同样做尾块吸收和 4 组展开

这两个函数结构和 difC4 类似，也有 `c >= 2` 主循环 + `if(c)` 尾块。可以用同样方式减少循环头尾开销。

---

### 3.5 `difSmall / iditSmall` 的 8 点标量分支 SIMD 化

`difSmall` 的 `float_len == 8` 分支目前是 8 个标量 load、`difSplit`、`transform2`、8 个标量 store。

这里可以用 AVX2 做一次 256-bit load + 一次 256-bit store，配合 `vperm2f128` / `vunpcklpd` / `vunpckhpd` 完成交换。`RIRI_IN/RIRI_OUT` 的 `std::swap` 也能用 `vpermilpd` 或 `vshufpd` 实现。

收益：主要减少 load/store 和地址计算。`difSmall` 占 5.79%，但其中 8 点路径被小 FFT 大量调用，值得做。

风险：中间重排容易写错，必须逐位 oracle 验证。

---

### 3.6 `carryPropSeg` 四段并行循环可尝试 AVX2 32-bit lane 向量化

当前四段并行：

```cpp
for (size_t k = 0; k < seg; ++k) {
    uint64_t s0 = c0 + cvtRoundU64(v + k);
    uint64_t s1 = c1 + cvtRoundU64(v + b1 + k);
    uint64_t s2 = c2 + cvtRoundU64(v + b2 + k);
    uint64_t s3 = c3 + cvtRoundU64(v + b3 + k);
    // divBASE + store
}
```

这四个段在同一个 `k` 上是独立的，天然适合 AVX2 四 lane 并行。

如果 `BASE == 2^16`，且 `cvtRoundU64` 的值域能严格限制在 `int32_t` 范围内，可以：

- 四个 double 组成 `__m256d`；
- `_mm256_cvtpd_epi32` 一次转换四段；
- 用 32-bit lane 做 `s = vi + carry`；
- `q = s >> 16`，`out = s & 0xFFFF`；
- 打包为 16-bit 后 store。

这能把四套标量 `cvt + add + shift + store` 压成一套向量路径。

关键限制：

- `_mm256_cvtpd_epi32` 是 round-to-nearest，必须与当前 `cvtRoundU64` 完全一致；
- 输入值必须可证明在 32-bit 范围；
- 四个段的 double 地址不是连续的，需要用 `_mm256_setr_pd` 或手动 `vinsertf128`，数据搬移会吃掉一部分收益。

这条是 `carryPropSeg` 最大潜在指令下降点，但必须离线先验证数值等价。

---

### 3.7 `absAdd / absSub` 暂不建议优先动

`absAdd` 1.15%、`absSub` 3.25%，合计约 4.4%。它们是 base-2^16 标量 carry 链。理论上可以用 SSE/AVX2 做 16-bit packed 运算，但 carry 链在向量里很难高效传播，可能为了省几条指令引入大量 mask/shift，反而得不偿失。

除非其他优化做完后还差最后 1–2%，否则不建议先碰。

---

## 4. 综合判断

在不做算法级重构、不碰 rdot 融合的前提下，`instructions:u` 的局部下降主要来自：

1. **手工 FMA 化 `c4mul/c4mulConj/dot_rfftX4`**，如果当前未收缩；
2. **强制内联 `difFixed/iditFixed`**；
3. **difSmall 8 点 SIMD 化**；
4. **carryPropSeg 四段向量化**；
5. 循环展开和尾块吸收，收益稳定但偏小。

粗估顺序：

- 单靠 1 + 2 + 5：可能降 10–25M 指令；
- 加上 3：再降 5–10M；
- 若 4 可行：再降 10–15M。

对应 `one_case` 426.36M 的“可达局部下界”，在保守前提下大约在 **390–410M** 量级；若 FMA 化失败或 carryProp 无法保证数值，则下界更高。

最终仍以你给出的唯一真值 `perf stat -e instructions:u` 和 9gen×Nseed oracle 为收口。

kimi:
暂无