# 超优化任务书 T1 —— AVX2 高基数融合蝶形（radix-8 / radix-16）

> 目标读者：具备 x86 SIMD 指令级优化能力的模型/工程师
> 提出者：已完成瓶颈定位与收益建模，但寄存器分配 + twiddle 调度的手工设计超出单人可靠完成的范围
> 交付要求：**可编译、可验证、逐字节正确**的替换实现

---

## 0. TL;DR

把当前的 **radix-4 融合蝶形**（一次 load/store 覆盖 2 层 FFT）升级为
**radix-8（覆盖 3 层）或 radix-16（覆盖 4 层）**，
目的**不是**减少浮点运算量，而是**减少数据在 L1↔L2 之间的往返次数**。

- 预期收益：**总运行时间 −3.3%（radix-8）~ −6.4%（radix-16）**
- 核心难点：AVX2 只有 16 个 ymm 寄存器，radix-16 需同时持有 16~32 个复数点 + 15 个 twiddle，
  朴素写法必然 spill 到栈，反而变慢。**需要精细的寄存器分配与 load/store 调度。**
- 关键助力（已由提出者推导，务必利用）：15 个 twiddle 中**只有 8 个需要从内存 load**，
  其余 7 个可由 `·i` 旋转（permute + xor 符号位，2 uops）免费得到。详见 §4。

---

## 1. 背景

### 1.1 应用
HEX 大整数乘法。输入两个 1.6M 十六进制位的整数，输出乘积。
核心是 **AVX2 双精度复数 FFT**，规模 **n = 2^19 复数点 = 8 MiB**（`__m128d` 每点 16 B）。

一次乘法执行：**2 次前向 FFT（DIF）+ 1 次逆向 FFT（DIT）+ 点乘**。

### 1.2 目标硬件
| 项 | 值 |
|---|---|
| 开发/测量机 | Intel i7-11370H **Tiger Lake**（Willow Cove 核） |
| 指令集 | AVX2 + FMA + BMI/BMI2（**AVX-512 存在但禁止使用**，见 §6 约束） |
| L1D | 48 KiB, 12-way |
| L2 | 1.25 MiB/core |
| L3 | 12 MiB（真实值；VMware 误报 36 MiB，勿信） |
| 提交目标机 | 疑似 AMD **Zen3**（L1D 32 KiB/8-way, L2 512 KiB/8-way, L3 32 MiB/16-way per CCX） |

> **写代码时按 L1D = 32 KiB（Zen3 的保守值）设计**，这样两边都不会翻车。

### 1.3 已实测的瓶颈画像（`perf stat`，单次 max_max 用例）

```
cycles:u        64,716,313     (78%)
cycles:k        18,399,387     (22%)
instructions:u 129,211,060     IPC = 2.00
uops_dispatched.port_0   26,093,190   → 39% 占用
uops_dispatched.port_1   26,780,991   → 40% 占用
uops_dispatched.port_5   24,071,589   → 36% 占用
fp_arith_inst_retired.256b_packed_double  35,127,283
```

cachegrind（Zen3 几何 `--I1=32768,8,64 --D1=32768,8,64 --LL=524288,8,64`）：
```
I refs      130,605,689
D refs       42,526,997
D1  misses    3,487,903
LLd misses    2,626,912   (L2→L3)
```

perf 采样热点（self%，30 轮累积）：
```
bf2Fwd            18.59%   ← 本任务目标
bf2Inv            12.99%   ← 本任务目标
difRec             7.85%
split_b2           5.78%
difRecZeroHi       4.97%
ditRec             3.99%
                  ------
蝶形族合计         ≈ 44%
```

### 1.4 结论：**不是 FP 端口受限，是访存受限**

三个 FP/shuffle 端口占用都只有 36~40%，远未饱和。
而 stall 分析显示 29M stall cycles 中 19.8M 是 `mem_any`。

**佐证实验（已完成，强证据）**：把 LEAF 从 2^11 改到 2^7，
Ir **上升** 1.39%，但 D1 miss **下降** 16.34%，实测墙钟 **快 2.33%**（p=0.0072, 17/21 wins）。
→ 每降低 1% 的 D1 miss ≈ 换来 0.143% 的时间。**减访存 > 减指令。**

> 因此：**不要**尝试 split-radix / tangent FFT 之类"减少浮点乘法数"的算法。
> 那条路在本机器上无收益（FP 端口本来就闲）。**唯一有效的方向是减少数据往返。**

