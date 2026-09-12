# O3 预展开代码路径清单

> **生成时间**: 2026-07-28
> **背景**: LC 用 O2 编译, `#pragma GCC optimize("O3")` 提交无效 (增益 <1ms 噪声级), 手动预展开是唯一获得 O3 级优化的路径
> **基准代码**: add.cpp (修复 str32to8limbs bug 后, LC AC 24ms) / mul.cpp / div.cpp

---

## 当前性能 (LC O2 编译, 2026-07-28)

| 题目 | 当前 | BEST | 差距 | 主要瓶颈 |
|------|------|------|------|----------|
| ADD  | 24ms | 18ms | 6ms  | PARSE 51.7%, WRITE 29.4% |
| MUL  | 38ms | 36ms | 2ms  | FFT 蝶形 + carry chain |
| DIV  | 90ms | -    | -    | FFT + absDivBasicCore |

---

## divBASE 状态 (已最优, 无需改动)

```cpp
// add.cpp:1295-1296 (mul.cpp/div.cpp 同)
static constexpr uint64_t BARRETT_M = 0x68DB8BAC710CCULL; // ceil(2^64/10000)
static uint64_t divBASE(uint64_t s) { return (uint64_t)((unsigned __int128)s * BARRETT_M >> 64); }
```
一条 `mul` 指令完成, 已是最优。carry chain 中 divBASE 不是瓶颈, 串行依赖链本身才是。

---

## 预展开路径 (按优先级排序)

### P1: WRITE 阶段 — ADD 最大优化空间 (29.4%)

**位置**:
- add.cpp: `writeTo` @ 1508, `toString` @ 1435
- mul.cpp: `toString` @ 1864
- div.cpp: `toString` @ 1639

**当前实现**:
- 40KB `outTable` 查表 (占 L1 83%, cache 压力源) + 标量逐 limb load + 输出
- Phase 41 的 AVX2 gather 方案已回退 (gather 指令开销过大, large_small -36%)

**预展开建议**:
1. **循环展开 4/8 路**: 一次处理 4/8 个 limb, 减少 50-75% 循环开销
2. **AVX2 纯算术替代 40KB 表** (之前 topic 确认可行, ~28 指令, 无 40KB 表):
   - limb → 4 位 ASCII 可用 `vpmulld` + `vpsrlvd` + `vpaddb` 纯算术实现
   - 消除 40KB 表的 L1 cache 占用, 释放给 FFT 数据
3. **拆表方案**: 2×400B 小表 (0.8% L1) + 算术组合

**预期收益**: ADD 3-5ms (WRITE 占 29.4%, 若减半则省 ~4ms)

**风险**: AVX2 纯算术方案需要仔细验证 limb 值域 (0-9999) 和字节序

---

### P2: fftMul carry chain — MUL/DIV 主热点

**位置**:
- add.cpp: `fftMul` @ 2180, `fftSqr` @ 2264 (carry chain @ 2210-2243)
- mul.cpp: `fftMul` @ 2513, `fftSqr` @ 2597 (carry chain 类似)
- div.cpp: `fftMul` @ 2257, `fftSqr` @ 2341 (carry chain 类似)
- 另有 `fftMulPre` / `fftMulModBm1Pre` 在三文件中各有一份 (mul.cpp:2688/2851/2926/3039, div.cpp:2432/2531/2606/2719)

**当前实现**:
- 标量 8 路展开 (从 AVX-512DQ `cvttpd_epi64` 回退)
- `divBASE` 已用 Barrett (一条 mul 指令)
- prefetch 2 批次 ahead (128 字节)

**预展开建议**:
1. **展开到 16 路**: 减少 50% 循环开销 (cmp/jmp 指令)
2. **carry 是串行依赖**, 展开不能打破依赖链, 只减循环开销
3. **prefetch 距离调整**: 16 路迭代消耗 ~48 cycles, prefetch 距离可从 +16/+24 调整为 +32/+48
4. **double→int64 转换**: 当前用 `uint64_t(v1[i] + 0.5)` 标量, AVX-512DQ `cvttpd_epi64` 被回退; 纯 AVX2 替代方案 (split-double) 复杂度高, 用户已决定放弃

**预期收益**: MUL 1-2ms, DIV 2-3ms (carry chain 占 FFT 后处理主要时间)

**关键约束**: carry 串行依赖是物理瓶颈, 展开收益有上限

---

### P3: PARSE 阶段 — ADD 51.7% (但已 SIMD, 展开收益小)

