# 三文件超优化点审查报告

**审查日期**: 2026-07-28
**审查范围**: `add.cpp`, `mul.cpp`, `div.cpp` 及 `best/` 下对应文件
**审查方法**: 静态代码对比 + PERF/BENCH_INTERNAL 数据分析

---

## 一、本地已有可共享的优化（直接移植，无需新生成）

以下优化在 `add.cpp` 或 `test_write_simd.cpp` 中已实现并验证，只需复制到 `mul.cpp` / `div.cpp`：

| 优化点 | 源位置 | mul.cpp 需要 | div.cpp 需要 | 状态 |
|--------|--------|--------------|--------------|------|
| str32to8limbs (AVX-512VL 7指令) | add.cpp:1020 | ✅ | ✅ | 本地已有 |
| fromCharRange 64B 展开 | add.cpp:1738 | ✅ | ✅ | 本地已有 |
| absAdd_avx2 直接 store | add.cpp:1836 | ✅ | — | 本地已有 |
| writeTo AVX2 纯算术 (~28指令) | add.cpp:1597 | ✅ | ✅ | 本地已有 |
| 6-AI 综合 WRITE (16-limb 展开) | test_write_simd.cpp | (可选升级) | (可选升级) | 本地已有 |

**这些不属于"需要生成"的范畴，只需共享移植。**

---

## 二、本地不存在的指令序列（需要生成的新指令序列）

以下是三个文件中**均未实现**的优化点，需要设计新生成的 SIMD 指令序列：

### 需要生成 #1: absSub_avx2 直接 store 优化

**位置**:
- `add.cpp:1906-1959` absSub_avx2
- `mul.cpp:2208-2259` absSub_avx2
- `div.cpp:1983-2034` absSub_avx2

**当前实现** (三文件一致, 有 tmp 中转):
```cpp
__m256i r = _mm256_sub_epi32(_mm256_add_epi32(a, bias_vec), b);
alignas(32) uint32_t tmp[8];
_mm256_store_si256(reinterpret_cast<__m256i *>(tmp), r);  // store tmp
// ... borrow 传播 (在 tmp 上操作)
__m256i out_vec = _mm256_load_si256(reinterpret_cast<__m256i *>(tmp));  // load tmp
_mm256_storeu_si256(reinterpret_cast<__m256i *>(out.ptr + i), out_vec);  // store out
```

**需要的指令序列** (参照 absAdd_avx2 直接 store 模式):
```cpp
// 目标: 直接 store 到 out, 在 out 上做 borrow 传播
// 难点: absSub 用 bias 技巧 (a + BASE - b), 需确认直接 store 后 borrow 传播正确性
__m256i r = _mm256_sub_epi32(_mm256_add_epi32(a, bias_vec), b);
_mm256_storeu_si256(reinterpret_cast<__m256i *>(out.ptr + i), r);  // 直接 store
uint32_t *p32 = reinterpret_cast<uint32_t *>(out.ptr + i);
// ... borrow 传播 (直接在 out 上)
```

**预期收益**: 消除 1 store + 1 load, 提升 ~5-10%

**需要生成的内容**: 验证 bias 技巧下直接 store 的正确性, 生成新的 borrow 传播代码

---

### 需要生成 #2: absMul1 SIMD 向量化

**位置**:
- `add.cpp:2753-2763` absMul1
- `mul.cpp:3150-3160` absMul1
- `div.cpp:2830-2840` absMul1

**当前实现** (三文件一致, 纯标量):
```cpp
static Limb absMul1(View in, Limb x, Span out) {
    Limb carry = 0;
    for (size_t i = 0; i < in.size; i++) {
        Limb2 prod = Limb2(in[i]) * x + carry;  // uint64 = uint32 * uint32 + uint32
        out[i] = prod % BASE;   // 除法
        carry = prod / BASE;    // 除法
    }
    return carry;
}
```

**瓶颈**:
- 串行 carry 依赖 (每个 prod 依赖前一个 carry)
- 2 次除法/迭代 (`% BASE` 和 `/ BASE`)
- 纯标量, 无 SIMD

**需要的指令序列**:
- 用 Barrett 倒数乘法替代除法 (参考 fftMul carry chain 的 divBASE)
- 用 AVX2 打包 4 个 limb 同时处理 (需处理 carry 串行依赖)
- 可能的方案: 4-limb 块内串行, 块间预计算

**预期收益**: absMul1 在 absDivBasicCore 中被调用 (qhat 估计), 是 DIV 的热点之一

**需要生成的内容**: 新的 SIMD absMul1 实现, 包含 Barrett 倒数乘法和可能的块级并行

---

