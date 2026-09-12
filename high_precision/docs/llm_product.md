KIMI K2.6 快速：

以下按**预期指令数收益**从高到低排列，给出可落地的重构方案。所有建议均针对 `instructions:u` 优化，同时兼顾你列出的 cache-miss 热点。

---

## P1. FFT 核循环展开 2×–4× + 条件化预取（difC4 / iditC4 / dif3StageC4）

**指令数占比**：`difC4` 24.78% + `iditC4` 15.34% + `dif3StageC4` 5.87% ≈ **46%**

### 问题
当前热循环每次只处理 **1 组 AVX2 寄存器**（`it += 8`，即 4 个复数对），循环控制指令（`k--`、`cmp`、`jcc`、4 组指针更新）的占比过高。  
另外，**软件预取 `HINT_PREFETCH` 在循环体内固定发射**，当递归进入小 stride（数据已在 L1）时，这两条预取是纯指令开销，对 cache 无帮助。

### 改动
1. **循环展开 2×**：把 `k--` 步进改为每次处理 2 组（`it += 16`），手动展开两套 `c4load` / `difSplitC4` / `c4store` / `c4mul`。  
   - 循环次数减半，控制指令（`dec`/`cmp`/`jne` + 指针更新）减少 ~50%。
   - 以 `difC4` 为例，顶层循环约 `float_len/32` 次，下层递归累积次数在百万级，可省 **数百万条** 控制指令。

2. **条件预取**：只在 `stride * sizeof(double) * 4 > L1_CACHE_LINE_SIZE * 4`（即当前层数据量大于若干 cache line）时发射软件预取。小 stride 时完全删除 `HINT_PREFETCH`。
   - 小 stride 的递归层数约占总层数一半，每迭代省 2 条预取指令。

### 预期收益
- 总指令数降低 **2%–4%**。
- 额外好处：减少前端 decode 压力，IPC 可能微升（但你只关心指令数）。

---

## P2. 融合 `copyU16ToF64AndFill` 与 FFT 首级（消除一次 O(N) 遍历）

**指令数占比**：`fftMulPre` 及其子调用 7.68%，其中隐藏了一次完整的内存遍历。

### 问题
`fftMulPre` 的流水线：
```cpp
copyU16ToF64AndFill(a.ptr, v, a_len, float_len);  // Pass 1: limb → double, store to v
transform::fft::rdif(v, float_len);                   // Pass 2: load from v, FFT
```
`rdif` 的底层最终会调用 `difC4<IN_MODE=true>`，其 `c4loadM<true>` 的作用就是把实数数组加载并打包成 C4 复数格式。这意味着 **Pass 1 的 store 和 Pass 2 的 load 是可消除的冗余内存操作**。

### 改动
不要先完整展开到 `v`。改为让 `rdif`（或 `difC4` 的最底层 base case）**直接从 `a.ptr`（uint16 limb 数组）读取并转换为 double**，同时完成 real-pack（C4 格式写入）。

具体做法：
- 在 `difC4` 的 `float_len <= FFT_C4_MIN` 标量回落路径中，或新增一个 `difC4<IN_MODE=LIMB>` 特化版本，让 `c4loadM` 从 `uint16_t*` 读取，内部用 `_mm256_cvtepu16_epi32` + `_mm256_cvtepi32_pd` 展开为 double 并打包。
- 这样 `copyU16ToF64AndFill` 这个独立函数在 `fftMulPre` 中可完全删除，**省掉一次 O(N) 的 load-store 对和循环控制指令**。

### 预期收益
- 每次卷积省 **~1M–2M 条** 指令（取决于块大小）。
- 若 `absDivMu` 块循环调用 `fftMulPre` 数十次，累计可降低总指令数 **1%–2%**。

---

## P3. carryPropSeg：divBASE 指令审计 + 边界修正循环压缩

**指令数占比**：8.92%，且 cache-miss 最高。

### 问题 A：`divBASE` 的指令序列
你提到 BASE = 65516。先确认一个事实：**16⁴ = 65536 = 2¹⁶**，而不是 65516。  
- 如果 BASE 实际上是 **65536**（2 的幂），`divBASE(s)` 和 `s - q * BASE` 可以分别用 `s >> 16` 和 `s & 0xFFFF` 替代，**从 3–4 条指令（mul+shr+sub）压缩到 1–2 条指令（shr/and）**。这对 `carryPropSeg` 是颠覆性优化。
- 如果 BASE 确实是 65516（非 2 的幂），检查编译器是否将其内联为 `mul+shr+sub` 序列。由于你使用 `-O2` 且 BASE 是编译期常量，通常如此。但如果是外部函数调用，指令数会爆炸。

### 问题 B：边界修正循环的冗余分支
当前 3 段边界修正：
```cpp
for (size_t p = b1; ov > 0 && p < b2; ++p) { ... }
```
`ov > 0` 在大多数段边界上为 false（进位跨段传播概率低），但**每次调用仍要执行比较+跳转指令**。更关键的是，3 个独立循环各自有 prologue/epilogue。

### 改动
1. **BASE 确认**：如果可能，将 BASE 改为 65536（如果你确实只需要编码 4 位十六进制，65536 完全兼容）。这是指令数收益最大的单点改动。
2. **边界修正合并**：将 3 个修正循环合并为 1 个顺序传播循环，从段 0 的末尾开始，用 `cmov` 或无条件顺序执行（因为 `ov` 通常很小，循环次数极少）：
   ```cpp
   uint64_t ov = c0;
   for (size_t p = b1; p < n && ov; ++p) {
       uint64_t s = uint64_t(out[p]) + ov;
       ov = divBASE(s);          // 若 BASE=65536: ov = s >> 16
       out[p] = Limb(s - (ov << 16)); // 或 s & 0xFFFF
   }
   // 同理传播 c1, c2，但复用同一循环体，避免 3 份循环控制指令
   ```
3. **预取**：`carryPropSeg` 顺序扫描 `v` 和 `out`，硬件预取器已能完美覆盖。**删除所有软件预取**（如果有的话），省指令。

### 预期收益
- 若 BASE 改为 65536：**3%–5%** 总指令数下降（`carryPropSeg` 本身指令减半）。
- 若保持 65516，仅合并边界循环：约 **0.3%–0.5%**。

---

## P4. 逆变换共轭 twiddle 预存（消除 c4mulConj）

**指令数占比**：`iditC4` 15.34%

### 问题
`iditC4` 循环内使用 `c4mulConj`（乘共轭）：
```cpp
C4d c2 = c4mulConj(c4load(it + s2), c4load(tp1));
```
在 AVX2 上，共轭乘需要额外 1 条指令对虚部取反（`vsubpd` 或 `vxorpd` sign-bit mask）。

### 改动
预计算两张共轭表 `table1_conj` / `table3_conj`（在 `table1`/`table3` 初始化时一并算出）。逆变换中改用普通 `c4mul`：
```cpp
// 原：
c4mulConj(x, c4load(tp1))
// 新：
c4mul(x, c4load(tp1_conj))
```
每蝶形点省 **1 条 AVX2 指令**。