**位置**:
- add.cpp: `str32to8limbs` @ 1016, `fromString` 循环 @ 1734-1748
- mul.cpp: `fromString` @ 1734 (原版标量, 无 str32to8limbs)
- div.cpp: `fromString` @ 1509 (原版标量, 无 str32to8limbs)

**当前实现**:
- add.cpp: AVX2 32字节/次 (`str32to8limbs`), 循环展开 2 次 (64字节/迭代)
- mul.cpp/div.cpp: 原版标量解析 (无 SIMD)

**预展开建议**:
1. **add.cpp**: 展开到 4 次 (128字节/迭代), 减少 50% 循环开销
2. **mul.cpp/div.cpp**: 移植 `str32to8limbs` (纯 AVX2, 无 AVX-512 依赖, 已验证正确)
   - 这是 mul.cpp/div.cpp PARSE 阶段的最大提升点
   - 移植时注意 `fromString` 的字节序处理 (高位在前, 低位在后)

**预期收益**:
- add.cpp: 1-2ms (已 SIMD, 展开收益小)
- mul.cpp/div.cpp: 3-5ms (从标量到 SIMD 的质变)

**注意**: add.cpp 的 `str32to8limbs` 刚修复了 `permute2x128` 的 bug, 移植时用修复后的版本 (extracti128 + unpacklo_epi64)

---

### P4: absAdd_avx2 — ADD 18.5% (收益有限)

**位置**: add.cpp:1832

**当前实现**: 16-limb 主循环 (AVX2 `_mm256_add_epi32` + 标量 carry 传播)

**预展开建议**: 展开 2 次 (32 limb/迭代)

**预期收益**: <1ms (循环开销占比小, carry 传播是瓶颈)

---

### P5: absDivBasicCore — DIV

**位置**: div.cpp:2906

**当前实现**: Newton 迭代 + 预计算 DFT, 内部调用 `absMul1` (SIMD) / `absSub_avx2` (直接 store)

**预展开建议**: 内部循环展开, 但 `absMul1`/`absSub` 已 SIMD, 收益有限

**预期收益**: <1ms

---

### P6: FFT 蝶形 — hint 库 (高风险)

**位置**: `transform::fft::real_conv` (hint 库内, radix-4 auto-vec FFT)

**当前实现**: radix-4 自动向量化 FFT, AlignedVec32 twiddle 表 + `__builtin_assume_aligned`

**预展开建议**: 蝶形循环展开 (radix-8 或 radix-16)

**风险**: 属于库代码, 修改风险高, 且 auto-vec 在 O2 下已自动展开一定程度

**预期收益**: 不确定, 需 profiling 确认 FFT 蝶形占比

---

## 预展开技术约束

1. **LC 编译参数**: `g++ -O2 -std=c++23 -DEVAL -DONLINE_JUDGE -march=native` (不能改)
2. **`#pragma GCC optimize("O3")` 无效**: LC 实测增益 <1ms (噪声级)
3. **`#pragma GCC target`**: `-march=native` 已自动启用 AVX2/FMA/BMI2, 无需手动 target
4. **AVX-512 不可用**: LC 评测机 Zen 3 虽支持 AVX-512 指令集, 但运行时可能受限 (之前 CE), 统一用 AVX2
5. **预展开产出**: CPP 文件 (不是 EXE)
6. **5 秒超时**: 程序必须包含 5 秒超时机制
7. **BASE=10^4**: 无需 BASE 转换

---

## 推荐预展开顺序

1. **mul.cpp/div.cpp 移植 str32to8limbs** (P3, 收益最高风险最低, 3-5ms)
2. **WRITE 阶段 AVX2 纯算术** (P1, ADD 最大空间, 3-5ms)
3. **fftMul carry chain 16 路展开** (P2, MUL/DIV, 1-3ms)
4. **add.cpp PARSE 展开 4 次** (P3, 1-2ms)
5. **absAdd_avx2 展开 2 次** (P4, <1ms, 可选)

---

## 文件结构

```
d:\precious_speed\
├── add.cpp          # 修复后 LC AC 24ms
├── mul.cpp          # LC 38ms
├── div.cpp          # LC 90ms
├── best/
│   ├── add.cpp      # 18ms (BEST)
│   ├── mul.cpp      # 36ms (BEST)
│   └── div.cpp      # 108ms (旧 BEST, 已被当前 90ms 超越)
├── support_product.cpp  # 超优化参考实现 (PRODUCT, 已应用 #1/#2/#5, #4 被 AVX-512 回退)
└── docs/
    ├── limits.md         # LC 编译环境/指令集
    └── o3_preexpansion_paths.md  # 本文件
```
