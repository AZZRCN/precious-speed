# moptm_fusion.cpp 9 项优化细节

> **用途**: 供 GCC 改造 AI 参考，理解 moptm_fusion.cpp 相比 archieve/cpp/moptm.cpp 的 9 项新增优化。
> **源文件**: `d:\precious_speed\moptm_fusion.cpp` (~142KB)
> **基线**: archieve/cpp/moptm.cpp (纯 O2)
> **编译**: `g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_{ADD|MUL|DIV}`
> **生成时间**: 2026-07-18

---

## 概述

moptm_fusion.cpp 在 archieve 版 moptm 基础上新增 9 项优化（无 `#pragma GCC optimize("O3")`，纯 O2 也可用）。

| # | 优化名 | 位置 | 影响题目 | 状态 |
|---|--------|------|----------|------|
| 1 | 64KB 查表法 parse (ParseTable) | L1024-1040 | ADD/MUL/DIV | 已验证 |
| 2 | 16 字节 SSE2 SIMD parse (str16to4limbs) | L1050-1075 | ADD/MUL/DIV | 已验证 |
| 3 | Barrett reduction (divBASE) | L1212-1222 | MUL/DIV | 已验证 |
| 4 | absAdd/absSub AVX2 双肢打包 | L1601-1730 | ADD | 已验证 |
| 5 | writeTo 表移 + SSE2 4× 展开 | L1180-1198, L1434-1498 | ADD/MUL/DIV | 已验证 |
| 6 | DIV blocks≥2 fast path | L3117-3146 | DIV | 已验证 |
| 7 | absDivMu (GMP mu_div_qr 风格) | L2832-2963 | DIV | 已验证 |
| 8 | fftMulModBm1 (cyclic FFT mod B^m-1) | L2267-2375 | DIV | **未通过正确性测试** |
| 9 | fftMulModBm1Pre (预计算 DFT 版本) | L2377-2468 | DIV | **未通过正确性测试** |

**性能演进（HANDBOOK 记录，VM g++ 15.2 -O2，ms）**:

| 版本 | ADD 1M | MUL 500k | DIV 1M/500k |
|------|:-:|:-:|:-:|
| moptm_O2 (基线) | 6.5-6.9 | 12.0 | 29.4 |
| +优化1 (64KB 查表) | 6.18 | 11.87 | 28.75 |
| +优化2 (SIMD parse) | 5.99 | 11.42 | 29.87 |
| +优化3 (Barrett) | 5.99 | 11.42 | 29.87 |
| +优化4 (AVX2 absAdd) | 6.05 | 12.55 | 30.32 |
| +优化5 (writeTo SSE2) | 5.69 | 11.47 | 28.33 |
| +优化6 (DIV fast path) | 5.69 | 11.47 | 28.33 |
| +优化7 (absDivMu) | 5.69 | 11.47 | 31.97* |
| moptm_fusion_O2 (最终) | 4.139 | 10.845 | 30.870 |

*absDivMu 单独测试 31.97ms，但 moptm_fusion_O2 整体 DIV 30.870ms（含 fast path 选择）。

---

## 优化 1: 64KB 查表法 parse (ParseTable)

**位置**: L1024-1040

**目的**: 用 64KB 查表替代 2 字节 ASCII→数值的算术计算，加速 str4toi。

**实现要点**:
- `struct ParseTable` 内含 `uint8_t table[0x10000]` (64KB)
- `constexpr` 构造函数在编译期填充：`table[i<<8 | j] = (i&15)*10 + (j&15)`，仅对 '0'-'9' (48-57) 范围有效
- `str4toi(s)` = `parseTable(s)*100 + parseTable(s+2)`，两次查表替代 4 次乘加