---

## 2. 当前实现（radix-4，待替换）

完整可编译上下文见同目录 `_fft_core.txt`。以下是核心部分。

### 2.1 基础类型与复乘原语

```cpp
namespace fft {
using cpx = __m128d;                       // 一个复数：[re, im]，低位 re

// 一个 __m256d 装 2 个复数：[re0, im0, re1, im1]

static inline __m256d cmul4(__m256d y, __m256d wb, __m256d wsw) {  // y * w （2 组并行）
    __m256d ylo = _mm256_unpacklo_pd(y, y), yhi = _mm256_unpackhi_pd(y, y);
    return _mm256_fmaddsub_pd(ylo, wb, _mm256_mul_pd(yhi, wsw));
}
static inline __m256d cmulconj4(__m256d y, __m256d wlo, __m256d whi) {  // y * conj(w)
    return _mm256_fmsubadd_pd(wlo, y, _mm256_mul_pd(whi, _mm256_permute_pd(y, 0x5)));
}
#define BC4(w) _mm256_set_m128d((w), (w))   // 把一个复数广播到 ymm 的两个 128 位 lane
// 配套：  W  = BC4(w);  Ws = _mm256_permute_pd(W, 0x5);      // 供 cmul4 用
//        Wl = _mm256_unpacklo_pd(W,W); Wh = _mm256_unpackhi_pd(W,W);  // 供 cmulconj4 用
```

### 2.2 前向 radix-4 蝶形（DIF，融合 2 层）

```cpp
// 层1：跨度 bs = 2q，twiddle w0
// 层2：跨度 q，左半 twiddle w1、右半 twiddle w2
static inline void bf2Fwd(cpx* s, u32 q, cpx w0, cpx w1, cpx w2) {
    const u32 bs = q << 1;
    const __m256d W0 = BC4(w0), W0s = _mm256_permute_pd(W0, 0x5);
    const __m256d W1 = BC4(w1), W1s = _mm256_permute_pd(W1, 0x5);
    const __m256d W2 = BC4(w2), W2s = _mm256_permute_pd(W2, 0x5);
    cpx* e = s + q;
    for (cpx* p = s; p + 2 <= e; p += 2) {
        __m256d x0 = _mm256_loadu_pd((const double*)p);
        __m256d x1 = _mm256_loadu_pd((const double*)(p + q));
        __m256d t2 = cmul4(_mm256_loadu_pd((const double*)(p + bs)),     W0, W0s);
        __m256d t3 = cmul4(_mm256_loadu_pd((const double*)(p + bs + q)), W0, W0s);
        __m256d a0 = _mm256_add_pd(x0, t2), a2 = _mm256_sub_pd(x0, t2);
        __m256d a1 = _mm256_add_pd(x1, t3), a3 = _mm256_sub_pd(x1, t3);
        __m256d u1 = cmul4(a1, W1, W1s), u3 = cmul4(a3, W2, W2s);
        _mm256_storeu_pd((double*)p,          _mm256_add_pd(a0, u1));
        _mm256_storeu_pd((double*)(p + q),    _mm256_sub_pd(a0, u1));
        _mm256_storeu_pd((double*)(p + bs),   _mm256_add_pd(a2, u3));
        _mm256_storeu_pd((double*)(p+bs+q),   _mm256_sub_pd(a2, u3));
    }
    /* q == 1 的标量尾巴见 _fft_core.txt */
}
```

内循环成本（每次处理 2 lane = 2 个 radix-4 蝶形 = 8 个复数）：
- 4 × `loadu_pd`, 4 × `storeu_pd`
- 4 × `cmul4` = 4 mul + 4 fmaddsub + **8 shuffle(p5)**
- 8 × add/sub
→ 16 FP-port uops（p0/p1，8 cycle）+ 8 shuffle uops（p5，8 cycle）+ 8 mem uops

### 2.3 逆向 radix-4 蝶形（DIT，乘 conj，融合 2 层）

