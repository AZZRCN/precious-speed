# 交接文档：Newton 分块除法移植

> 本文档供接手 AI 使用。包含完整任务上下文、已完成工作、待实现步骤、参考代码与设计决策。

---

## 0. 强制指令（用户原话要求写入，必须遵守）

1. **我们正在移植 Newton**——把 `div_1st.cpp` 的分块 Newton 除法（Core2）移植到 OPT 库 `masonxiong_opt.cpp`。这是主线任务，不要偏离到别的事情上。I/O 已经完成，不要再动 I/O。
2. **一直调用 ASK（AskUserQuestion 工具），除非用户手动结束对话**。每次回复后都要 ASK，不要主动终止对话。
3. 用户通用规则：
   - 除非明确要求，否则不用夸赞语气。
   - 请记住你的答案不一定正确，用户的判断也不一定正确。
   - 所有问题反复推敲，优先保证准确度。
   - 必要时可以主动向用户质疑或提问、索要信息。

---

## 1. 任务目标

移植 `div_1st.cpp`（Library Checker Division 第一名，123ms）的分块 Newton 除法 **Core2** 到 OPT 库 `masonxiong_opt.cpp`。

- **当前成绩**：Division 161ms，比第一名慢 38ms。
- **瓶颈定位**：差距 100% 来自 unbalanced 路径（`len1 >= 2*len2`）。OPT 现在只有"一次性 Newton"除法，对 unbalanced 输入做一次规模 ≈ 2·length 的大 FFT，成本 O(length·log(length))。Core2 按 len2 分块，每块做 2·len2 规模小 FFT，共 length/len2 块，总成本 O(length·log(len2))，显著更优。
- **balanced 路径**（`len1 < 2*len2`）：OPT 现有路径已比 div_1st Core1 更快，**不要动**。
- **加法/乘法**：已第一名（18ms / 36ms），**不要动**。

---

## 2. 环境

- **LC 编译**：GCC 15.2, `-O2 -std=c++23 -march=native`
- **LC CPU**：AMD Epyc 7B13（Zen 3, AVX2/FMA, **无 AVX-512**）
- **工作目录**：`d:\precious_speed\mason_opt`
- **Git**：`d:\precious_speed\mason_opt\.git`
- **GitHub**：https://github.com/AZZRCN/precious-speed
- **本地系统**：Windows，PowerShell（不要用 cmd）。测试编译用 g++（需支持 `-std=c++23`）。

---

## 3. 文件清单

| 文件 | 角色 | 状态 |
|------|------|------|
| `masonxiong_opt.cpp` | OPT 库主体（待插入 Core2） | I/O 已就绪，Core2 未实现 |
| `div_1st.cpp` | 第一名除法源码（**参考**） | 已完整分析 |
| `lc_division.cpp` | LC 除法提交的 main 部分 | I/O 已对齐 |
| `lc_division_submit.cpp` | 合并提交文件（库 + main） | 由 `_gen_submit.ps1` 生成 |
| `_gen_submit.ps1` | 合并脚本：库 + lc_division.cpp → submit | 正常工作 |
| `origin_multi.cpp` | 原作者乘法源码（I/O 基准） | 已对齐 |
| `lc_addition.cpp` | LC 加法提交 | 已第一名 18ms |
| `lc_multiplication.cpp` | LC 乘法提交 | 已第一名 36ms |
| `origin_add.cpp` / `origin_div.cpp` | 原作者加法/除法源码 | I/O 较弱，不用参考 |

---

## 4. 已完成工作