**代码**:
```cpp
struct ParseTable {
    uint8_t table[0x10000];
    constexpr ParseTable() : table() {
        for (uint32_t i = 48; i < 58; ++i)
            for (uint32_t j = 48; j < 58; ++j)
                table[i << 8 | j] = uint8_t((i & 15) * 10 + (j & 15));
    }
    inline uint32_t operator()(const char* s) const {
        return table[uint32_t(uint8_t(s[0])) << 8 | uint32_t(uint8_t(s[1]))];
    }
};
static constexpr ParseTable parseTable{};

inline uint16_t str4toi(const char *s) {
    return uint16_t(parseTable(s) * 100 + parseTable(s + 2));
}
```

**性能影响**: ADD 1M 6.5→6.18ms (-5%)，三题通用。

---

## 优化 2: 16 字节 SSE2 SIMD parse (str16to4limbs)

**位置**: L1050-1075

**目的**: 一次解析 16 字节 ASCII 数字 → 4 个 uint16 limbs，加速 fromCharRange 主循环。

**实现要点**:
- 纯 SSE2（x86-64 baseline，无 SSSE3 依赖，LC 默认可用）
- 步骤：`_mm_loadu_si128` 加载 16 字节 → 减 '0' → `unpacklo/hi_epi8` 零扩展到 16 位 → `mullo_epi16` 按 [10,1] 缩放 → `madd_epi16(ones)` 合并相邻字节对 → `packs_epi32` 打包 → `madd_epi16(mul4)` 合并 4 位数 → `shuffle_epi32` 反转 dword 顺序 → `packs_epi32` 打包到 4 个 word → `_mm_storel_epi64` 存储
- 关键修正：`mul4 = _mm_set1_epi32(0x00010064)` (low word=100, high word=1)，原任务原稿布局有 bug
- 倒序解析：bytes[0..3]→limb[3]（最高位），bytes[12..15]→limb[0]（最低位）

**调用点**: fromCharRange L1524-1528 主循环

**性能影响**: ADD 1M 6.18→5.99ms (-3%)，MUL 500k 11.87→11.42ms (-4%)。

---

## 优化 3: Barrett reduction (divBASE)

**位置**: L1212-1222

**目的**: 用乘法 + 移位替代除法，加速 FFT 进位传播中的 `/BASE` 和 `%BASE`。

**实现要点**:
- `BARRETT_M = 0x68DB8BAC710CC` = `ceil(2^64 / 10000)`
- `divBASE(s) = (uint64_t)((unsigned __int128)s * BARRETT_M >> 64)` == `s / 10000`
- S=64 意味着 q = 128 位积的高 64 位，无需移位，只需一条 `mul` 指令
- GCC 默认用 S=75 (`mulq` + `shrq $11`)，S=64 省一次 `shrq`
- 正确性范围：`s <= 2200231879019999` (~2^51)，FFT 卷积和 `N*(BASE-1)^2 ~ N*10^8`，N<11263 limbs 时 s_max ~1.1e12，安全余量 2000×

**代码**:
```cpp
static constexpr uint64_t BARRETT_M = 0x68DB8BAC710CCULL;
static uint64_t divBASE(uint64_t s) {
    return (uint64_t)((unsigned __int128)s * BARRETT_M >> 64);
}
```

**调用点**: fftMulPre/fftMulModBm1 进位传播（L1966, L2030, L2224, L2310 等）

**性能影响**: 汇编层面消除 45 条 `shrq`，但性能无 measurable 变化（瓶颈是 `mul` 延迟，非 `shrq`）。保留是因为理论上更快。

---

## 优化 4: absAdd/absSub AVX2 双肢打包

**位置**: L1601-1730 (scoped pragma `#pragma GCC target("avx2,fma")`)

**目的**: 将两个 uint16 limb 打包成一个 uint32，用 AVX2 一次处理 16 个 limb，串行进位链减半。

**实现要点**:
- `absAdd_avx2`: 16 limb 主循环（8 个 uint32 = 1 个 `__m256i`），`_mm256_add_epi32` 一次加 8 个打包对
- 串行 carry 传播：8 步，每步处理 2 个 limb (lo/hi)，无分支掩码（`lo >= 10000` → `clo` → `lo -= clo*10000u`）
- 不变量：limb 最大 9999，两 limb 和最大 19998 < 65536，不会进位到高 16 位
- `absSub_avx2`: 用 `bias = BASE|BASE<<16` 加到 a 后再减 b，保证每个 16-bit lane 不下溢
- 标量尾部 8 路展开 + `add_half`/`sub_half` 无分支掩码
- 第三段：carry=0 时用 `memcpy` 快速拷贝剩余部分