```cpp
static inline void bf2Inv(cpx* s, u32 q, cpx w0, cpx w1, cpx w2) {
    const u32 bs = q << 1;
    const __m256d W0 = BC4(w0), W0l = _mm256_unpacklo_pd(W0,W0), W0h = _mm256_unpackhi_pd(W0,W0);
    const __m256d W1 = BC4(w1), W1l = _mm256_unpacklo_pd(W1,W1), W1h = _mm256_unpackhi_pd(W1,W1);
    const __m256d W2 = BC4(w2), W2l = _mm256_unpacklo_pd(W2,W2), W2h = _mm256_unpackhi_pd(W2,W2);
    cpx* e = s + q;
    for (cpx* p = s; p + 2 <= e; p += 2) {
        __m256d x0 = _mm256_loadu_pd((const double*)p);
        __m256d x1 = _mm256_loadu_pd((const double*)(p + q));
        __m256d x2 = _mm256_loadu_pd((const double*)(p + bs));
        __m256d x3 = _mm256_loadu_pd((const double*)(p + bs + q));
        __m256d a0 = _mm256_add_pd(x0, x1), a1 = cmulconj4(_mm256_sub_pd(x0, x1), W1l, W1h);
        __m256d a2 = _mm256_add_pd(x2, x3), a3 = cmulconj4(_mm256_sub_pd(x2, x3), W2l, W2h);
        _mm256_storeu_pd((double*)p,        _mm256_add_pd(a0, a2));
        _mm256_storeu_pd((double*)(p+bs),   cmulconj4(_mm256_sub_pd(a0, a2), W0l, W0h));
        _mm256_storeu_pd((double*)(p+q),    _mm256_add_pd(a1, a3));
        _mm256_storeu_pd((double*)(p+bs+q), cmulconj4(_mm256_sub_pd(a1, a3), W0l, W0h));
    }
}
```

### 2.4 递归驱动器

```cpp
#define FFT_LEAF_LOG 7          // ← 已调优确定：2^7 = 128 复数 = 2 KiB

static void difRec(cpx* d, u32 n, u32 bb) {
    if (n <= (1u << FFT_LEAF_LOG)) { difFlat(d, n, bb); return; }
    const u32 q = n >> 2, b4 = bb << 2;
    if (bb == 0) bf2FwdOne(d, q, tw[1]);                       // w0==w1==1 的特化
    else         bf2Fwd(d, q, tw[bb], tw[bb<<1], tw[(bb<<1)|1]);
    difRec(d,       q, b4);
    difRec(d +   q, q, b4|1);
    difRec(d + 2*q, q, b4|2);
    difRec(d + 3*q, q, b4|3);
}

static void ditRec(cpx* d, u32 n, u32 bb) {          // DIT：子调用在前，蝶形在后
    if (n <= (1u << FFT_LEAF_LOG)) { ditFlat(d, n, bb); return; }
    const u32 q = n >> 2, b4 = bb << 2;
    ditRec(d,       q, b4);
    ditRec(d +   q, q, b4|1);
    ditRec(d + 2*q, q, b4|2);
    ditRec(d + 3*q, q, b4|3);
    if (bb == 0) bf2InvOne(d, q, tw[1]);
    else         bf2Inv(d, q, tw[bb], tw[bb<<1], tw[(bb<<1)|1]);
}

// difFlat / ditFlat：leaf 内的迭代式实现，逐层扫，见 _fft_core.txt
```

**这是一个自排序（self-sorting）DIF/DIT 对**：DIF 输出为 bit-reversed 序，
点乘在该序下进行，DIT 再变回自然序。**不存在独立的 bit-reversal permutation pass。**

---

## 3. 为什么高基数能赢：收益模型

设 `L = log2(n) = 19` 层（radix-2 等价层数）。

一个 radix-`2^r` kernel 一次 load/store 往返覆盖 `r` 层。
只有当子问题规模 **超过 L1D** 时，这次往返才产生真实的 L1 miss。

L1D 按 32 KiB（Zen3）算 = 2048 复数 = 2^11 点。
从 2^19 递归到 2^11 需剥掉 **8 层**。

| kernel | 覆盖层数 | 超-L1 往返次数 | 相对 radix-4 |
|---|---|---|---|
| radix-4（现状） | 2 | 8/2 = **4** | — |
| **radix-8** | 3 | ⌈8/3⌉ = **3** | 省 1 次 |
| **radix-16** | 4 | 8/4 = **2** | 省 2 次 |

每次超-L1 往返 = 8 MiB read + 8 MiB write = 262,144 cache lines。
一次大整数乘法跑 3 遍 FFT（2 DIF + 1 DIT）→ **786K lines / 往返**。