### 预期收益
- 总指令数降低 **0.5%–1%**。
- 代码变更极小，风险极低。

---

## P5. real_dot_binrev3：常量外提 + 分支消除

**指令数占比**：7.20%

### 问题
1. `invv = _mm256_set1_pd(...)` 和 `rotv` 在 `begin` 循环内部重复生成。
2. `if (begin >= 32)` 运行时分支在循环内部，阻碍编译器展开。

### 改动
1. 将 `invv`、`rotv`、`invx` 全部移到最外层。
2. 把 `real_dot_binrev3` 拆成两个版本（或模板特化）：
   - `real_dot_binrev3_small`：处理 `begin < 32` 的固定短序列（完全展开）。
   - `real_dot_binrev3_large`：处理 `begin >= 32`，无内部 `if`。

### 预期收益
- **0.3%–0.6%** 总指令数。

---

## P6. fftMulPre 输出零填充消除

### 问题
```cpp
if (out.size > conv_len + 1)
    std::fill_n(out.ptr + conv_len + 1, out.size - conv_len - 1, Limb(0));
```
在块循环中，`out` 缓冲区如果由调用方 `absDivMu` 预先清零（或复用同一块已清零内存），这条 `fill_n` 完全是冗余指令。

### 改动
在 `absDivMu` 层保证 `out` 缓冲区在进入块循环前已 `memset(0)`，然后删除 `fftMulPre` 中的 `fill_n`。

### 预期收益
- 取决于 `out.size - conv_len` 的大小，通常可降低 **0.1%–0.3%**。

---

## P7. （可选）增大 FFT base case，减少递归层数

当前 `float_len <= FFT_C4_MIN` 时回落到标量 `dif` + `packC4`。如果 `FFT_C4_MIN` 较小（如 64 或 128），递归层数较多，每次递归有 `call`/`ret` 和栈帧指令。

### 改动
将 `FFT_C4_MIN` 增大到 **512 或 1024**，并写一个 **手动 AVX2 展开的 base case**（处理 512/1024 点的纯汇编式蝶形，无递归）。  
这样减少约 2–3 层递归调用，省掉每层 3 路调用的 `call`/`ret` 指令。

### 预期收益
- 总指令数降低 **0.2%–0.5%**（递归开销本身占比不高，但 base case 增大后标量回落路径更短）。

---

## 验证与测量建议

1. **先验证 BASE**：检查源码中 `BASE` 的定义。如果是 `65516` 但你的实际需求是 4 位十六进制（16⁴），应改为 `65536u`。这是指令数收益最高的单点。
2. **divBASE 汇编检查**：
   ```bash
   objdump -d your_binary | grep -A5 -B5 divBASE
   ```
   确认生成的是 `mul`+`shr`（或 `shr`+`and`）而非 `call` 或 `div r64`。
3. **逐步回归**：每次只应用一项优化，跑 `perf stat -e instructions:u -x; ./test_oracle`，确保指令数下降且 9 组生成器 × 8 种子通过。
4. **循环展开度量**：展开 2× 通常是最优 sweet point（指令数收益高、代码膨胀可控）。4× 可能因代码体积导致 icache miss，反而增加取指指令。

---

## 总结：预期总收益

| 优化项 | 预期指令数降幅 | 风险 |
|--------|-------------|------|
| P1. FFT 循环展开 + 条件预取 | **2%–4%** | 低 |
| P2. 融合 copy 与 FFT 首级 | **1%–2%** | 中（需改 rdif 入口） |
| P3. carryPropSeg（若 BASE→65536） | **3%–5%** | 低（需确认语义） |
| P3. carryPropSeg（仅边界压缩） | 0.3%–0.5% | 低 |
| P4. 预存共轭 twiddle | **0.5%–1%** | 极低 |
| P5. real_dot_binrev3 常量外提 | 0.3%–0.6% | 低 |
| P6. 消除 fill_n | 0.1%–0.3% | 低 |
| **合计（保守）** | **4%–8%** | — |

如果 BASE 确实是 65536 且你尚未利用其 2 的幂特性，**仅 P3 一项即可打平或超越其他所有局部微调**。建议优先验证这一点。



DEEPSEEK_PART1：

针对当前 FFT 除法热点，以 **降低 x86-64 指令条数** 为唯一目标（Zen3 / AVX2 / `-O2`），以下优化方案按预期收益排序，可直接落地。所有改动不影响正确性，需用既有 oracle 回归验证。

---

## 1. 消除所有预取指令（收益最高，零风险）

**现状**：`difC4`、`iditC4`、`dif3StageC4` 中每条循环都执行 `HINT_PREFETCH`（2 条/迭代）。这些指令本身计入指令数，但**不影响** `perf instructions:u` 下的性能评分。由于 twiddle 表通常较小且顺序访问，L1 命中率已很高，预取收益微乎其微。

**修改**：直接删除所有 `HINT_PREFETCH(...)` 行（或注释掉）。

**预期收益**：以 `difC4` 24.78% 占比计算，循环体内预取约占 2/10 条指令，移除后可降低该函数指令数 **约 20%**，整体减少约 5% 总指令数。

---

## 2. 加倍循环展开，减少循环控制开销

### `difC4` 与 `iditC4`
当前每次迭代处理 8 个 double（4 个复数），循环次数较多。改为每次处理 16 个 double（8 个复数），将循环体复制一份，减少地址增量、比较、跳转指令。

**示例（difC4）**：
```cpp
for (size_t k = fft_len / 32; k > 0; k--, it += 16, tp1 += 16, tp3 += 16)
{
    // 第一组
    C4d c0 = c4loadM<IN_MODE>(it), c1 = c4loadM<IN_MODE>(it + s1);
    C4d c2 = c4loadM<IN_MODE>(it + s2), c3 = c4loadM<IN_MODE>(it + s3);
    difSplitC4(c0, c1, c2, c3);
    c4store(it, c0);
    c4store(it + s1, c1);
    c4store(it + s2, c4mul(c2, c4load(tp1)));
    c4store(it + s3, c4mul(c3, c4load(tp3)));

    // 第二组（偏移 +8）
    C4d d0 = c4loadM<IN_MODE>(it + 8), d1 = c4loadM<IN_MODE>(it + s1 + 8);
    C4d d2 = c4loadM<IN_MODE>(it + s2 + 8), d3 = c4loadM<IN_MODE>(it + s3 + 8);
    difSplitC4(d0, d1, d2, d3);
    c4store(it + 8, d0);
    c4store(it + s1 + 8, d1);
    c4store(it + s2 + 8, c4mul(d2, c4load(tp1 + 8)));
    c4store(it + s3 + 8, c4mul(d3, c4load(tp3 + 8)));
}
```
注意调整循环次数为 `fft_len / 32`。对于奇数情况，增加尾部处理循环。

**预期收益**：循环控制指令减少约一半，`difC4`、`iditC4` 指令数降低 **约 10%~15%**。

---

## 3. 调整递归基例阈值，减少函数调用

**现状**：`difC4` 与 `iditC4` 在 `float_len <= FFT_C4_MIN` 时调用 `packC4` + `dif<false>`。递归深度大，每次递归产生 `call`/`ret`、栈帧操作等指令。

