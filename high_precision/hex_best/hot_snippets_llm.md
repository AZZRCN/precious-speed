# HEX 大整数除法 — FFT 内核热点（待优化代码片段）

## 目标与约束（事实）
- 目标 CPU：AMD EPYC 7B13 == Zen3 (Milan)。指令集仅 AVX2（256-bit），**无 AVX-512**。每周期 2 条 FMA，每 FMA 处理 4 个 double。
- 编译命令：`g++ -march=znver3 -mtune=znver3 -O2 -std=c++20`
- 唯一真值度量：`perf stat -e instructions:u`（指令条数）。不要用墙钟时间或 cycles 评判。x86-64 二进制指令数在 Intel/AMD 上一致，故任意 x86-64 机器上测得的指令数即该 CPU 上的指令数。
- 当前基线：单次大数除法（被除数 ≈400k limbs，除数 ≈206k limbs；1 limb = uint32_t，BASE = 65516，每 limb 编码 4 位十六进制）约 **386.97M 条指令**，IPC ≈ 2.33。
- 当前已实现的优化（事实陈述，非限制）：FFT 变换核已 AVX2 向量化；逆变换用预广播的 `1/N` 常量（热循环无 vdiv）；real-packing(C4) 数据布局；固定基变换；预取；`divisor_dft` / `inv_dft` 在 `absDivMu` 块循环外各算一次、块内复用；块尺寸自适应模型已饱和。
- 已移除：cyclic 卷积（mod B^m−1，原 `fftMulModBm1`）已封存删除，请勿参考或恢复。

## 你可以自由做什么
- 目标只有一个：降低 `perf stat -e instructions:u` 的总指令数（覆盖各规模，尤其上述大 case）。
- 欢迎任何重构：函数内联 / 拆分、改变数据布局（struct-of-arrays、padding、对齐）、cache-blocking（尤其进位传播与 FFT 大数组跨块 evict）、AVX2 向量化（进位链、点乘）、分块转置、跨函数透传复用中间结果（如把已算的 DFT 往下传避免重算）、调整递归 base case 等。**不限于局部微调，启发式策略皆可尝试。**
- 下面每个函数的"功能说明"只是告诉你它在整体除法里干什么、便于你定位，**不要被它框死思路，它不是方案建议**。
- 正确性硬约束：改动后必须用既有 oracle 测试（9 组生成器 × 8 种子，覆盖 schoolbook / Newton / FFT 三条路径，含负除数/被除数 trunc 语义）全程通过，且指令数不高于基线。回归即否决。

## 除法整体流程（背景）
`absDivRem` 按规模分派：
- `len2 <= 64`：schoolbook 除法
- `len1 < len2*2`：Newton 迭代（`absInvNewtonGMP`，递归子层用 schoolbook `absMul`）
- 否则：`absDivMu`（线性 FFT 路径）——在块循环里对每个块调用 `fftMulPre` 做卷积。`fftMulPre` 内部流程：limb → double 展开 → 正向 FFT（`difC4` / `dif3StageC4`）→ 与预存的 `divisor_dft` 逐点乘 → 逆 FFT（`iditC4` / `real_dot_binrev3`）→ `carryPropSeg` 把 double 累加值还原为整数 limb 并传播进位。

## 热点排名（perf self-instructions，大 case ×10 次采样）
1. `difC4` 24.78%（FFT 正向变换核心）
2. `iditC4` 15.34%（FFT 逆变换核心）
3. `carryPropSeg` 8.92%（cache-miss 占比第 1，约 16.3%）
4. `real_dot_binrev3` 7.20%（实数 FFT 位反转点乘）
5. `dif3StageC4` 5.87%（FFT 长度含因子 3 的 radix-3 级）
6. `fftMulPre` 2.19%（其调用的 `fftMul` 占 5.49%）

---

## 1. difC4 — FFT 正向变换核心（radix-4, decimation-in-frequency）
**干什么**：对长度为 `float_len` 的实数打包（C4 格式）复数数组**原地**做正向 FFT。把数组按 radix-4 蝶形分解，twiddle 因子从预计算的 `table1` / `table3` 取；递归到更小 stride 直到 base case。模板参数 `IN_MODE` 决定 load 时是否做 real-unpack（输入是实数打包 RRII 还是纯复数）。当 `float_len` 低于阈值 `FFT_C4_MIN` 时回落到标量 `dif` + `packC4`。整个除法里它被 `fftMulPre` 的正向变换调用，是占比最高的热函数。
```cpp
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
        for (size_t k = fft_len / 16; k > 0; k--, it += 8, tp1 += 8, tp3 += 8)
        {
            HINT_PREFETCH(tp1 + 16, 0, 1);
            HINT_PREFETCH(tp3 + 16, 0, 1);
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
```