**关键: scoped pragma**:
```cpp
#pragma GCC push_options
#pragma GCC target("avx2,fma")
static bool absAdd_avx2(...) { ... }
static bool absSub_avx2(...) { ... }
#pragma GCC pop_options
```
**必须用 scoped pragma**，不能用全局 `#pragma GCC target("avx2,fma")`，否则 FFT 代码生成受影响导致 MUL/DIV 回退。

**调用点**: `absAdd`/`absSub` 在 `in2.size >= 16` 时分发到 AVX2 版本（L1733-1736, L1776-1779）

**性能影响**: ADD 1M 5.99→6.05ms（+1%，AVX2 开销在小数据抵消收益，但大数据有效），MUL/DIV 无变化。

---

## 优化 5: writeTo 表移 + SSE2 4× 展开

**位置**: L1180-1198 (OutTable 定义), L1434-1498 (writeTo 实现)

**目的**: 消除函数局部 `static` 的 magic static guard 运行时开销，并用 SSE2 一次输出 16 字节。

**实现要点**:

1. **OutTable 移到命名空间作用域**:
   - 原 moptm 在 `writeTo` 函数内用 `static const uint32_t* table = [](){...}();`，每次调用有 magic static guard（原子变量检查）
   - 改为命名空间作用域 `constexpr OutTable outTable{}`，编译期初始化，无运行时 guard

2. **SSE2 4× 展开**:
   - 主循环 `while (i >= 4)`：一次取 4 个 limb 的查表值，`_mm_set_epi32(v3,v2,v1,v0)` 打包，`_mm_storeu_si128` 一次写 16 字节
   - 尾部标量 `memcpy(p, &outTable.t[data[i]], 4)` 处理剩余

**代码**:
```cpp
namespace {
    struct OutTable {
        uint32_t t[10000];
        constexpr OutTable() : t() {
            for (int i = 0; i < 10000; i++) {
                t[i] = uint32_t(i / 1000 + '0') |
                       (uint32_t(i / 100 % 10 + '0') << 8) |
                       (uint32_t(i / 10 % 10 + '0') << 16) |
                       (uint32_t(i % 10 + '0') << 24);
            }
        }
    };
    constexpr OutTable outTable{};
}
// writeTo 内:
while (i >= 4) {
    uint32_t v0 = outTable.t[data[i - 1]];
    uint32_t v1 = outTable.t[data[i - 2]];
    uint32_t v2 = outTable.t[data[i - 3]];
    uint32_t v3 = outTable.t[data[i - 4]];
    __m128i v = _mm_set_epi32(v3, v2, v1, v0);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(p), v);
    p += 16;
    i -= 4;
}
```

**性能影响**: ADD 1M 6.05→5.69ms (-6%)，MUL 500k 12.55→11.47ms (-9%)，DIV 1M/500k 30.32→28.33ms (-7%)。**三题通用且收益最大**。

---

## 优化 6: DIV blocks≥2 fast path

**位置**: L3117-3146 (absDivRem 路径选择)

**目的**: 根据被除数/除数长度比例，选择最优除法路径。

**实现要点**:
- `len2 <= 64 || (len1 - len2) <= 64` → `absDivBasicCore`（基础长除法）
- `len1 < len2 * 2` → `absDivNewtonCore1`（单块牛顿除法）
- `len1 >= len2 * 2` → GMP mu_div_qr 风格：
  - 计算 `mu_in`（近似逆精度）
  - `mu_in < len2` → `absDivMu`（近似逆分块）
  - `mu_in >= len2` → `absDivNewtonCore2`（精确逆分块）