| 方案 | D1 miss 预估 | Δ vs 现状 3.49M | 时间预估（× 0.143） |
|---|---|---|---|
| radix-8 | 3.49M − 0.79M = **2.70M** | −22.6% | **−3.2%** |
| radix-16 | 3.49M − 1.57M = **1.92M** | −45.0% | **−6.4%** |

> 系数 0.143 由 LEAF 实验标定（D1 −16.34% ↔ 时间 −2.33%）。
> 这是**保守线性外推**，实际可能因 TLB/预取效应偏离，以实测为准。

---

## 4. Twiddle 表：精确语义 + 两条可省 load 的恒等式（**关键**）

### 4.1 表的定义

```cpp
alignas(1<<21) static __m128d twbuf[LMMAX >> 1];   // LMMAX = 1<<20，故表最大 2^19 项 = 8 MiB
static cpx* tw = twbuf;

static void resize(u32 n) {                      // n = 复数点数
    if (n <= (twlen << 1)) return;
    u32 halfLog = (u32)(31 - __builtin_clz(n)) >> 1, halfSize = 1u << halfLog;
    alignas(64) static cpx base[1u << 12];
    const double a0 = M_PI / halfSize, a1 = a0 / halfSize;
    for (u32 i = 0, j = (halfSize*3)>>1, p = 0; i != halfSize;
             p -= halfSize - (j >> __builtin_ctz(++i))) {
        int32_t sp = (int32_t)p;
        std::complex<double> f = std::polar(1.0, sp*a0), s = std::polar(1.0, sp*a1);
        base[i]            = _mm_set_pd(f.imag(), f.real());
        base[i | halfSize] = _mm_set_pd(s.imag(), s.real());
    }
    for (u32 i = twlen; i != (n >> 1); ++i)
        tw[i] = cmul(base[i & (halfSize-1)], base[halfSize | (i >> halfLog)]);
    twlen = n >> 1;
}
```

### 4.2 实测语义（已 dump 验证，n=64）

```
tw[ 0] = +1.000000 +0.000000i    arg/π = 0.0000
tw[ 1] = +0.000000 +1.000000i    arg/π = 0.5000      ← 纯 i
tw[ 2] = +0.707107 +0.707107i    arg/π = 0.2500
tw[ 3] = -0.707107 +0.707107i    arg/π = 0.7500
tw[ 4] = +0.923880 +0.382683i    arg/π = 0.1250
tw[ 5] = -0.382683 +0.923880i    arg/π = 0.6250
tw[ 6] = +0.382683 +0.923880i    arg/π = 0.3750
tw[ 7] = -0.923880 +0.382683i    arg/π = 0.8750
tw[ 8..15] arg/π × 16 = 1, 9, 5, 13, 3, 11, 7, 15
```

闭式：
```
tw[2^m + j] = exp( i·π·(2·bitrev_m(j) + 1) / 2^(m+1) )        j ∈ [0, 2^m)
tw[0] = 1
```
（`bitrev_m` = m 位的位反转。注意符号为 **正**指数；DIT 侧统一用 `conj`。）

### 4.3 ★ 两条恒等式（务必利用，可大幅减少 twiddle load）

```
(I)   tw[2i + 1] = tw[2i] · i          （纯 90° 旋转）
(II)  tw[2i]     = sqrt(tw[i])         （角度减半）
```

**(I) 的实现是免费的**：复数 `z = [re, im]` 乘 `i` 得 `[-im, re]`，
即 `_mm256_permute_pd(z, 0x5)` 后对低位（re 通道）取反 → `permute + xor` 共 **2 uops**，
且可在**循环外**完成（twiddle 在整个内循环里是常量）。

因此当前 `bf2Fwd(s, q, tw[bb], tw[2bb], tw[2bb+1])` 中的
**`w2 = tw[2bb+1]` 无需 load**，直接由 `w1` 旋转得到。

**对 radix-16 尤其关键**：一个 radix-16 kernel 概念上需要 15 个 twiddle
（= 4 层展开：1 + 2 + 4 + 8），但按恒等式 (I)，**奇数下标全部免费**：

| 层 | 需要的 twiddle | 需 load | 由 ·i 导出 |
|---|---|---|---|
| 1 | `tw[bb]` | 1 | 0 |
| 2 | `tw[2bb]`, `tw[2bb+1]` | 1 | 1 |
| 3 | `tw[4bb+k]`, k=0..3 | 2 (`4bb`, `4bb+2`) | 2 |
| 4 | `tw[8bb+k]`, k=0..7 | 4 (偶数下标) | 4 |
| **合计** | **15** | **8** | **7** |