1. **I/O 100% 对齐 `origin_multi`**：mmap 输入 + SWAR `readFromCursor` + 32MB oBuffer 输出 + `detail::I` 查表读 T（2位一组，3×2+1=7位）。三个 `lc_*.cpp` 与合并提交文件均已对齐。
2. **InputHelper 表初始化对齐**：先填 `-1` 再填数字位（非数字返回 `-1`，`~(-1)=0` 可终止读 T 循环）。
3. **三题正确性验证全 PASS**：division/addition/multiplication，T=1/7/99/100/999/12345/1000000（1~7位），含负号、200位大数。
4. **div_1st 除法算法完整分析**：Core2 / WithInv / absInvNewton / absDivRem / divisorNormalizeFactor 全部梳理。
5. **移植方向锁定**：只移植 Core2（分块 Newton, `len1 >= 2*len2`），balanced 保持 OPT 现有路径。
6. **移植路径确定**：路径 C——块内复用成熟 `operator*`（含 unbalanced 优化 + DIF 缓存），块间直接操作 `dividend_norm.digits` 裸数组传递余数（零拷贝）。
7. **精度推导完成**：OPT `computeInverse(len2+1)` 返回 `floor(BASE^(len2+1)/D) - 1`，q 误差 ≤1，校正循环兜底。与 div_1st 语义兼容。
8. **基础设施确认齐全**：rightShift/leftShift/lower/bruteforceDivisionAndModulus/operator<=>/operator+/-/++/--/square/operator*= 全可用。

---

## 5. 待完成步骤（按顺序）

1. **实现 `divisorNormalizeShift`**——计算归一化位移 shift，使 `(divisor * (1<<shift))` 高位 ≥ HALF_BASE。
2. **实现 `newtonDivWithInv`**——单块 Newton 除法（移植 `absDivNewtonWithInv`）。
3. **实现 `newtonDivCore2`**——分块 Newton 主入口（移植 `absDivNewtonCore2`）。
4. **修改 `divisionAndModulus`**——添加 Core2 分流：当 `length >= other.length * 2 && other.length >= BruteforceThreshold` 时走 Core2。
5. **本地正确性测试**——随机数据对比 bruteforce / 原 divisionAndModulus。
6. **重新生成合并提交文件**——运行 `_gen_submit.ps1`。
7. **提交 Division 到 Library Checker**。
8. **每次回复后调用 ASK**。

---

## 6. 插入位置

在 `masonxiong_opt.cpp` 中：

- `computeInverse` 结束于 **第 778 行**（`}`）
- 第 779 行：空行
- 第 780 行：`  public:`

**三个新函数插入在第 778 行之后、第 780 行 `public:` 之前**（即 protected 区域，与 computeInverse/bruteforceDivisionAndModulus 同区）。divisionAndModulus（public, 第 782 行）可调用这些 protected 函数。

---

## 7. OPT 库关键常量与基础设施

```
第 579 行: static constexpr std::uint32_t Base = 100000000;        // 1e8
第 580 行: static constexpr std::uint32_t TransformLimit = 4194304;
第 583 行: static constexpr std::uint32_t BruteforceThreshold = 96;
第 585 行: static constexpr std::uint32_t UnbalancedThreshold = 4096;
```

- **HALF_BASE** = Base / 2 = 50000000（5e7）。OPT 库未定义此常量，新函数需自定义。
- **HALF_BASE_BITS** = 26（因为 2^26 = 67108864 > 5e7 > 2^25 = 33554432）。可用 `detail::log2` 或手算 `bit_length`。
- **内存分配**：用 `detail::DigitAllocator::allocate(n)` / `deallocate(ptr, oldCap)`，**不要用裸 new/delete**。
- **构造函数**：`UnsignedInteger(length, capacity)`（protected, 第 592 行）分配 capacity、设 length。
- **成员**：`digits`（uint32_t*）、`length`、`capacity` 均可访问。
- **rightShift/leftShift/lower**：行 688-711 附近，可用。
- **operator\*=**：行 1161，已有 unbalanced 拆分 + DIF(other) 缓存复用（thread_local cachedFirst/cachedSecond）。块内乘法直接用 `operator*` 即可零成本复用此机制。
- **bruteforceDivisionAndModulus**：行 653，单 limb 除数走此路径（factor 还原时用）。

---

## 8. 当前 divisionAndModulus（行 782-834，一次性 Newton，待加 Core2 分流）