**mu_in 计算逻辑**:
```cpp
size_t qn_mu = len1 - len2;
size_t mu_in;
if (qn_mu > len2) {
    mu_in = (qn_mu - 1) / ((qn_mu - 1) / len2 + 1) + 1;
} else if (3 * qn_mu > len2) {
    mu_in = (qn_mu - 1) / 2 + 1;
} else {
    mu_in = qn_mu;
}
```

**性能影响**: 1M/500k 是 blocks=2 无余数最差情况，净收益仅 0.7ms。1M/100k 走 `absDivNewtonCore2`（mu_in=len2）。

---

## 优化 7: absDivMu (GMP mu_div_qr 风格)

**位置**: L2832-2963

**目的**: 用 `in` 位近似逆替代 `len2+1` 位精确逆，减小 absInvNewton 规模。

**算法**:
1. `absInvNewton(divisor + (len2 - in), inv_span)` 计算 divisor 高 `in` 位的精确逆（`in+1` 位）
2. 预计算 inv DFT (`inv_float_len = ceil2(2*in+1)`) 和 divisor DFT (`divisor_float_len = ceil2(len2+in)`)
3. 分块循环（从高位向低位）：
   - `qhat = (divid_high * inv) >> (in+1)`，取高 `this_in+1` 位
   - `prod = divisor * qhat`（fftMulPre 复用预计算 DFT）
   - 修正 1：`while (prod > window) { qhat--; prod -= divisor; }`
   - `window -= prod`
   - 修正 2：`while (window >= divisor) { qhat++; window -= divisor; }`

**B-1 优化复用**: 当 `in == len2` 时，divisor 切片 = 完整 divisor，DFT 可复用给 absInvNewton：
```cpp
if (in == len2) {
    prepareDFT(divisor, divisor_dft_buf.data(), divisor_float_len);
    absInvNewton(divisor, inv_span, divisor_dft_buf.data(), divisor_float_len);
} else {
    absInvNewton(divisor + (len2 - in), inv_span);
    prepareDFT(divisor, divisor_dft_buf.data(), divisor_float_len);
}
```

**OPT 细节**:
- `divisor_float_len` 从 `ceil2(len2+in+1)` 改为 `ceil2(len2+in)`（安全：conv_len <= in+len2）
- 删除冗余 `count_true_length`（`absCompare` 内部会重算）
- 循环外预 `resize` 到最大（`qhat_len_max = 2*in+2`, `prod_len_max = len2+in+1`）

**性能（VM g++ 15.2 -O2，7 次中位数）**:

| 测试 | absDivMu | moptm 基线 | delta |
|------|:-:|:-:|:-:|
| 1M/500k | 31.97ms | 37.90ms | **-15.6%** |
| 200k/100k | 6.88ms | 8.55ms | **-19.6%** |
| 1M/100k | 26.12ms | 25.99ms | +0.5% (走 Core2) |

**正确性**: 3 个用例 MD5 全部匹配 baseline ✓

---

## 优化 8: fftMulModBm1 (cyclic FFT mod B^m-1)

**位置**: L2267-2375

**目的**: 计算 `a * b mod (B^m - 1)`，结果长度 `m`（cyclic convolution），用于 absInvNewton GMP 风格两步分解，FFT 长度从 `ceil2(2m-1)` 降到 `m`。

**算法**:
1. `assert(is_2pow(m))`，`a_len <= m`，`b_len <= m`
2. thread_local 缓冲 `va`/`vb`（长度 `m`）
3. cyclic FFT：`fft.dif<true>(va, m)`, `fft.dif<true>(vb, m)`
4. `real_dot_binrev2(va, vb, m)` 点积
5. `fft.idit<true>(va, m)` 逆变换
6. 进位传播（8 路展开 + Barrett reduction）
7. **cyclic carry 折叠**：`B^m ≡ 1 (mod B^m-1)`，所以 `out_final = (out + carry) mod (B^m-1)`
   - `while (carry > 0)`：绕 out 一圈传播 carry
   - 极罕见情况（原 `sum >= 2*B^m`）：carry=1 时 out 全 0