**修改**：将 `FFT_C4_MIN` 提高（例如从当前值 16 提升到 32 或 64），使更多小规模 FFT 直接走标量 base case，避免深层递归。**但需实测**：标量 base case 可能比向量化多指令，需找到平衡点。建议在典型大数长度下用 `perf stat` 对比不同阈值。

**预期收益**：递归深度减少，可降低总体指令数 **约 3%~5%**（取决于原阈值大小）。

---

## 4. 向量化 `copyU16ToF64AndFill`（`fftMulPre` 内部）

**现状**：`copyU16ToF64AndFill` 将 `uint16` 打包的 limb 数组展开并转换为 `double`，填充到 `v`。若为标量循环，每个元素需 `movzx`、`vcvtsi2sd` 等指令，占一定比例。

**修改**：使用 AVX2 指令一次处理 8 个 limb：
```cpp
#include <immintrin.h>
void copyU16ToF64AndFill(const Limb* src, double* dst, size_t count, size_t fill_len)
{
    size_t i = 0;
    for (; i + 8 <= count; i += 8)
    {
        __m128i x = _mm_loadu_si128((const __m128i*)(src + i)); // 8 个 uint16
        __m256i y = _mm256_cvtepu16_epi32(x);                  // 8 个 uint32
        __m256d lo = _mm256_cvtepi32_pd(_mm256_castsi256_si128(y)); // 低 4 个
        __m256d hi = _mm256_cvtepi32_pd(_mm256_extracti128_si256(y, 1)); // 高 4 个
        _mm256_storeu_pd(dst + i, lo);
        _mm256_storeu_pd(dst + i + 4, hi);
    }
    // 处理剩余元素
    for (; i < count; ++i)
        dst[i] = (double)src[i];
    // 填充 0
    for (; i < fill_len; ++i)
        dst[i] = 0.0;
}
```
注意 `Limb` 是 `uint32_t`，但描述中提到“1 limb = uint32_t，BASE = 65516，每 limb 编码 4 位十六进制”，实际 limb 可能仅用 16 位？需确认 `copyU16ToF64AndFill` 的输入类型。若 limb 是 `uint32_t` 且值 < 65536，可用 `_mm256_cvtepu32_pd` 直接转换（AVX2 支持 `_mm256_cvtepu32_pd`？实际上只有 `_mm256_cvtepi32_pd`，需要先转为 int32，若值小于 2^31 可直接当有符号处理）。

**预期收益**：`fftMulPre` 中该函数占比虽未单列，但若为标量，向量化可减少大量转换指令，整体指令数降低 **约 2%~4%**。

---

## 5. 优化 `carryPropSeg` 的标量循环

### 增加并行路数
当前 4 路并行，可改为 **8 路并行**：
```cpp
const size_t seg = (n >> 3) & ~size_t(7);
const size_t b1 = seg, b2 = seg*2, ..., b7 = seg*7;
uint64_t c[8] = {0};
for (size_t k = 0; k < seg; ++k) {
    for (int j = 0; j < 8; ++j)
        c[j] += cvtRoundU64(v + bj + k);
    for (int j = 0; j < 8; ++j) {
        uint64_t q = divBASE(c[j]);
        out[bj + k] = Limb(c[j] - q * BASE);
        c[j] = q;
    }
}
```
减少循环迭代次数和地址计算指令，但需增加修正循环数量（8 路时需 7 个涟漪修正），通常修正迭代极少，净收益为正。

### 使用 `_mm256_round_pd` 辅助舍入？
`cvtRoundU64` 若为 `_mm256_cvttpd_epi64` 不可用，但可通过“双转换”技巧：先将 double 舍入到 int32，再组合成 int64？过于复杂，不建议。

**预期收益**：`carryPropSeg` 指令数降低 **约 10%~15%**（因循环开销占比下降）。

---

## 6. 调整 FFT 数据布局（中期重构）

**现状**：C4 格式为复数交错存储（AoS），导致 `c4loadM`/`c4store` 可能需要额外的 `vpermpd`/`vshufpd` 来重排实虚部。

**建议**：将复数数组改为 **SoA**（分离的实部数组与虚部数组），FFT 蝶形直接对 `__m256d` 实部、虚部操作，消除 shuffle。这需要修改所有 FFT 核心函数及点乘、carry 还原等，工作量大但收益高。

**预期收益**：FFT 核心指令数可降低 **15%~25%**，尤其对 `difC4`、`iditC4`、`real_dot_binrev3` 影响显著。

---

## 7. 合并 `real_dot_binrev3` 中的循环块

**现状**：块 0 的循环与块 1/2 的循环结构类似，但分别处理，导致重复的循环控制代码。

**修改**：尝试将三块的处理合并到一个统一的循环中，通过条件判断选择不同的 twiddle 常数，减少循环次数和重复的地址计算。例如，将块 1/2 的镜像处理集成到主循环内。

**预期收益**：减少冗余循环，指令数降低 **约 1%~2%**。

---

## 8. 启用编译器的积极优化（不改变命令，但调整源码）

- 确保关键函数使用 `inline` 或 `__attribute__((always_inline))`，避免函数调用。
- 使用 `__restrict__` 修饰指针，消除别名检查，让编译器生成更紧凑的代码。
- 将常量 `BASE`、`inv` 等定义为 `constexpr` 或 `static const`，方便立即数嵌入。

**示例**：在 `carryPropSeg` 中，将 `BASE` 作为编译期常量，`divBASE` 使用模板特化，确保生成 `mulhi`+`shr` 而非 `div` 指令。

---

## 总结优先级

| 优化项 | 预期指令数降幅 | 风险 | 实施难度 |
|--------|----------------|------|----------|
| 移除预取 | ~5% | 极低 | 极低 |
| 循环展开 ×2 | ~5% | 低 | 低 |
| 调整 FFT_C4_MIN | ~3% | 中（需实测） | 低 |
| 向量化 copyU16 | ~3% | 低 | 中 |
| carryPropSeg 8 路 | ~2% | 低 | 低 |
| SoA 数据布局 | ~15% | 高 | 高 |
| 合并 real_dot 循环 | ~2% | 低 | 中 |

**建议执行顺序**：先实施 1、2、5 项，快速降低指令数；再根据测试结果调整 3 和 4；最后考虑 SoA 重构。所有改动必须通过既有 9×8 组 oracle 测试验证正确性，并确认 `perf stat -e instructions:u` 不高于基线。

DEEPSEEK——Part2：

除了前面提到的循环展开、预取删除、carryPropSeg 并行路数等，还有几个**更底层、收益更高**的方向，尤其适合当前 AVX2 无 AVX-512、只看 `instructions:u` 的约束。

---

## 1. 用 FMA 重写所有复数乘法和蝶形里的乘加/乘减

这是目前最值得做、也最容易被忽略的点。  
当前 `c4mul` / `c4mulConj` / `c4twiddle` / `difSplitC4` / `iditSplitC4` / `dif3StageC4` 里大概率有类似：

```cpp
re = a.re * b.re - a.im * b.im;
im = a.re * b.im + a.im * b.re;
```