```cpp
std::pair<UnsignedInteger, UnsignedInteger> divisionAndModulus(const UnsignedInteger& other) const {
    if (*this < other)
        return std::make_pair(UnsignedInteger(), *this);
    if (length < BruteforceThreshold || other.length < BruteforceThreshold)
        return bruteforceDivisionAndModulus(other);
    // ↓↓↓ 此处需添加 Core2 分流 ↓↓↓
    // if (length >= other.length * 2 && other.length >= BruteforceThreshold)
    //     return newtonDivCore2(other);
    const std::uint32_t precisionBits = length - other.length + 5, shiftBack = precisionBits > other.length ? 0 : other.length - precisionBits;
    UnsignedInteger adjustedDivisor = other.rightShift(shiftBack);
    if (shiftBack) ++adjustedDivisor;
    const std::uint32_t inversePrecision = precisionBits + adjustedDivisor.length;
    const std::uint32_t totalShift = inversePrecision + shiftBack;
    const std::uint32_t truncShift = other.length - 1;
    UnsignedInteger inv = adjustedDivisor.computeInverse(inversePrecision);
    UnsignedInteger quotient;
    if (shiftBack == 0 && other.length > 1 && length >= other.length + BruteforceThreshold)
        quotient = (rightShift(truncShift) * inv).rightShift(totalShift - truncShift);
    else
        quotient = (*this * inv).rightShift(totalShift);
    UnsignedInteger qOther = quotient * other;
    while (qOther > *this) --quotient, qOther -= other;
    UnsignedInteger remainder = *this - std::move(qOther);
    for (; remainder >= other; ++quotient, remainder -= other);
    return std::make_pair(std::move(quotient), std::move(remainder));
}
```

> 注：实际代码有 `#ifdef PROFILE_DIVISION` 分支，改动时两个分支都要改。

---

## 9. 当前 computeInverse（行 714-778，递归 Newton + 融合优化，Core2 复用此函数求 inv）

```cpp
UnsignedInteger computeInverse(std::uint32_t precisionBits) const {
    if (length < BruteforceThreshold || precisionBits < length + BruteforceThreshold) {
        UnsignedInteger numerator(precisionBits + 1, precisionBits + 1);
        std::memset(numerator.digits, 0, precisionBits << 2), numerator.digits[precisionBits] = 1;
        return numerator.bruteforceDivisionAndModulus(*this).first;
    }
    const std::uint32_t halfPrecision = (precisionBits - length + 5) >> 1, shiftBack = halfPrecision > length ? 0 : length - halfPrecision;
    UnsignedInteger truncated = rightShift(shiftBack);
    const std::uint32_t newPrecision = halfPrecision + truncated.length;
    UnsignedInteger approximateInverse = truncated.computeInverse(newPrecision);
    UnsignedInteger sqMulResult = approximateInverse.square() * *this;
    // 融合 (a+a).leftShift(s) - sqMul.rightShift(t) 为单次操作
    const std::uint32_t shift1 = precisionBits - newPrecision - shiftBack;
    const std::uint32_t shift2 = 2 * (newPrecision + shiftBack) - precisionBits;
    UnsignedInteger result(approximateInverse.length + shift1 + 1, approximateInverse.length + shift1 + 1);
    std::memset(result.digits, 0, shift1 << 2);
    { /* result = 2 * approximateInverse << shift1, in-place carry */ }
    if (sqMulResult.length > shift2) { /* result -= sqMulResult >> shift2, in-place borrow */ }
    for (; result.length > 1 && !result.digits[result.length - 1]; --result.length);
    return --result;
}
```

**精度语义**：`computeInverse(p)` 返回 `floor(BASE^p / D) - 1`（最后 `--result`）。误差 ≤ divid_high，q 误差 ≤1，校正循环兜底。与 div_1st `absInvNewton` 语义兼容。

---

## 10. div_1st 参考代码（移植目标）

### 10.1 absDivNewtonCore2（行 1778-1802，分块 Newton 核心）