> radix-8（3 层）同理：需要 7 个 twiddle，其中 **4 个 load + 3 个免费**。

### 4.4 已有的零 twiddle 特化

`bb == 0` 时 `w0 = w1 = 1`，代码已有 `bf2FwdOne` / `bf2InvOne` 特化（只剩 `w2 = tw[1] = i`）。
**radix-8/16 版本也应提供对应的 `bb == 0` 特化**，因为递归左链上 `bb` 恒为 0，
这条路径被调用的次数与其他路径同量级。

另注意：`tw[1] = i` 精确等于 `i`，乘它是 **permute+xor，无需任何乘法**。
当前 `bf2FwdOne` 仍在用 `cmul4(a3, W2, W2s)` 做完整复乘 —— **这里本身就有一处可省的浮点乘法**，
高基数版本请顺手修掉。

---

## 5. 交付目标

### 5.1 必交（T1-A）：radix-8 融合蝶形

```cpp
// 融合 3 层：跨度 4q（w 层1）、2q（层2）、q（层3）
// 语义必须严格等价于依次调用：
//     bf2Fwd(s, 2q, tw[bb], tw[2bb], tw[2bb+1]);     // 层1+2   ← 注意此处第二参数
//     然后对 4 个 quarter 各做一层 radix-2 bfFwd
// 精确等价定义见 §5.3 的参照实现
static inline void bf8Fwd(cpx* s, u32 q, u32 bb);
static inline void bf8Inv(cpx* s, u32 q, u32 bb);
static inline void bf8FwdOne(cpx* s, u32 q);   // bb == 0 特化
static inline void bf8InvOne(cpx* s, u32 q);
```

同时改写 `difRec` / `ditRec` 为 8 路递归（`n>>3`，`bb<<3`）。

### 5.2 可选加分（T1-B）：radix-16 融合蝶形

同上，`bf16Fwd/Inv`，`difRec` 改 16 路递归。
**仅当能证明不发生寄存器 spill（`-fverbose-asm` 或直接看 objdump 无 `%rsp` 相关的 vmovupd 溢出）时才提交。**

### 5.3 语义参照实现（黄金标准，可直接跑）

最稳妥的做法：**先写一个朴素标量版**，用它生成参照输出，再优化到 SIMD。

radix-8 DIF kernel 在数学上等价于以下"先 radix-4 再 radix-2"的组合。
`s` 指向长度 `8q` 的连续段：

```cpp
// 朴素参照：绝对不要提交这个，只用来对拍
static void bf8Fwd_ref(cpx* s, u32 q, u32 bb) {
    const u32 Q = q << 1;                       // radix-4 阶段的半跨度
    // 阶段 1：一个覆盖 [0, 8q) 的 radix-4（跨度 4q / 2q）
    bf2Fwd(s, Q, tw[bb], tw[bb<<1], tw[(bb<<1)|1]);
    // 阶段 2：对 4 个 quarter 各做一层 radix-2（跨度 q）
    for (u32 k = 0; k < 4; ++k) {
        u32 sub = (bb << 2) | k;
        if (sub == 0) bfPlain(s + k*Q, q);                 // w == 1
        else          bfFwd  (s + k*Q, q, tw[sub]);
    }
}
```
（`bfPlain` / `bfFwd` / `bfInv` 的定义见 `_fft_core.txt` 第 150~200 行。）

> ⚠️ 上面 `bb` 的推进规则必须与 `difRec` 的 `b4 = bb << 2` 保持一致。
> radix-8 递归时应为 `b8 = bb << 3`，子块 `bb*8 + k`（k = 0..7）。
> **请务必先用 n = 64 / 256 的小规模做穷举对拍，确认 index bookkeeping 正确后再上大规模。**

---

## 6. 硬约束

1. **禁止 `#pragma GCC optimize(...)`** —— 目标评测机会编译错误。
   `#pragma GCC target("avx2,fma,bmi,bmi2,popcnt,lzcnt")` 是允许的，已在用。
2. **禁止 AVX-512**（`vpermq zmm`、`_mm512_*`、mask 寄存器等一律不可）。
   开发机有 AVX-512VL，但目标机可能没有；且 valgrind 也不支持部分指令。
3. 编译命令固定为：`g++ -O2 -std=c++17 file.cpp`（**不加 `-march=native`**，
   靠源码里的 `#pragma GCC target`）。