在 `-O2` 且没有 `-ffast-math` 时，编译器**不会**把它们自动变成 FMA。每这样一个复数乘法会生成 4 条乘法 + 2 条加/减，共 **6 条指令**。  
Zen3 有 2 条 FMA/cycle，AVX2 FMA 一次处理 4 个 `double`，而且 FMA 在 `instructions:u` 里只算 1 条。

### 示例改写

#### `c4mul`
```cpp
inline C4d c4mul(C4d a, C4d b) {
    __m256d re = _mm256_fmsub_pd(a.re, b.re,
                 _mm256_mul_pd(a.im, b.im));   // a.re*b.re - a.im*b.im
    __m256d im = _mm256_fmadd_pd(a.re, b.im,
                 _mm256_mul_pd(a.im, b.re));   // a.re*b.im + a.im*b.re
    return {re, im};
}
```

从 6 条指令降到 4 条。

#### `c4mulConj`
```cpp
inline C4d c4mulConj(C4d a, C4d b) {   // a * conj(b)
    __m256d re = _mm256_fmadd_pd(a.im, b.im,
                 _mm256_mul_pd(a.re, b.re));   // a.re*b.re + a.im*b.im
    __m256d im = _mm256_fmsub_pd(a.im, b.re,
                 _mm256_mul_pd(a.re, b.im));   // a.im*b.re - a.re*b.im
    return {re, im};
}
```

同样从 6 条降到 4 条。

#### `dif3StageC4` 中的常量乘加
原代码：

```cpp
const __m256d hr = a0.re - sr * vh;
const __m256d hi = a0.im - si * vh;
const __m256d gr = di * vns;
const __m256d gi = dr * vs;
```

可改成：

```cpp
const __m256d hr = _mm256_fnmadd_pd(sr, vh, a0.re);  // a0.re - sr*vh
const __m256d hi = _mm256_fnmadd_pd(si, vh, a0.im);
const __m256d gr = _mm256_mul_pd(di, vns);
const __m256d gi = _mm256_mul_pd(dr, vs);
```

`hr`、`hi` 从“1 乘法 + 1 减法”变成 1 条 FMA。每轮 `dif3StageC4` 蝴蝶至少省 2 条，配合 `c4mul` 的 FMA 改写，单函数指令数下降会非常明显。

同样的思路扫描 `difSplitC4` / `iditSplitC4`：  
凡是 `x - y * z` 用 `_mm256_fnmadd_pd(y,z,x)`；  
凡是 `x + y * z` 用 `_mm256_fmadd_pd(y,z,x)`。  
注意要手动使用 intrinsic，不能依赖编译器在 `-O2` 下自动生成。

**预期收益**：`difC4`、`iditC4`、`dif3StageC4` 的指令数可下降约 8%～15%，是纯指令数优化中收益最大的一项。

---

## 2. 把 `rdot` 点乘融合进逆 FFT 的基例，删除整个独立点乘 pass

当前 `fftMulPre` 路径是：

```cpp
rdif(v, float_len);       // 正向 FFT
rdot(v, b_dft, float_len); // 逐点乘
ridit(v, float_len);      // 逆 FFT
```

其中 `rdot` 是一轮完整遍历：

- load `v[i]`
- load `b_dft[i]`
- 复数乘法
- store `v[i]`

紧接着 `ridit` 的第一层递归/基例会再次 load `v`。  
也就是说 `v` 被多读一遍、多写一遍。

更优方案：**写一个 `riditMul(v, b_dft, float_len)`**，把点乘下沉到逆 FFT 的叶子基例。

### 实现思路

`iditC4` 的递归形状是：

```cpp
iditC4<OUT_MODE>(inout, stride * 2);
iditC4<OUT_MODE>(inout + stride * 2, stride);
iditC4<OUT_MODE>(inout + stride * 3, stride);
// 再做本层 butterfly
```

新增版本：

```cpp
void iditC4Mul(Float inout[], const double *b_dft, size_t float_len)
{
    if (float_len <= FFT_C4_MIN) {
        // 这里 inout 仍处于 bit-reversed 频域顺序
        for (size_t i = 0; i < float_len; ++i)
            inout[i] = c4mul(inout[i], b_dft[i]);
        idit<false>(inout, float_len);
        packC4(inout, float_len);
        return;
    }
    const size_t stride = float_len / 4;
    iditC4Mul(inout, b_dft, stride * 2);
    iditC4Mul(inout + stride * 2, b_dft + stride * 2, stride);
    iditC4Mul(inout + stride * 3, b_dft + stride * 3, stride);
    // 之后复制原 iditC4 的 butterfly 循环
}
```

然后在 `fftMulPre` 中：

```cpp
rdif(v, float_len);
riditMul(v, b_dft, float_len);   // 不再调用 rdot
uint64_t carry = carryPropSeg(v, out.ptr, conv_len);
```

### 为什么正确

`rdif` 输出和 `b_dft` 的存储顺序一致，都是 bit-reversed 频域顺序。  
`iditC4` 的基例在递归到底时，处理的正是该顺序下的连续子段；因此可以在基例处直接逐点乘 `b_dft`，再进入逆变换。  
`b_dft` 的偏移只需跟随 `inout` 递归时的偏移即可。

如果逆变换路径中包含 `real_dot_binrev3`，也需要在它的叶子处乘对应 `b_dft`，思路相同。

**预期收益**：消除 `rdot` 的整轮 load/store 和循环控制。  
大 case 下 `float_len` 约几十万～上百万，可减少约 2%～4% 总指令数。

---

## 3. 向量化并手写小长度 FFT 基例

现在 `float_len <= FFT_C4_MIN` 时会调用：

```cpp
packC4(inout, float_len);
dif<false>(inout, float_len);
```

以及逆变换的：

```cpp
idit<false>(inout, float_len);
packC4(inout, float_len);
```

如果 `FFT_C4_MIN` 取值较小，基例会走标量路径。  
标量路径在小长度上即使长度短，仍可能产生不少 `call`、`movsd`、`mulsd`、`addsd` 等指令。

建议为长度 4、8、16、32 写专用的 AVX2 蝶形，直接操作 `__m256d`，并消除 `packC4` 的额外重排。  
例如长度 16 的实输入 FFT 可以完全用若干个 `_mm256_add_pd` / `_mm256_sub_pd` / `_mm256_mul_pd` 完成，不需要任何循环。

小长度基例在递归中被调用很多次，累计指令数不可忽视。

**预期收益**：基例及调用开销可下降 3%～6%，尤其是 FFT 长度含多个小因子时。

---

## 4. 优化 `copyU16ToF64AndFill`：AVX2 展开 + 清零用 `memset`

当前 `copyU16ToF64AndFill` 会把 limb 展开成 `double`，并把剩余部分清零。  
如果它目前是标量循环，可以改成一次处理 8 个 limb：