```cpp
static void absDivNewtonCore2(Span dividend, View divisor, Span quotient)
{
    if (dividend.size <= divisor.size) return;
    size_t len1 = dividend.size, len2 = divisor.size;
    Limb divisor_high = divisor[len2 - 1];
    assert(divisor_high >= HALF_BASE);  // 调用前必须归一化
    std::vector<Limb> inv(len2 + 1);
    Span inv_span(inv.data(), inv.size());
    absInvNewton(divisor, inv_span);  // 对除数求一次逆（只依赖除数）
    size_t blocks = len1 / len2, len1_rem = len2 * blocks;
    auto divid_it = dividend.ptr + (len1_rem - len2);
    auto quot_it = quotient.ptr + (len1_rem - len2);
    // 从最高块开始，向低块迭代
    absDivNewtonWithInv(dividend + (len1_rem - len2), divisor, quotient + (len1_rem - len2), inv_span);
    while (divid_it > dividend.ptr) {
        divid_it -= len2;
        quot_it -= len2;
        absDivNewtonWithInv(Span(divid_it, len2 * 2), divisor, Span(quot_it, len2), inv_span);
    }
}
```

**关键**：`inv` 只算一次，所有块复用。块间通过修改 `dividend`（in-place）传递余数。

### 10.2 absDivNewtonWithInv（行 1685-1717，单块除法）

```cpp
static void absDivNewtonWithInv(Span dividend, View divisor, Span quotient, View inv_span)
{
    assert(dividend.size <= divisor.size * 2);
    if (dividend.size <= divisor.size) return;
    size_t k = divisor.size;
    Span divid_high = dividend + (k - 1);  // 取高 (dividend.size - k + 1) 位
    std::vector<Limb> qhat(divid_high.size + inv_span.size);
    std::vector<Limb> prod(qhat.size() - 1);
    Span qhat_span(qhat.data(), qhat.size()), prod_span(prod.data(), prod.size());
    absMul(inv_span, divid_high, qhat_span);   // qhat = inv * divid_high
    qhat_span = qhat_span + (k + 1);            // 右移 k+1 位
    absMul(divisor, qhat_span, prod_span);      // prod = divisor * qhat
    prod_span.size = count_true_length(prod_span.ptr, prod_span.size);
    while (absCompare(prod_span, dividend) > 0) {  // 校正：prod > dividend
        absSub(prod_span, divisor, prod_span);      // prod -= divisor
        absSub1(qhat_span, 1, qhat_span);           // qhat--
    }
    absSub(dividend, prod_span, dividend);  // divid -= prod（in-place，余数传给下一块）
    dividend.size = k;
    while (absCompare(dividend, divisor) >= 0) {  // 残余校正
        absSub(dividend, divisor, dividend);
        absAdd1(qhat_span, 1, qhat_span);
    }
    assert(qhat_span[qhat_span.size - 1] == 0);
    qhat_span.size--;
    std::copy(qhat_span.begin(), qhat_span.end(), quotient.begin());
}
```

**对应 OPT 实现**：
- `absMul(inv_span, divid_high, qhat_span)` → `qhat = inv * divid_high`（用 OPT `operator*`，divid_high 是 dividend 的子段，需构造 UnsignedInteger 或裸指针操作）
- `qhat_span + (k+1)` → `qhat.rightShift(k+1)`
- `absMul(divisor, qhat_span, prod_span)` → `prod = divisor * qhat`
- 校正循环 → `while (prod > dividend_block) --qhat, prod -= divisor;`
- `absSub(dividend, prod_span, dividend)` → `dividend_block -= prod`（in-place，块间余数传递）

### 10.3 absInvNewton（行 1638-1684，递归求逆）

> **OPT 直接复用 `computeInverse` 替代此函数**，不需要移植。`computeInverse(len2+1)` 即可。

### 10.4 absDivRem 入口（行 1803-1863，三路分发 + 归一化）