## 2. iditC4 — FFT 逆变换核心（radix-4, decimation-in-time，含 1/N 归一化）
**干什么**：`difC4` 的对称逆操作。先递归后做蝶形，且用 `c4mulConj`（乘 twiddle 共轭）实现逆变换；`OUT_MODE` 控制输出打包格式（是否做 real-pack）。也是 `fftMulPre` 逆变换调用的最热函数。注意其循环顺序与 `difC4` 相反（先递归三子段，再做本层蝶形）。
```cpp
void iditC4(Float inout[], size_t float_len)
{
    if constexpr (std::is_same_v<Float, double>)
    {
        if (float_len <= FFT_C4_MIN)
        {
            idit<false>(inout, float_len);
            packC4(inout, float_len);
            return;
        }
        const size_t stride = float_len / 4;
        iditC4<0>(inout, stride * 2);
        iditC4<0>(inout + stride * 2, stride);
        iditC4<0>(inout + stride * 3, stride);
        const size_t fft_len = float_len / 2;
        const size_t s1 = float_len / 4, s2 = s1 * 2, s3 = s1 * 3;
        auto tp1 = reinterpret_cast<const double *>(
            __builtin_assume_aligned(table1.getBegin(fft_len), 32));
        auto tp3 = reinterpret_cast<const double *>(
            __builtin_assume_aligned(table3.getBegin(fft_len), 32));
        auto it = reinterpret_cast<double *>(__builtin_assume_aligned(inout, 32));
        for (size_t k = fft_len / 16; k > 0; k--, it += 8, tp1 += 8, tp3 += 8)
        {
            HINT_PREFETCH(tp1 + 16, 0, 1);
            HINT_PREFETCH(tp3 + 16, 0, 1);
            C4d c0 = c4load(it), c1 = c4load(it + s1);
            C4d c2 = c4mulConj(c4load(it + s2), c4load(tp1));
            C4d c3 = c4mulConj(c4load(it + s3), c4load(tp3));
            iditSplitC4(c0, c1, c2, c3);
            c4storeM<OUT_MODE>(it, c0);
            c4storeM<OUT_MODE>(it + s1, c1);
            c4storeM<OUT_MODE>(it + s2, c2);
            c4storeM<OUT_MODE>(it + s3, c3);
        }
    }
}
```

## 3. carryPropSeg — radix-B 进位重构（cache-miss 第一）
**干什么**：FFT 卷积结果是 `double` 累加值数组 `v`（每个位置 = 对 BASE=65516 的带权系数）。此函数把 `v[0..n)` 还原为整数 limb 数组 `out`：对每个元素 round 到最近整数、减去 BASE 的整数倍得余数写入 `out`、商作为进位传递到下一位。采用 4 路分段并行 + 段边界涟漪修正以支持进位跨段传播。它是 cache-miss 占比第一的热点，因大数组顺序扫描且 `cvtRoundU64`（double→u64 舍入取整）密集；`MIN_PAR` 以下走串行。`divBASE` 是除以 BASE 的常量除法（建议先看它是否被编译成 mul+shift）。
```cpp
static uint64_t carryPropSeg(const double *v, Limb *out, size_t n)
{
    constexpr size_t MIN_PAR = 2048;
    uint64_t carry = 0;
    size_t i = 0;
    if (n >= MIN_PAR)
    {
        const size_t seg = (n >> 2) & ~size_t(7);
        const size_t b1 = seg, b2 = seg * 2, b3 = seg * 3;
        uint64_t c0 = 0, c1 = 0, c2 = 0, c3 = 0;
        for (size_t k = 0; k < seg; ++k)
        {
            uint64_t s0 = c0 + cvtRoundU64(v + k);
            uint64_t s1 = c1 + cvtRoundU64(v + b1 + k);
            uint64_t s2 = c2 + cvtRoundU64(v + b2 + k);
            uint64_t s3 = c3 + cvtRoundU64(v + b3 + k);
            uint64_t q0 = divBASE(s0);
            uint64_t q1 = divBASE(s1);
            uint64_t q2 = divBASE(s2);
            uint64_t q3 = divBASE(s3);
            out[k]      = Limb(s0 - q0 * BASE);
            out[b1 + k] = Limb(s1 - q1 * BASE);
            out[b2 + k] = Limb(s2 - q2 * BASE);
            out[b3 + k] = Limb(s3 - q3 * BASE);
            c0 = q0; c1 = q1; c2 = q2; c3 = q3;
        }
        carry = c3;
        for (i = b3 + seg; i < n; ++i)
        {
            carry += cvtRoundU64(v + i);
            uint64_t q = divBASE(carry);
            out[i] = Limb(carry - q * BASE);
            carry = q;
        }
        uint64_t ov = c0;
        for (size_t p = b1; ov > 0 && p < b2; ++p)
        {
            uint64_t s = uint64_t(out[p]) + ov;
            uint64_t q = divBASE(s);
            out[p] = Limb(s - q * BASE);
            ov = q;
        }
        ov += c1;
        for (size_t p = b2; ov > 0 && p < b3; ++p)
        {
            uint64_t s = uint64_t(out[p]) + ov;
            uint64_t q = divBASE(s);
            out[p] = Limb(s - q * BASE);
            ov = q;
        }
        ov += c2;
        for (size_t p = b3; ov > 0 && p < n; ++p)
        {
            uint64_t s = uint64_t(out[p]) + ov;
            uint64_t q = divBASE(s);
            out[p] = Limb(s - q * BASE);
            ov = q;
        }
    }
    else
    {
        for (i = 0; i < n; ++i)
        {
            carry += cvtRoundU64(v + i);
            uint64_t q = divBASE(carry);
            out[i] = Limb(carry - q * BASE);
            carry = q;
        }
    }
    return carry;
}
```