4. **单文件、无外部依赖**（不得引入 FFTW/GMP/Eigen 等）。
5. 数值必须是 **double**。已验证 float / FMA 重结合会破坏大整数进位的精度余量。
6. 输出必须与现有实现 **逐字节一致**（不是"数值接近"）。

---

## 7. 验证协议（必须全部通过）

### 7.1 正确性（**必须先于 7.2 性能测量**，见陷阱 9）

```bash
# 1) 小规模穷举对拍（自己写，n = 16/64/256，与 bf8Fwd_ref 逐 bit 比）
# 2) 400 轮随机 + 边界压力测试 —— 至少 3 个不同 seed，这是主闸门
for S in 20260810 777 424242; do python3 stress_mul.py ./new 400 $S | tail -1; done
#    每行都必须是 "STRESS DONE: rounds=400 bad=0 ALL OK"
# 3) 端到端逐字节一致（22 个官方点全跑，不要只跑 max_max_01）
for f in data/mul/*.in; do cmp <(./new < $f) <(./base < $f) || echo "DIFF $f"; done
```

> ⚠️ 步骤 2 与 3 **不可互相替代**。官方点只覆盖大规模，压测才覆盖小/中 FFT 分支。
> 曾有版本通过全部 22 个官方点、cachegrind + 墙钟双证 −3.52%，随后压测 `bad=3 FAILED`。

### 7.2 性能（**两个判据都必须过**，缺一不可）

**判据 1 — cachegrind（Zen3 几何，权威、零噪声）**
```bash
valgrind --tool=cachegrind --cache-sim=yes \
  --I1=32768,8,64 --D1=32768,8,64 --LL=524288,8,64 \
  --cachegrind-out-file=/dev/null ./new < data/mul/max_max_01.in > /dev/null
```
要求：**`D1 misses` 相对基线 3,487,903 明显下降**（radix-8 目标 < 2.9M，radix-16 目标 < 2.2M）。
`I refs` 允许上升（已证明 Ir 不是判据）。

**判据 2 — 抗热漂移墙钟**
```bash
python3 loopbench2.py data/mul/max_max_01.in 3 21 ./base ./new
```
要求：`paired ratio` 的 **median < 1.0 且 IQR 不跨 1.0**，符号检验 **p < 0.05**，
且报告 `[OK] 无显著热漂移`。

> 单独的 `time` / 单次计时 **一律不接受**。该平台墙钟分辨率地板约 1.5%，
> 且存在频率爬坡效应，必须用配对比值 + 符号检验。

---

## 8. 已知陷阱（前人踩过，别再踩）

| # | 陷阱 |
|---|---|
| 1 | **`Ir` 下降不等于变快。** 已有反例：LEAF 7 比 LEAF 11 多 1.39% 指令，却快 2.33%。判据看 D1 miss。 |
| 2 | **微基准会骗人。** 孤立测 FFT 时 LEAF=1（全递归 radix-4）最快，但放进真实程序反而慢 9.1%（递归调用 4^9 次）。**一切以端到端为准。** |
| 3 | **`rusage` 的 user/sys 时间不可信。** 同一二进制两次跑给出 6.4/11.5ms 和 16.2/5.4ms 的相反结果。要用 `perf stat -e cycles:u,cycles:k`。 |
| 4 | 递归左链 `bb == 0` 的路径占比很高，**忘记写特化会吃掉大部分收益**。 |
| 5 | `-march=native` 会生成 valgrind 跑不了的指令（"非法指令"崩溃）。用 `-O2` + 源码内 `#pragma GCC target`。 |
| 6 | 数组是 `alignas(2MiB)` 的静态 BSS，但蝶形访问跨度大，**用 `loadu/storeu`**（当前实现即如此），不要假设 32B 对齐。 |
| 7 | radix-16 若发生寄存器 spill，收益会被完全吃掉甚至变负。**必须 objdump 检查。** |
| 8 | ★ **两级 twiddle 表的高位分量不能无条件提到循环外**（真实事故，见下方 8.1）。基线现已是两级表，`tw[i]` 不再是一次 load，而是 `cmul(base[i&mask], base[halfSize\|(i>>halfLog)])`。 |
| 9 | **先跑压测，再测性能。** 官方测试点规模分布极窄，"逐字节一致"完全覆盖不到小/中 FFT 分支。陷阱 8 的 bug 就是在官方点全过 + 双证 −3.52% 之后才被随机压测揪出来的。 |