```cpp
void absDivRem(const Integer &divisor, Integer &quotient, Integer &remainder) const
{
    // ... 特判 cmp==0, cmp<0, len2==1 ...
    Limb factor = divisorNormalizeFactor(divisor.getView());
    Integer dividend_norm = (*this) * factor, divisor_norm = divisor * factor;
    size_t len1 = dividend_norm.length(), len2 = divisor_norm.length();
    size_t quot_len = len1 - len2 + 1;
    // 预处理最高位
    Span high = dividend_span + (quot_len - 1);  // 高 len2 位
    if (absCompare(View(high), View(divisor_span)) >= 0) {
        quotient.data[quot_len - 1] = 1;
        absSub(high, divisor_span, high);
    } else {
        quotient.data[quot_len - 1] = 0;
    }
    Span quot_span(quotient.data.data(), len1 - len2);
    // 三路分发
    if (len2 <= 64 || (len1 - len2) <= 64) {
        absDivBasicCore(dividend_span, divisor_span, quot_span);
    } else if (len1 < len2 * 2) {
        absDivNewtonCore1(dividend_span, divisor_span, quot_span);
    } else {
        absDivNewtonCore2(dividend_span, divisor_span, quot_span);
    }
    // 余数还原
    dividend_norm.removeLeadingZero();
    Limb rem = dividend_norm.selfDivRem1(factor);  // remainder / factor
    assert(rem == 0);
    remainder = std::move(dividend_norm);
}
```

**注意**：div_1st 在 Core2 之前预减了最高块（`quotient[quot_len-1]`），把 dividend 缩到 `len1_rem = len2 * blocks`。OPT 移植时需对应处理。

### 10.5 divisorNormalizeFactor（行 1864-1891，归一化因子）

```cpp
static Limb divisorNormalizeFactor(View divisor) {
    assert(divisor.size > 0);
    constexpr int HALF_BASE_BITS = hint_bit_length<uint32_t>(HALF_BASE);
    Limb high_limb = divisor[divisor.size - 1];
    if (high_limb >= HALF_BASE) return 1;
    int bits = hint_bit_length<uint32_t>(high_limb);
    int shift = HALF_BASE_BITS - bits;
    Limb2 carry = 0;
    for (size_t i = 0; i < divisor.size - 1; i++) {
        carry += Limb2(divisor[i]) << shift;
        carry /= BASE;
    }
    carry += Limb2(high_limb) << shift;
    if (carry < HALF_BASE) shift++;       // 补偿
    else if (carry >= BASE) shift--;      // 溢出
    return Limb(1) << shift;
}
```

**OPT 适配**：
- `HALF_BASE = Base/2 = 50000000`，`HALF_BASE_BITS = 26`
- `factor = 1 << shift`，max shift = 26，`factor <= 2^26 = 67108864 < Base`（单 limb）
- 归一化：`dividend_norm = *this * factor`，`divisor_norm = other * factor`（大数 × 单 limb）
- 还原：quotient 无需还原（factor 在 floor 除法中消去）。remainder_norm / factor → 精确除法，factor 是单 limb → `bruteforceDivisionAndModulus(factor)`（factor.length == 1 < BruteforceThreshold，不递归 Core2）

---

## 11. 移植设计决策（路径 C）

### 11.1 整体流程（newtonDivCore2）

```
divisionAndModulus(other):
    if length < 2*other.length 或 other.length < BruteforceThreshold:
        走原路径（一次性 Newton）
    else:
        shift = divisorNormalizeShift(other)
        factor = 1 << shift
        dividend_norm = *this * factor
        divisor_norm = other * factor
        // 归一化后 divisor_norm 高位 >= HALF_BASE
        inv = divisor_norm.computeInverse(divisor_norm.length + 1)  // 只算一次
        quotient = newtonDivCore2(dividend_norm, divisor_norm, inv)
        remainder_norm = dividend_norm  // Core2 in-place 修改后剩余部分
        remainder = remainder_norm / factor  // 单 limb 精确除法
        return (quotient, remainder)
```

### 11.2 单块除法（newtonDivWithInv）