```cpp
#include <immintrin.h>

void copyU16ToF64AndFill(const Limb *src, double *dst,
                         size_t count, size_t fill_len)
{
    size_t i = 0;
    for (; i + 8 <= count; i += 8) {
        // Limb 值 < 65536，按 uint16 处理
        __m128i x = _mm_loadu_si128((const __m128i *)(src + i));
        __m256i y = _mm256_cvtepu16_epi32(x);          // 8 个 uint32
        __m128i lo = _mm256_castsi256_si128(y);
        __m128i hi = _mm256_extracti128_si256(y, 1);
        __m256d dlo = _mm256_cvtepi32_pd(lo);
        __m256d dhi = _mm256_cvtepi32_pd(hi);
        _mm256_storeu_pd(dst + i, dlo);
        _mm256_storeu_pd(dst + i + 4, dhi);
    }
    for (; i < count; ++i)
        dst[i] = (double)src[i];

    // 清零剩余部分直接用 memset，而不是 std::fill 循环
    if (fill_len > count)
        std::memset(dst + count, 0, (fill_len - count) * sizeof(double));
}
```

注意 `dst` 必须保证 32 字节对齐时可用 `_mm256_store_pd`；否则用 `storeu`。  
清零部分用 `memset` 通常编译为 `rep stos`，在指令数统计中远优于逐元素循环。

**预期收益**：减少 limb→double 转换及清零循环的指令数，整体约 1%～2%。

---

## 5. `carryPropSeg`：8 路并行 + 消除 `llround` 指令

当前 4 路并行，每轮循环仍有一定循环控制开销。  
可改成 8 路：

```cpp
const size_t seg = (n >> 3) & ~size_t(7);
const size_t b[7] = {seg, 2*seg, 3*seg, 4*seg, 5*seg, 6*seg, 7*seg};
uint64_t c[8] = {0};

for (size_t k = 0; k < seg; ++k) {
    for (int j = 0; j < 8; ++j)
        c[j] += cvtRoundU64(v + b[j] + k);

    for (int j = 0; j < 8; ++j) {
        uint64_t q = divBASE(c[j]);
        out[b[j] + k] = Limb(c[j] - q * BASE);
        c[j] = q;
    }
}
```

循环迭代次数减半，循环控制指令下降。  
修正涟漪循环从 3 段变成 7 段，但每段通常极短，总指令增加很小。

另外，`cvtRoundU64` 若当前用的是 `std::llround` 或 `std::nearbyint`，其内部可能包含多次转换和舍入模式检查。  
在 Zen3 上可用 SSE4.1 的 `roundsd` 再 `cvttsd2si`：

```cpp
inline uint64_t cvtRoundU64(const double *v) {
    __m128d x = _mm_load_sd(v);
    __m128d r = _mm_round_sd(x, x,
                _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
    return static_cast<uint64_t>(_mm_cvttsd_si64(r));
}
```

如果当前已是该实现，则忽略此项。

**预期收益**：`carryPropSeg` 指令数可再下降 5%～10%。

---

## 6. 在热循环指针上显式加 `__restrict__`

当前代码已有 `__builtin_assume_aligned`，但没有明显的别名声明。  
在 `difC4`、`iditC4`、`dif3StageC4`、`carryPropSeg` 中，`inout`、`table1`、`table3`、`v`、`out` 等指针如果能加上 `__restrict__`，编译器可以省略部分重复加载和保守同步。

例如：

```cpp
void difC4(double *__restrict__ inout, size_t float_len)
```

以及内部：

```cpp
const double *__restrict__ tp1 = ...;
const double *__restrict__ tp3 = ...;
double *__restrict__ it = ...;
```

这通常不会大幅减少指令数，但能让 FMA 和循环展开后的代码更紧凑。

---

## 7. 可选：对 FFT 函数单独开启 fast-math 语义

编译命令是固定的 `-O2`，没有 `-ffast-math`。  
但可以在源码中对特定函数加：

```cpp
__attribute__((optimize("fast-math")))
void difC4(...) { ... }
```

或者在文件开头：

```cpp
#pragma STDC FP_CONTRACT ON
```

后者不一定被 GCC 完全遵守，前者可以强制该函数使用 fast-math 优化。  
这会让编译器自动完成乘加融合、省略 NaN/Inf 检查，从而减少指令数。  
**但必须通过 oracle 回归**，因为 fast-math 可能改变舍入和特殊值语义。  
如果不想依赖编译器行为，优先用第 1 条的显式 FMA intrinsic，更可控。

---

## 优先级建议

| 改动 | 预计指令数下降 | 风险 | 实施难度 |
|---|---:|---|---|
| FMA 重写所有复乘/蝶形 | 8%～15% | 低 | 中 |
| 融合 `rdot` 到逆 FFT 基例 | 2%～4% | 中 | 中高 |
| 手写小长度 AVX2 基例 | 3%～6% | 低 | 中 |
| `copyU16ToF64AndFill` 向量化 + memset | 1%～2% | 低 | 低 |
| `carryPropSeg` 8 路 + roundsd 转换 | 5%～10% | 低 | 低 |
| `__restrict__` 和 aligned 提示 | 1%左右 | 极低 | 低 |

建议先实施 **1、5、4**，这三项风险低且立刻见效；再实施 **2、3**。  
每完成一项都跑一次既有 9×8 oracle 回归，并确认 `perf stat -e instructions:u` 不高于基线。


豆包：

# HEX大整数除法FFT内核指令数优化方案

针对Zen3 AVX2平台、以**动态指令条数**为唯一优化目标，结合热点函数占比与代码结构，按优先级从高到低给出以下优化方案，所有方案均满足正确性约束。

---

## 一、零成本高收益优化（优先落地）

### 1. 逆FFT归一化合并舍入偏移（+0.5）

#### 优化原理

当前`carryPropSeg`中每个元素需执行`double→uint64`舍入转换，标量实现为`(uint64_t)(v[i] + 0.5)`，每个元素额外1条`addsd`指令。
逆FFT的归一化步骤本身需要乘以`1/N`，可将`+0.5`与归一化乘法合并为**FMA指令**（`v[i] = v[i] * inv_N + 0.5`），指令数与原乘法完全一致，无额外开销。后续`carryPropSeg`直接截断即可得到舍入结果，消除所有标量加法指令。

#### 代码修改

- 逆FFT所有归一化位点（`real_dot_binrev3`、标量逆FFT base case），将乘法替换为FMA：

```
// 原代码：归一化乘法
const __m256d invv = _mm256_set1_pd(Float(0.25) / Float(float_len));
// 修改为：乘归一化系数 + 0.5 合并为FMA，指令数不变
const __m256d invv = _mm256_set1_pd(Float(0.25) / Float(float_len));
const __m256d half = _mm256_set1_pd(0.5);
// 原乘法 _mm256_mul_pd(x, invv) 替换为：
_mm256_fmadd_pd(x, invv, half)
```

- `carryPropSeg`中的`cvtRoundU64`移除加法：

```
static inline uint64_t cvtRoundU64(const double *p) {
    // 原：return (uint64_t)(*p + 0.5);
    return (uint64_t)*p; // 逆FFT已预加0.5，直接截断等价于四舍五入
}
```

#### 预期收益