## 4. real_dot_binrev3 — 实数 FFT 位反转点乘（长度含因子 3 的基变换）
**干什么**：实数 FFT 的"虚部对称重组"关键步骤。输入 `in` 是 real FFT 的半长结果（按位反转 binrev 顺序），本函数在位反转顺序下与 twiddle 因子做点乘，原地重构到 `in_out`。分两块：块 0 与 2 的幂长度实数解包同构（`real_dot_binrev<2>` 循环）；块 1/块 2 互为镜像、twiddle 乘以常数 `w_FL`。长度 `float_len` 含因子 3（故与 `dif3StageC4` 配合）。`fftMulPre` 的逆变换路径会调用它。**注：以下为 perf 采样到的片段（在原文件中未完整截断于本段），完整定义见 `div_base16.cpp` 的 `real_dot_binrev3`；热点主体是前半段块 0 的 `real_dot_binrev<2>` / AVX2 循环。**
```cpp
inline void real_dot_binrev3(Float in_out[], const Float in[], size_t float_len)
{
    using F2 = Float2<Float>;
    using C2 = Complex2<Float>;
    const size_t m = float_len / 6, blk = m * 2;
    const Float inv = Float(1) / Float(float_len);
    const F2 invx = F2::from1(Float(0.25) / Float(float_len));
    static thread_local BinRevTableC2HP<Float> table(31, 32);
    real_dot_binrev<2>(in_out, in, 16, inv);
    for (size_t begin = 16; begin < blk; begin *= 2)
    {
        table.reset(begin / 2);
        if constexpr (std::is_same_v<Float, double>)
        {
            if (begin >= 32)
            {
                const __m256d invv = _mm256_set1_pd(Float(0.25) / Float(float_len));
                double *q0 = in_out + begin, *q1 = q0 + begin - 8;
                const double *q2 = in + begin, *q3 = q2 + begin - 8;
                for (size_t u = begin / 16; u > 0; u--, q0 += 8, q1 -= 8, q2 += 8, q3 -= 8)
                {
                    const __m256d w0 = table.iterateV();
                    const __m256d w1 = table.iterateV();
                    dot_rfftX4(q0, q1, q2, q3, c4twiddle(w0, w1), invv);
                }
                continue;
            }
        }
        auto it0 = in_out + begin, it1 = it0 + begin - 4;
        auto it2 = in + begin, it3 = it2 + begin - 4;
        for (; it0 < it1; it0 += 4, it1 -= 4, it2 += 4, it3 -= 4)
        {
            dot_rfftX2(it0, it1, it2, it3, table.iterate(), invx);
        }
    }
    Float *o1 = in_out + blk, *o2 = in_out + blk * 2;
    const Float *n1 = in + blk, *n2 = in + blk * 2;
    const Float ang = Float(-HINT_2PI) / Float(float_len);
    const C2 rot(Float(std::cos(ang)), Float(std::sin(ang)));
    {
        const C2 *sm = smallOmega8<Float>();
        for (size_t t = 0; t < 4; t++)
        {
            const size_t f = 4 * t;
            dot_rfftX2(o1 + f, o2 + blk - 4 - f, n1 + f, n2 + blk - 4 - f,
                       sm[t].mul(rot), invx);
        }
    }
    for (size_t begin = 16; begin < blk; begin *= 2)
    {
        table.reset(begin / 2);
        if constexpr (std::is_same_v<Float, double>)
        {
            const __m256d invv = _mm256_set1_pd(Float(0.25) / Float(float_len));
            const C4d rotv{_mm256_set1_pd(rot.real.x0), _mm256_set1_pd(rot.imag.x0)};
            double *p0 = o1 + begin, *p1 = o2 + blk - 8 - begin;
        }
    }
}
```

