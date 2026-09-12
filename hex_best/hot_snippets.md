# HEX 除法 FFT 内核热点代码片段（EPYC 7B13 / znver3 约束，供线上 LLM 优化）

## 平台与度量约束（必读）
- **目标 CPU**：AMD EPYC 7B13 = Zen3 (Milan)。仅 **AVX2（256-bit）**，**无 AVX-512**。
  FMA 吞吐 = **2 / cycle**（每 FMA 处理 4 个 double）。编译：`-march=znver3 -mtune=znver3 -O2 -std=c++20`。
- **唯一真值度量**：`perf stat -e instructions:u`（指令数）。**禁止墙钟/cycles**（你铁律：x86-64 二进制指令数 Intel/AMD 一致，VM 上测即 7B13 上测）。
- **当前基线**：大 case（A≈400k limbs / B≈206k limbs）单次除法 = **386.97M 指令**，IPC=2.33（已贴 Zen3 FMA 上限）。
- **已优化项（勿重复）**：FFT 核已 AVX2 向量化 + 预计算 `1/N` 广播（无热循环 vdiv）+ real-packing(C4) + 固定基变换 + 预取；`divisor_dft`/`inv_dft` 已在 absDivMu 块循环外算一次、块内复用；块尺寸 `muBlockCost` 自适应模型已饱和（波动<0.05%）。

## 热点排名（perf self-instructions, 大 case ×10）
1. `difC4` 24.78% — FFT 正向变换核心
2. `iditC4` 15.34% — FFT 逆变换核心
3. `carryPropSeg` 8.92%（且 cache-miss 第1，16.3%）— radix-B 进位重构
4. `real_dot_binrev3` 7.20% — 点乘 twiddle 迭代
5. `dif3StageC4` 5.87% — radix-3 级
6. `fftMulPre` 2.19%（含 `fftMul` 5.49%）— 预计算 DFT 乘法

## 给线上 LLM 的优化提示
- 总指令 ≈71% 在 FFT 变换核（dif/idit + 3stage + real_dot）。**唯一再抬 FMA 吞吐的是 AVX-512，但 7B13 不支持**——所以纯计算层面已近极限。
- **最有价值的可优化点不是计算而是数据局部性**：`carryPropSeg` 是 cache-miss 第一（16.3%），可尝试 **cache-blocking 进位传播** 或 **AVX2 向量化进位链**；FFT 大数组跨块 evict L3，可尝试小块转置/分块。
- `absInvNewtonGMP` 递归子层用 `absMul`（schoolbook O(k²)）而非 `m_dft+fftMulPre`，因 m 切片每层不同未传 `m_dft`——重构传 `m_dft` 可能省 DFT 但增复杂度，需实测。
- 任何改动必须以 9gen×8seed oracle 守口 + perf 指令数下降为收口，禁止回归。

---

## 1. difC4  (L1363-1400, 24.78% self instructions)
```cpp
                void difC4(Float inout[], size_t float_len)
                {
                    if constexpr (std::is_same_v<Float, double>)
                    {
                        if (float_len <= FFT_C4_MIN)
                        {
                            // 只有递归内部 (IN_MODE==0) 会落到叶子: 最外层进来时 float_len > FFT_C4_MIN
                            packC4(inout, float_len); // C4 -> RRII, 回到原路径
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
                template <int OUT_MODE>
                void iditC4(Float inout[], size_t float_len)
```

## 2. iditC4  (L1400-1440, 15.34% self instructions)
```cpp
                void iditC4(Float inout[], size_t float_len)
                {
                    if constexpr (std::is_same_v<Float, double>)
                    {
                        if (float_len <= FFT_C4_MIN)
                        {
                            idit<false>(inout, float_len);
                            packC4(inout, float_len); // RRII -> C4, 交回上层
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

                template <bool RIRI_IN>
                HINT_AI_SMALL void difSmall(Float inout[], size_t float_len)
                {
                    if (float_len <= 2)
```