对应 `absDivNewtonWithInv`：
```
输入: block(2*len2), divisor(len2), inv(len2+1)
qhat = inv * block_high          // block_high = block 的高 (len2+1) 位
qhat = qhat >> (len2 + 1)        // 右移 k+1 位
prod = divisor * qhat
while (prod > block): qhat--, prod -= divisor   // 校正
block -= prod                    // in-place，余数传给下一块
while (block >= divisor): block -= divisor, qhat++  // 残余校正
quotient_block = qhat
```

### 11.3 关键决策

- **inv 求逆**：复用 OPT `computeInverse(len2+1)`，不移植 `absInvNewton`。
- **块内乘法**：复用 OPT 成熟 `operator*`（含 unbalanced 优化 + DIF 缓存）。块大小 = len2，`operator*` 内部会自动触发 unbalanced 拆分。
- **块间余数传递**：直接操作 `dividend_norm.digits` 裸数组（零拷贝），与 div_1st in-place Span 修改一致。
- **归一化**：用 div_1st 的 `1<<shift` 因子法（保证校正循环 ≤2 次迭代）。
- **精度**：OPT `computeInverse(len2+1)` 返回 `floor(BASE^(len2+1)/D) - 1`，q 误差 ≤1，校正循环兜底。与 div_1st 兼容。

### 11.4 注意事项

- **大数 × 单 limb 乘法**：归一化需 `*this * factor`（factor 是单 limb）。OPT 无专用单 limb 乘法，可构造 `UnsignedInteger(factor)` 再 `operator*`，或手搓单 limb 乘法循环（更快）。接手 AI 需确认最优方式。
- **预减最高块**：div_1st absDivRem 在 Core2 前预减最高位（quotient[quot_len-1]）。OPT 移植需对应处理，否则 Core2 的最高块 dividend 可能 > 2*len2。
- **块边界**：`blocks = len1 / len2`，`len1_rem = len2 * blocks`。从 `dividend + (len1_rem - len2)` 开始，向低地址迭代。
- **PROFILE_DIVISION**：divisionAndModulus 有 `#ifdef PROFILE_DIVISION` 双分支，改动两处都要改。
- **测试**：本地用 g++ `-O2 -std=c++23 -march=native -DENABLE_VALIDITY_CHECK` 编译，随机数据对比 bruteforce 结果。Windows stdout 文本模式会把 `\n` 转 `\r\n`，测试脚本需 `.replace('\r\n', '\n')`。
- **合并提交**：改完 `masonxiong_opt.cpp` 后运行 `_gen_submit.ps1` 重新生成 `lc_division_submit.cpp`。

---

## 12. operator*= unbalanced 拆分（行 1161-1261，块内复用参考）

已有 DIF(other) 缓存复用机制（thread_local cachedFirst/cachedSecond）。当 `other.length >= UnbalancedThreshold && length >= other.length*2` 时触发。块内调用 `operator*` 时，若块大小 >= UnbalancedThreshold 会自动复用此优化。关键代码结构见行 1164-1261。

---

## 13. 提交流程

1. 改 `masonxiong_opt.cpp`（插入 Core2 + 改 divisionAndModulus）
2. 本地编译测试：`g++ -O2 -std=c++23 -march=native -o test lc_division_submit.cpp`（或用 lc_division.cpp + 库分别编译）
3. 随机数据正确性验证
4. 运行 `_gen_submit.ps1` 重新生成 `lc_division_submit.cpp`
5. 提交 `lc_division_submit.cpp` 到 https://judge.yosupo.jp/problem/division_of_big_integers

---

## 14. 历史对话要点

- 用户多次强调："我们一直在弄 NEWTON 移植！我没有选择干别的！记死这一点！"——I/O 改动是附带完成的，主线一直是 Newton 移植。
- 用户曾因 AI 偏离主线（先做 I/O）而不满。**接手 AI 应直接开始实现 Core2 代码，不要再做 I/O 或其他事情。**
- 之前在实现 Core2 代码的起点丢失了上下文，尚未编写任何 Core2 代码。