## 5. dif3StageC4 — FFT 长度含因子 3 的 radix-3 级（C4 格式，正向）
**干什么**：当 FFT 长度含因子 3 时，用它对一段做 3 点 DFT 蝶形（替代 radix-4 的一级）。用 `SQRT3_DIV2` 常数做对称的 3 点组合，twiddle 从 `getTable3a` / `getTable3b` 取。`RIRI_IN` 控制输入打包格式。`difC4` 递归到含 3 因子的子长度时调用它。
```cpp
inline void dif3StageC4(double *inout, size_t m)
{
    const __m256d vh = _mm256_set1_pd(0.5);
    const __m256d vs = _mm256_set1_pd(double(SQRT3_DIV2));
    const __m256d vns = _mm256_set1_pd(-double(SQRT3_DIV2));
    auto &t1 = getTable3a<double>();
    auto &t2 = getTable3b<double>();
    t1.expand(m), t2.expand(m);
    auto p1 = reinterpret_cast<const double *>(__builtin_assume_aligned(t1.getBegin(m), 32));
    auto p2 = reinterpret_cast<const double *>(__builtin_assume_aligned(t2.getBegin(m), 32));
    double *b0 = reinterpret_cast<double *>(__builtin_assume_aligned(inout, 32));
    double *b1 = b0 + m * 2, *b2 = b0 + m * 4;
    for (size_t c = m / 4; c > 0; c--, b0 += 8, b1 += 8, b2 += 8, p1 += 8, p2 += 8)
    {
        HINT_PREFETCH(p1 + 16, 0, 1);
        HINT_PREFETCH(p2 + 16, 0, 1);
        const C4d a0 = c4loadM<RIRI_IN ? 2 : 1>(b0);
        const C4d a1 = c4loadM<RIRI_IN ? 2 : 1>(b1);
        const C4d a2 = c4loadM<RIRI_IN ? 2 : 1>(b2);
        const __m256d sr = a1.re + a2.re, si = a1.im + a2.im;
        const __m256d dr = a1.re - a2.re, di = a1.im - a2.im;
        const __m256d hr = a0.re - sr * vh, hi = a0.im - si * vh;
        const __m256d gr = di * vns, gi = dr * vs;
        c4store(b0, C4d{a0.re + sr, a0.im + si});
        c4store(b1, c4mul(C4d{hr - gr, hi - gi}, c4loadC2(p1)));
        c4store(b2, c4mul(C4d{hr + gr, hi + gi}, c4loadC2(p2)));
    }
}
```

## 6. fftMulPre — 预计算 DFT 的卷积乘（absDivMu 块循环主体）
**干什么**：给定**已 DFT 过**的 `b_dft` 和待卷积的整数 `a`，完成一次线性卷积：把 `a` 从 limb（uint16 打包）展开并补零到 double 数组 `v` → 正向 FFT → 与 `b_dft` 逐点乘 → 逆 FFT → `carryPropSeg` 还原整数卷积写入 `out`。在 `absDivMu` 块循环里 `divisor_dft` 块外算一次、块内复用，故本函数只持 `b_dft` 不重算它。返回的 `carry` 是最高位进位。
```cpp
static void fftMulPre(View a, const double *b_dft, size_t b_len, size_t float_len, Span out)
{
    size_t a_len = count_true_length(a.ptr, a.size);
    if (a_len == 0)
    {
        std::fill_n(out.ptr, out.size, Limb(0));
        return;
    }
    size_t conv_len = a_len + b_len - 1;
    assert(float_len >= conv_len);
    HINT_ASSUME(float_len >= conv_len);
    thread_local AlignedVec32<double> tv;
    if (tv.capacity() < float_len) tv.reserve(float_len);
    double *v = tv.data();
    copyU16ToF64AndFill(a.ptr, v, a_len, float_len);
    transform::fft::rdif(v, float_len);
    transform::fft::rdot(v, b_dft, float_len);
    transform::fft::ridit(v, float_len);
    uint64_t carry = carryPropSeg(v, out.ptr, conv_len);
    out[conv_len] = Limb(carry);

    if (out.size > conv_len + 1)
    {
        std::fill_n(out.ptr + conv_len + 1, out.size - conv_len - 1, Limb(0));
    }
}
```