**状态**: ⚠️ **未通过正确性测试**
- HINT_OP_TESTMOD 测试 main (L3594-3666) 中 `Integer a(buf_a)` 触发模板构造函数错误
- `Integer(const T&) [with T = char [2097152]]` 导致 `sign = input < 0` 指针比较
- 代码已写入但未验证，待修复测试 main

---

## 优化 9: fftMulModBm1Pre (预计算 DFT 版本)

**位置**: L2377-2468

**目的**: `fftMulModBm1` 的预计算版本，`b` 的 DFT 预先计算，避免重复 DFT。

**与 fftMulModBm1 差异**:
- 参数：`const double *b_dft`（预计算好的 b 的 DFT）+ `size_t b_len`
- 省略 `vb` 缓冲和 `fft.dif<true>(vb, m)`
- 直接 `real_dot_binrev2(v, b_dft, m)`
- 进位传播 + cyclic 折叠逻辑完全相同

**状态**: ⚠️ **未通过正确性测试**（同优化 8，测试 main 未修复）

---

## 辅助函数

### add_half / sub_half (无分支掩码)

**位置**: L1150-1164

```cpp
template <typename T>
T add_half(T a, T b, T base, T &cf) {
    T r = a + b;
    cf = r >= base;
    T mask = T(0) - T(cf);
    return r - (base & mask);
}
template <typename T>
T sub_half(T a, T b, T base, T &bf) {
    bf = a < b;
    T mask = T(0) - T(bf);
    return a - b + (base & mask);
}
```

用算术掩码替代 `cmov`/分支，消除分支预测失败开销。用于 absAdd/absSub 标量尾部 8 路展开。

---

## 编译方法

```bash
# ADD
g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_ADD moptm_fusion.cpp -o moptm_fusion_O2_ADD

# MUL
g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_MUL moptm_fusion.cpp -o moptm_fusion_O2_MUL

# DIV
g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV moptm_fusion.cpp -o moptm_fusion_O2_DIV

# O3 版本（命令行 -O3）
g++ -O3 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_{ADD|MUL|DIV} moptm_fusion.cpp -o moptm_fusion_O3_{ADD|MUL|DIV}
```

**注意**:
- 无 `#pragma GCC optimize("O3")`（LC 真实评测不接受，已确认）
- 无 `-mavx2`（LC 默认不启用，AVX2 代码靠 scoped `#pragma GCC target("avx2,fma")` 启用）
- 源码 ≤ 256KB（当前 ~142KB，符合）

---

## 文件结构（moptm_fusion.cpp 关键段落）

| 行号范围 | 内容 |
|----------|------|
| L1-100 | 文件头注释（优化清单、性能记录） |
| L1000-1022 | InputHelper (cin 批量读) |
| L1024-1075 | ParseTable + str16to4limbs (优化 1, 2) |
| L1076-1148 | itostr4, ViewTy, SpanTy |
| L1150-1164 | add_half / sub_half (无分支掩码) |
| L1180-1198 | OutTable (优化 5 表移) |
| L1201-1600 | Integer 类核心 |
| L1212-1222 | Barrett reduction divBASE (优化 3) |
| L1434-1498 | writeTo SSE2 4× (优化 5) |
| L1500-1600 | fromCharRange (调用 str16to4limbs) |
| L1601-1730 | absAdd_avx2 / absSub_avx2 (优化 4) |
| L1731-1900 | absAdd / absSub 标量版 + 运算符 |
| L1900-2265 | fftMul / fftSqr / fftMulPre |
| L2267-2468 | fftMulModBm1 / fftMulModBm1Pre (优化 8, 9) |
| L2500-2825 | absDivNewtonCore1/Core2, absDivRem |
| L2832-2963 | absDivMu (优化 7) |
| L3000-3158 | absDivRem 路径选择 (优化 6) |
| L3300-3590 | main (ADD/MUL/DIV 三模式) |
| L3594-3666 | HINT_OP_TESTMOD 测试 main (未通过) |

---

**文档结束**