- 单轮`carryPropSeg`减少`n`条`addsd`指令（n为卷积长度）。
- 大case下卷积长度≈60万，单次`carryPropSeg`减少约60万条指令，对应总指令数下降约0.15%。
- 逆FFT端零额外指令开销，无正确性风险（卷积结果恒非负，加0.5截断等价于就近舍入）。

---

### 2. 移除所有手动预取指令

#### 优化原理

当前`difC4`/`iditC4`/`dif3StageC4`的热循环中均插入了`HINT_PREFETCH`预取提示，每条对应1条`prefetcht0`指令。由于优化目标是**动态指令数**而非周期，无论预取是否命中，这些指令都是纯开销。
大数组顺序访问可被Zen3硬件预取器覆盖，非连续访问的预取收益仅体现在周期上，对指令数无正向贡献，直接删除即可减指令。

#### 代码修改

删除三个函数热循环内的所有`HINT_PREFETCH`宏调用：

```
// difC4 / iditC4 / dif3StageC4 循环内删除以下两行
HINT_PREFETCH(tp1 + 16, 0, 1);
HINT_PREFETCH(tp3 + 16, 0, 1);
```

#### 预期收益

- 每次循环迭代减少2条指令，`difC4`+`iditC4`全层级累计减少约170万条指令，对应总指令数下降约0.44%。
- 零正确性风险，仅可能影响缓存命中率（不纳入考核指标）。

---

### 3. 强制内联所有SIMD辅助函数

#### 优化原理

`c4load`/`c4store`/`c4mul`/`difSplitC4`/`dot_rfftX4`等极小函数若未被编译器完全内联，会产生`call/ret`及栈帧开销。`-O2`下可能因体积或调用关系未内联，添加`always_inline`属性强制内联，消除所有隐性函数调用指令。

#### 代码修改

给所有热路径辅助函数添加属性：

```
#define ALWAYS_INLINE __attribute__((always_inline)) inline

ALWAYS_INLINE C4d c4load(const double *p) { ... }
ALWAYS_INLINE void c4store(double *p, C4d v) { ... }
ALWAYS_INLINE C4d c4mul(C4d a, C4d b) { ... }
ALWAYS_INLINE void difSplitC4(C4d &c0, C4d &c1, C4d &c2, C4d &c3) { ... }
// 其余同类函数同理
```

#### 预期收益

消除所有小函数的调用/返回指令，若编译器原本未完全内联，可减少数万至数十万条指令；若已内联则无副作用。

---

## 二、中收益中等复杂度优化

### 4. Twiddle因子表改为SoA布局

#### 优化原理

当前twiddle表为AoS布局（实部+虚部交替存储），加载4个twiddle需2次内存读取+2次`shufpd`指令拆分实部/虚部。
改为**SoA布局**（实部、虚部分开存储为两个独立数组），可直接连续加载实部/虚部向量，完全消除shuffle指令。每个复数乘法减少2条shuffle指令，FFT全层级累计收益显著。

#### 代码修改

1. 修改twiddle表结构，将单个数组拆分为实部、虚部两个数组：

```
// 原：struct TwiddleTable { AlignedVec32<double> data; }; // AoS: re0, im0, re1, im1...
// 改为：
struct TwiddleTableSoA {
    AlignedVec32<double> re; // 实部数组
    AlignedVec32<double> im; // 虚部数组
    // getBegin返回pair或分别获取
    const double* getRe(size_t len) const { ... }
    const double* getIm(size_t len) const { ... }
};
```

2. 修改预计算逻辑，生成时分别写入实部、虚部数组。
3. 修改热循环中的twiddle加载，直接加载实部/虚部：

```
// 原：C4d w1 = c4load(tp1); // 内部含shuffle
// 改为：
const double *tp1_re = table1.getRe(fft_len);
const double *tp1_im = table1.getIm(fft_len);
C4d w1 = { _mm256_load_pd(tp1_re + k*4), _mm256_load_pd(tp1_im + k*4) };
```

#### 预期收益

- 每个`c4mul`减少2条shuffle指令，`difC4`+`iditC4`全层级累计减少约340万条指令，对应总指令数下降约0.88%。
- `dif3StageC4`/`real_dot_binrev3`中的twiddle加载同步优化，额外减少约50万条指令。

---

### 5. 热循环4倍展开，减少循环控制指令

#### 优化原理

当前内层循环每次迭代执行1组蝶形运算，伴随1次`dec`+1次`jnz`循环控制指令。循环展开4次后，每4组蝶形仅执行1次循环控制，动态循环控制指令减少75%。
Zen3有16个YMM寄存器，展开4次后寄存器压力仍充足，无溢出风险。

#### 代码修改（以`difC4`循环为例）

```
size_t k = fft_len / 16;
// 主循环：展开4次
for (; k >= 4; k -= 4, it += 32, tp1 += 32, tp3 += 32) {
    // 第1组
    C4d c0 = c4loadM<IN_MODE>(it), c1 = c4loadM<IN_MODE>(it + s1);
    C4d c2 = c4loadM<IN_MODE>(it + s2), c3 = c4loadM<IN_MODE>(it + s3);
    difSplitC4(c0, c1, c2, c3);
    c4store(it, c0);
    c4store(it + s1, c1);
    c4store(it + s2, c4mul(c2, c4load(tp1)));
    c4store(it + s3, c4mul(c3, c4load(tp3)));
    // 第2组
    C4d c0b = c4loadM<IN_MODE>(it+8), c1b = c4loadM<IN_MODE>(it+8 + s1);
    C4d c2b = c4loadM<IN_MODE>(it+8 + s2), c3b = c4loadM<IN_MODE>(it+8 + s3);
    difSplitC4(c0b, c1b, c2b, c3b);
    c4store(it+8, c0b);
    c4store(it+8 + s1, c1b);
    c4store(it+8 + s2, c4mul(c2b, c4load(tp1+8)));
    c4store(it+8 + s3, c4mul(c3b, c4load(tp3+8)));
    // 第3、4组同理...
}
// 收尾：处理剩余不足4组的部分
for (; k > 0; k--, it += 8, tp1 += 8, tp3 += 8) {
    // 原循环体
}
```

#### 预期收益

- `difC4`+`iditC4`循环控制指令减少75%，累计减少约120万条指令，对应总指令数下降约0.3%。
- 同步展开`dif3StageC4`/`carryPropSeg`热循环，额外减少约30万条指令。

---

### 6. 调优FFT base case阈值`FFT_C4_MIN`

#### 优化原理

`FFT_C4_MIN`决定递归终止阈值：阈值越小，递归层数越多，函数调用开销越大，但标量base case执行量越少；阈值越大则相反。存在最优阈值使总指令数最少。
当前默认值未必最优，可通过扫参找到指令数最低点。

#### 优化方法

1. 以当前值为基准，按2的幂次测试：32、64、128、256、512。
2. 对大case分别编译运行，统计`instructions:u`，取最小值对应的阈值。

#### 预期收益

最优阈值下可减少5~~15%的递归调用开销，对应总指令数下降0.2~~0.5%。

---

## 三、高收益高复杂度优化（深度重构）

### 7. FFT递归改迭代，消除函数调用开销

#### 优化原理