### 8.1 陷阱 8 详解（radix-8/16 会加剧此问题，务必读）

基线 twiddle 已从 4 MiB 平表改为 16 KiB 两级表：

```cpp
static inline cpx twg(u32 i) {                       // 完整查表: 2 load + 1 cmul
    return cmul(twbase[i & twHalfMask], twbase[twHalfSize | (i >> twHalfLog)]);
}
static inline cpx twlo(u32 i, cpx hi) { return cmul(twbase[i & twHalfMask], hi); }
static inline cpx twhi(u32 i)         { return twbase[twHalfSize | (i >> twHalfLog)]; }
// twHalfLog  = (31 - clz(n)) >> 1 ;  twHalfSize = 1 << twHalfLog
```

自然的优化是把高位分量 `twhi(base)` 提到 `j` 循环外（省一半 load）。
**但这只在整个索引块不跨 `halfSize` 边界时成立**：

| 用途 | 索引 | 跨度 | 成立条件 |
|---|---|---|---|
| `hiA` | `base + j`, j<bc | `bc` | `bc ≤ halfSize` |
| `hiB` | `base2 + 2j`, j<bc | `2·bc` | `2·bc ≤ halfSize` |

`halfSize` **随 FFT 规模缩小**：n=2^20 → 1024（安全），n=2^11 → 32（越界）。
⇒ **只有小 FFT 会取错 twiddle**，大输入完全正常。这正是它躲过官方点验证的原因。

正确写法（分支在**层级**，不在蝶形级，开销可忽略）：

```cpp
if ((bc << 1) <= twHalfSize) {                 // 安全: 外提
    const cpx hiA = twhi(base), hiB = twhi(base2);
    for (...) { const cpx w1 = twlo(base2 + 2*j, hiB);
                bf2Fwd(s, q, twlo(base + j, hiA), w1, mulI(w1)); }
} else {                                       // 越界: 退回完整查表
    for (...) { const cpx w1 = twg(base2 + 2*j);
                bf2Fwd(s, q, twg(base + j), w1, mulI(w1)); }
}
```

> **radix-8/16 的跨度更大**（radix-16 一个 kernel 触及 `8bb..8bb+7`，跨度 `8·bc`），
> 所以外提条件要相应收紧到 `8·bc ≤ halfSize`。请在设计里显式给出你的跨度与保护条件。
>
> **不要试图直接抬高 `halfLog` 来绕过。** 建表用 `a0 = π/halfSize, a1 = a0/halfSize`，
> 角度分辨率被绑死在 `halfSize²`；改 `halfLog` 会改变 `twg(i)` 的语义（结果全错）。

---

## 9. 附件

- `_fft_core.txt` —— 完整 `namespace fft`（第 108~420 行原文），含 `bfPlain/bfFwd/bfInv`、
  `bf2FwdOne/bf2InvOne`、`difFlat/ditFlat`、`difRec/ditRec`、`difZeroHiTop/difRecZeroHi`、
  `pointwise/pointwiseSq`。
- 完整源文件 `v9.cpp`（约 700 行）可另行索取；本任务只需改动 `namespace fft` 内部。

---

## 10. 提出者已排除的方向（**不要重复投入**）

| 方向 | 结论 |
|---|---|
| split-radix / tangent FFT（减浮点乘法） | FP 端口仅 39% 占用，减乘法无收益 |
| SSA / NTT（数论变换） | 同项目十进制版实测慢 4.3~14.6× |
| TFT（截断 FFT） | 同项目实测无净收益 |
| DCT / 矩阵乘法路线 | 理论证伪 |
| 改用 FFTW3 | 我方现有 radix-4 核已比 FFTW3 **快 1.64×**（3.31ms vs 5.41ms @ N=2^19） |
| SoA（拆分 re/im 数组）消 shuffle | 归一化后 FP uop 数不变，p5 压力转移到 p0/p1，净收益不明；优先级低于本任务 |

---

## 11. 期望产出格式

1. 一个 patch 或完整的替换版 `namespace fft`；
2. 附上你自己跑出的 §7.1 / §7.2 结果原文；
3. 简述寄存器分配方案与调度理由（尤其 radix-16 如何避免 spill）；
4. 如果只做到 radix-8，请说明 radix-16 的具体阻碍在哪一步。