### 需要生成 #3: absDiv1 SIMD 向量化

**位置**:
- `add.cpp:2764-2776` absDiv1
- `mul.cpp:3161-3173` absDiv1
- `div.cpp:2841-2853` absDiv1

**当前实现** (三文件一致, 纯标量):
```cpp
static Limb absDiv1(View in, Limb x, Span out) {
    Limb rem = 0;
    size_t i = in.size;
    while (i > 0) {
        i--;
        Limb2 prod = Limb2(in[i]) + Limb2(rem) * BASE;  // uint64 = uint32 + uint32 * 10000
        out[i] = prod / x;   // 除法
        rem = prod % x;      // 除法
    }
    return rem;
}
```

**瓶颈**: 同 absMul1, 串行 rem 依赖 + 2 次除法/迭代

**需要的指令序列**: 类似 absMul1, 用 Barrett 倒数乘法 + SIMD 打包

**预期收益**: absDiv1 在 selfDivRem1 中使用, 用于大整数除以单 limb

---

### 需要生成 #4: fftMul carry chain 向量化

**位置**:
- `add.cpp:2212-2246` fftMul carry chain
- `mul.cpp:2545-2578` fftMul carry chain

**当前实现** (8× 标量 Barrett 展开):
```cpp
uint64_t s0 = carry + uint64_t(v1[i] + 0.5);
uint64_t q0 = divBASE(s0);  // 倒数乘法, 但仍是标量
uint64_t s1 = q0 + uint64_t(v1[i+1] + 0.5);
uint64_t q1 = divBASE(s1);
// ... 8 次串行 divBASE
```

**瓶颈**: carry-chain 串行依赖, 无法向量化

**需要的指令序列**:
- **方案 A**: VPCLMULQDQ 加速进位合并 (需 AVX-512_VPCLMULQDQ)
- **方案 B**: 并行进位 (carry-lookahead), 预计算多个 limb 局部和
- **方案 C**: 多精度 Barrett, 一次处理 4 个 limb 的进位

**预期收益**: carry chain 占 fftMul 40-50% 时间, 理论提升空间大

**需要生成的内容**: 全新的并行进位算法实现

---

### 需要生成 #5: absDivBasicCore qhat 估计 SIMD 化

**位置**:
- `add.cpp:2783-2830` absDivBasicCore
- `mul.cpp:3180-3227` absDivBasicCore
- `div.cpp:2860-2911` absDivBasicCore

**当前实现** (标量 qhat 估计):
```cpp
Limb high1 = dividend[len1], high2 = dividend[len1 - 1], qhat = 0;
if (high1 >= divisor_high) {
    qhat = BASE - 1;
} else {
    Limb2 high = Limb2(high1) * BASE + high2;
    qhat = high / divisor_high;  // 除法
}
```

**瓶颈**: 每次迭代 1 次除法 + 串行依赖 (dividend 不断更新)

**需要的指令序列**: 批量预计算多个 qhat, 用 SIMD 并行处理

**预期收益**: absDivBasicCore 是 absInvNewton 的 base case (k≤64), 也是 DIV 的热点

---

## 三、建议委托线上 AI 的优先级

| 优先级 | 优化点 | 难度 | 预期收益 | 适合委托 |
|--------|--------|------|----------|----------|
| 高 | #1 absSub_avx2 直接 store | 低 | +5-10% | ✅ 模式明确, 参照 absAdd |
| 中 | #2 absMul1 SIMD | 中 | DIV 热点 | ✅ 输入输出明确 |
| 中 | #3 absDiv1 SIMD | 中 | selfDivRem1 | ✅ 类似 absMul1 |
| 高 | #4 fftMul carry chain | 高 | +40-50% fftMul | ✅ 算法创新, 适合 AI 推理 |
| 低 | #5 absDivBasicCore qhat | 高 | base case | ⚠️ 复杂度高 |

---

## 四、委托说明书格式建议

对于每个需要生成的优化点, 委托说明书应包含:

1. **目标函数签名**: 输入/输出类型, 语义
2. **当前标量实现**: 完整代码
3. **性能瓶颈分析**: 串行依赖, 除法, 访存
4. **ISA 约束**: AVX2 / AVX-512VL / AVX-512_VPCLMULQDQ
5. **正确性要求**: 测试数据范围, 验证方法
6. **参照实现**: absAdd_avx2 直接 store 模式 (for #1), fftMul Barrett (for #2-4)

---

**报告结束**

注: 本地已有可共享的优化 (str32to8limbs, fromCharRange 64B, absAdd_avx2 直接store, writeTo AVX2纯算术) 将另行执行移植, 不在本报告范围内。