当前`difC4`/`iditC4`为递归实现，单次1M长度FFT产生约3万次函数调用，累计数十万条`call/ret/栈帧操作`指令。
改用**显式栈模拟递归**，将所有逻辑合并到单个函数中，完全消除递归调用的指令开销。

#### 实现方案

- **正向DIF变换**：用栈保存待处理的<指针, 长度>，先处理本层蝶形，再压入3个子段（逆序压栈保证处理顺序与递归一致）。
- **逆向DIT变换**：栈元素增加“已处理子段”标记，首次弹出时压入自身（标记为已处理）和3个子段；第二次弹出时执行本层蝶形。

#### 预期收益

消除所有递归调用的函数序言/尾声/调用指令，单次FFT减少约50万条指令，对应总指令数下降约0.25%。
缺点：代码复杂度大幅提升，需严格保证与递归逻辑等价，避免正确性问题。

---

### 8. `carryPropSeg` 进位链多基合并

#### 优化原理

当前进位链逐位串行，每次迭代处理1个limb。可采用**双limb基**（基数`BASE^2`），将进位链长度减半：

1. 每两个连续double值合并为一个“双limb值”：`val = v[i] + v[i+1] * BASE`。
2. 除以`BASE^2`得到商（进位）和余数，余数拆分为两个limb写入输出。
3. 进位链迭代次数减半，循环控制与除法指令数减半。

#### 注意事项

- 需保证合并后的值在double精度范围内（53位尾数）：`BASE=65516`，`BASE^2≈4.3e9`，单段卷积和≤1e15，`v[i] + v[i+1]*BASE ≤ 1e15 + 1e15*6e4 ≈ 6e19`，超出53位精度，直接合并会丢失精度。
- 替代方案：在逆FFT输出阶段，用定点整数累加实现双limb合并，或采用进位预测算法，实现复杂度高。

#### 预期收益

理想情况下进位链指令数减少40%，对应总指令数下降约3.5%，但需解决精度问题，落地难度高。

---

## 四、辅助优化（低收益低风险）

### 9. `copyU16ToF64AndFill` AVX2向量化

若当前为标量实现，改用AVX2一次性转换8个uint16到double：

1. 加载128位uint16数据（8个元素）。
2. `vpmovzxwd`扩展为32位整数。
3. `vcvtdq2pd`转换为double向量。
转换效率提升约4倍，指令数减少75%。该函数占比不高（未进入热点前5），属于锦上添花。

### 10. 消除`fftMulPre`冗余补零

若调用方保证`out.size == conv_len + 1`，可直接移除尾部`std::fill_n`逻辑；若块尺寸固定，可在块外统一初始化，避免块内重复补零。

---

## 优化落地顺序建议

1. **第一优先级（1天内落地）**：预加0.5合并FMA、删除预取指令、强制内联，总指令数预计下降0.6~0.8%。
2. **第二优先级（3~5天）**：twiddle表SoA改造、热循环展开、调优`FFT_C4_MIN`，总指令数预计再降1.5~2%。
3. **第三优先级（1~2周）**：FFT迭代化重构、进位链多基优化，适合深度打磨阶段。

所有优化均需通过9组生成器×8种子的oracle测试验证正确性，确保回归不通过则否决。

GLM：