## 3. carryPropSeg  (L3604-3665, 8.92% inst + 16.3% cache-miss)
```cpp
        static uint64_t carryPropSeg(const double *v, Limb *out, size_t n)
        {
            constexpr size_t MIN_PAR = 2048;   // 小规模并行化不划算, 走原串行路径
            uint64_t carry = 0;
            size_t i = 0;
            if (n >= MIN_PAR)
            {
                const size_t seg = (n >> 2) & ~size_t(7);
                const size_t b1 = seg, b2 = seg * 2, b3 = seg * 3;
                uint64_t c0 = 0, c1 = 0, c2 = 0, c3 = 0;
                for (size_t k = 0; k < seg; ++k)
                {
                    // D44: 删除手工预取。4 条流均为顺序步进, 硬件 L2 stream
                    // prefetcher 已覆盖; 而条件预取块每轮固定付 test+jne, 且 4 个
                    // 预取地址常驻抬高寄存器压力, 逼 GCC 每轮把 q0..q3 溢出到栈。
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
                // 尾部 [4*seg, n) 紧接段 3, 用 c3 继续串行
                carry = c3;
                for (i = b3 + seg; i < n; ++i)
                {
                    carry += cvtRoundU64(v + i);
                    uint64_t q = divBASE(carry);
                    out[i] = Limb(carry - q * BASE);
                    carry = q;
                }
                // 段边界涟漪 (低 -> 高)
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
```

## 4. real_dot_binrev3  (L2425-2485, 7.20% self instructions)
```cpp
            inline void real_dot_binrev3(Float in_out[], const Float in[], size_t float_len)
            {
                using F2 = Float2<Float>;
                using C2 = Complex2<Float>;
                const size_t m = float_len / 6, blk = m * 2;
                const Float inv = Float(1) / Float(float_len);
                const F2 invx = F2::from1(Float(0.25) / Float(float_len));
                static thread_local BinRevTableC2HP<Float> table(31, 32);
                // ---- 块0: 与 2 幂长度 blk 的实数解包完全同构 ----
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
                // ---- 块1[p] <-> 块2[M-1-p], twiddle 同序列 * 常数 w_{FL} ----
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
                // 注意: begin 是"块内 float 偏移", 块1 有 M complex = blk floats,
                // 因此层循环上界必须是 blk (不是 m=M, 那是 complex 计数, 会漏掉最后一层)
                for (size_t begin = 16; begin < blk; begin *= 2)
                {
                    table.reset(begin / 2);
                    if constexpr (std::is_same_v<Float, double>)
                    {
                        // begin/4 次 C2 迭代 (begin >= 16 -> 至少 4 次) -> begin/8 次 C4
                        const __m256d invv = _mm256_set1_pd(Float(0.25) / Float(float_len));
                        const C4d rotv{_mm256_set1_pd(rot.real.x0), _mm256_set1_pd(rot.imag.x0)};
                        double *p0 = o1 + begin, *p1 = o2 + blk - 8 - begin;
```

## 5. dif3StageC4  (L2165-2193, 5.87% self instructions)
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
            template <bool RIRI_OUT>
```

## 6. fftMulPre  (L3912-3973, 含 fftMul 调用; 2.19% self)
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

        // fftMulModBm1: 计算 a * b mod (B^m - 1), 结果长度 m (cyclic convolution)
        // m 必须是合法 FFT 长度 (2^k 或 3*2^k), 且 a_len <= m, b_len <= m
        //   模数 B^m-1 只要求 m 落在调用方的整数区间内, 对 m 的因子分解无要求;
        //   放开 3*2^k 后 mn=fft_ceil(k+1) 最坏 1.5k (原 int_ceil2 最坏 2k) -> cyclic 死区消失.
        // 用途: absInvNewton GMP 风格两步分解, FFT 长度从 ceil2(2m-1) 降到 m
        // 算法: cyclic FFT (float_len=m) + 进位传播 + cyclic carry 折叠 (B^m ≡ 1 mod B^m-1)
        static void fftMulModBm1(View a, View b, size_t m, Span out)
        {
            assert(is_2pow(m) || is_fft3(m) || is_fft5(m));
            assert(out.size >= m);
            size_t a_len = count_true_length(a.ptr, a.size);
            size_t b_len = count_true_length(b.ptr, b.size);
            if (a_len == 0 || b_len == 0)
            {
                std::fill_n(out.ptr, m, Limb(0));
                if (out.size > m)
                    std::fill_n(out.ptr + m, out.size - m, Limb(0));
                return;
            }
            assert(a_len <= m && b_len <= m);

            thread_local AlignedVec32<double> tv;
            if (tv.capacity() < 2 * m) tv.reserve(2 * m);
            double *va = tv.data();
            double *vb = tv.data() + m;

            copyU16ToF64AndFill(a.ptr, va, a_len, m);
            copyU16ToF64AndFill(b.ptr, vb, b_len, m);

            transform::fft::rdif(va, m);
            transform::fft::rdif(vb, m);
            transform::fft::rdot(va, vb, m);
            transform::fft::ridit(va, m);

            // 进位传播 (8 路展开, 同 fftMulPre)
```