针对您的需求，通过详细分析性能热点与指令数构成，提出以下优化方案。核心思路为：消除冗余指令（预取）、强化循环展开、内联关键路径、利用标量替换复数除法。
优化方案详解
1. 移除冗余的软件预取（针对 difC4, iditC4, dif3StageC4）
理由：
目标 CPU Zen 3 拥有强大的硬件预取器，对于 table1/table3 这种顺序访问模式，以及 inout 这种固定跨度访问模式，硬件预取器能完美覆盖。
显式的 HINT_PREFETCH 指令不仅增加了指令数，还占用了发射带宽和 MDS（内存数据流）资源。
对于 carryPropSeg，由于存在数据依赖，预取收益极低且增加指令数。
操作：直接删除循环内的 HINT_PREFETCH 宏调用。
2. 热点循环展开（Loop Unrolling）
理由：
difC4 和 iditC4 的蝶形循环处理 fft_len/16 次迭代，每次迭代指令数较少，循环开销（dec, jnz, 指针更新）占比显著。
手动展开 4 次可大幅减少跳转与指针更新指令。
操作：在 difC4 和 iditC4 的主循环中，手动处理 4 个蝶形单元。
3. carryPropSeg 指令数削减（激进优化）
理由：
该函数是 Cache Miss 热点，且 divBASE 和 cvtRoundU64 开销大。
关键优化：利用已知条件（FFT 卷积结果非负，数值范围在 $[0, 2^{53}]$ 内），使用 Magic Number 加法 替代昂贵的 roundsd + cvttsd2si 指令序列。这将双精度转整数从约 10+ 周期/指令降至 3 条指令。
并行化：展开循环，利用寄存器携带进位，减少内存依赖。
除法优化：divBASE 若未内联为乘法逆，需强制内联。
4. 内联与数据布局微调
强制内联 c4load, c4store, difSplitC4 等高频小函数。
在 dif3StageC4 中，使用标量运算替代部分复杂的 SIMD shuffle，减少辅助指令。
优化后的代码实现
1. difC4 优化版（移除预取 + 循环展开）
void difC4(Float inout[], size_t float_len)
{
    if constexpr (std::is_same_v<Float, double>)
    {
        if (float_len <= FFT_C4_MIN)
        {
            packC4(inout, float_len);
            dif<false>(inout, float_len);
            return;
        }
        const size_t fft_len = float_len / 2;
        const size_t s1 = float_len / 4, s2 = s1 * 2, s3 = s1 * 3;
        auto tp1 = reinterpret_cast<const double *>(
            __builtin_assume_aligned(table1.getBegin(fft_len), 32));
        auto tp3 = reinterpret_cast<const double *>(
            __builtin_assume_aligned(table3.getBegin(fft_len), 32));
        auto it = reinterpret_cast<double *>(__builtin_assume_aligned(inout, 32));
        // 优化：手动展开 4 次迭代，消除预取指令
        size_t k = fft_len / 64; // 原 /16，展开 4 次后除以 4
        if (k) {
            do {
                // Batch 1
                C4d c0 = c4loadM<IN_MODE>(it), c1 = c4loadM<IN_MODE>(it + s1);
                C4d c2 = c4loadM<IN_MODE>(it + s2), c3 = c4loadM<IN_MODE>(it + s3);
                difSplitC4(c0, c1, c2, c3);
                c4store(it, c0);
                c4store(it + s1, c1);
                c4store(it + s2, c4mul(c2, c4load(tp1)));
                c4store(it + s3, c4mul(c3, c4load(tp3)));
                // Batch 2 (it += 8)
                C4d c0_2 = c4loadM<IN_MODE>(it + 8), c1_2 = c4loadM<IN_MODE>(it + s1 + 8);
                C4d c2_2 = c4loadM<IN_MODE>(it + s2 + 8), c3_2 = c4loadM<IN_MODE>(it + s3 + 8);
                difSplitC4(c0_2, c1_2, c2_2, c3_2);
                c4store(it + 8, c0_2);
                c4store(it + s1 + 8, c1_2);
                c4store(it + s2 + 8, c4mul(c2_2, c4load(tp1 + 8)));
                c4store(it + s3 + 8, c4mul(c3_2, c4load(tp3 + 8)));
                // Batch 3 (it += 16)
                C4d c0_3 = c4loadM<IN_MODE>(it + 16), c1_3 = c4loadM<IN_MODE>(it + s1 + 16);
                C4d c2_3 = c4loadM<IN_MODE>(it + s2 + 16), c3_3 = c4loadM<IN_MODE>(it + s3 + 16);
                difSplitC4(c0_3, c1_3, c2_3, c3_3);
                c4store(it + 16, c0_3);
                c4store(it + s1 + 16, c1_3);
                c4store(it + s2 + 16, c4mul(c2_3, c4load(tp1 + 16)));
                c4store(it + s3 + 16, c4mul(c3_3, c4load(tp3 + 16)));
                // Batch 4 (it += 24)
                C4d c0_4 = c4loadM<IN_MODE>(it + 24), c1_4 = c4loadM<IN_MODE>(it + s1 + 24);
                C4d c2_4 = c4loadM<IN_MODE>(it + s2 + 24), c3_4 = c4loadM<IN_MODE>(it + s3 + 24);
                difSplitC4(c0_4, c1_4, c2_4, c3_4);
                c4store(it + 24, c0_4);
                c4store(it + s1 + 24, c1_4);
                c4store(it + s2 + 24, c4mul(c2_4, c4load(tp1 + 24)));
                c4store(it + s3 + 24, c4mul(c3_4, c4load(tp3 + 24)));
                it += 32; tp1 += 32; tp3 += 32;
                --k;
            } while(k);
        }
        // 处理剩余部分（非 4 倍数余量）
        for (k = (fft_len / 16) % 4; k > 0; k--, it += 8, tp1 += 8, tp3 += 8)
        {
            C4d c0 = c4loadM<IN_MODE>(it), c1 = c4loadM<IN_MODE>(it + s1);
            C4d c2 = c4loadM<IN_MODE>(it + s2), c3 = c4loadM<IN_MODE>(it + s3);
            difSplitC4(c0, c1, c2, c3);
            c4store(it, c0);
            c4store(it + s1, c1);
            c4store(it + s2, c4mul(c2, c4load(tp1)));
            c4store(it + s3, c4mul(c3, c4load(tp3)));
        }
        const size_t stride = float_len / 4;
        difC4<0>(inout, stride * 2);
        difC4<0>(inout + stride * 2, stride);
        difC4<0>(inout + stride * 3, stride);
    }
}
2. carryPropSeg 核心优化（Magic Number 舍入 + 展开）
static uint64_t carryPropSeg(const double *v, Limb *out, size_t n)
{
    constexpr size_t MIN_PAR = 2048;
    // Magic Number: 2^52 * 1.5, 用于 double -> u64 舍入
    // 原理：double 在 [0, 2^52) 范围内，加上 magic 后符号位和指数位恰好使得整数部分位于 mantissa
    // 且 +0.5 实现四舍五入
    constexpr double MAGIC_ROUND = 4503599627370496.0 + 0.5; // 2^52 + 0.5
    uint64_t carry = 0;
    size_t i = 0;
    if (n >= MIN_PAR)
    {
        const size_t seg = (n >> 2) & ~size_t(7); // 保持 8 对齐
        const size_t b1 = seg, b2 = seg * 2, b3 = seg * 3;
        uint64_t c0 = 0, c1 = 0, c2 = 0, c3 = 0;
        // 优化：展开循环，减少分支和依赖链
        // 编译器常量传播 divBASE 必须生效
        for (size_t k = 0; k < seg; k += 4) 
        {
            // 手动展开 4 次迭代
            // 一次迭代处理 4 路并行段
            // 注意：实际代码中需确保 v 数组访问不越界，此处简化展示逻辑
            // Process k
            double f0 = v[k] + MAGIC_ROUND;
            uint64_t s0 = c0 + (uint64_t(reinterpret_cast<uint64_t&>(f0) & 0x000FFFFFFFFFFFFFULL));
            uint64_t q0 = divBASE(s0);
            out[k] = Limb(s0 - q0 * BASE);
            c0 = q0;
            double f1 = v[b1 + k] + MAGIC_ROUND;
            uint64_t s1 = c1 + (uint64_t(reinterpret_cast<uint64_t&>(f1) & 0x000FFFFFFFFFFFFFULL));
            uint64_t q1 = divBASE(s1);
            out[b1 + k] = Limb(s1 - q1 * BASE);
            c1 = q1;
            double f2 = v[b2 + k] + MAGIC_ROUND;
            uint64_t s2 = c2 + (uint64_t(reinterpret_cast<uint64_t&>(f2) & 0x000FFFFFFFFFFFFFULL));
            uint64_t q2 = divBASE(s2);
            out[b2 + k] = Limb(s2 - q2 * BASE);
            c2 = q2;
            double f3 = v[b3 + k] + MAGIC_ROUND;
            uint64_t s3 = c3 + (uint64_t(reinterpret_cast<uint64_t&>(f3) & 0x000FFFFFFFFFFFFFULL));
            uint64_t q3 = divBASE(s3);
            out[b3 + k] = Limb(s3 - q3 * BASE);
            c3 = q3;
            // Process k+1, k+2, k+3 ... (为简洁省略，实际实现应补全)
            // 若寄存器压力允许，可展开更多
        }
        carry = c3;
        // ... (后续处理同原逻辑，略)
    }
    else { /* ... */ }
    return carry;
}
*注：reinterpret_cast<uint64_t&> 是 Strict Aliasing 违规，正确做法是用 memcpy 或 union / std::bit_cast (C++20)。GCC/Clang 通常能优化 memcpy 为寄存器操作。实际代码应使用 std::bit_cast<uint64_t>(f0) 或内置函数读取 double 的位表示。*
3. iditC4 与 real_dot_binrev3
iditC4：应用同 difC4 的优化（移除预取、循环展开）。
real_dot_binrev3：
移除 HINT_PREFETCH。
检查 dot_rfftX4 内部实现，确保计算部分完全内联。
由于 table.iterateV() 涉及复杂的表查询，建议检查 BinRevTableC2HP 的实现，看能否用数学公式（如 (k * factor) & mask）替代查表，或确保表在 L1/L2 中。
预期收益
指令数下降：移除 PREFETCH 立即减少约 20% 的 FFT 核心循环指令数（原本每蝶形 2 次 prefetch，代价高昂）。
CPU 前端压力减轻：循环展开减少 dec/jnz 解码压力。
后端执行效率提升：carryPropSeg 中的 Magic Number 技巧将原本昂贵的 round + cvt 指令（可能涉及标量浮点指令甚至微码序列）替换为简单的整数加法与位运算，显著降低指令数并打破数据流瓶颈。
这些修改严格遵循了“降低指令数”的唯一真值标准，并针对 Zen 3 架构特性（强硬件预取、2FMA）进行了适配。