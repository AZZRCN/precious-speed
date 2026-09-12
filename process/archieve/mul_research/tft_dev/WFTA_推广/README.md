# WFTA 16/32 向量化方案 调研结论

## 状态：不可行（向量化无法实现净收益）

## 标量版结论（已完成）
- WFTA 16/32 标量版：-0.4% 负优化（CUR wins 70/200）
- 大用例（max_max/fft_killer）慢 2-2.5%
- 标量操作破坏 SIMD 流水线

## 向量化方案分析

### 方案 1: mul1st（复数0乘1，复数1乘旋转因子）
```cpp
constexpr C2 mul1st(const C2 &other) const
{
    // other = (1, or1), (0, oi1)
    const F2 ii = imag * other.imag;  // 复数0: i0*0=0, 复数1: i1*oi1
    const F2 ri = real * other.imag;  // 复数0: r0*0=0, 复数1: r1*oi1
    const F2 r = real * other.real - ii;  // 复数0: r0*1-0=r0, 复数1: r1*or1-i1*oi1
    const F2 i = imag * other.real + ri;  // 复数0: i0*1+0=i0, 复数1: i1*or1+r1*oi1
    return C2(r, i);
}
```

**问题**：mul1st 与标准 mul 的乘法次数相同（4 次 F2 乘法），复数0的乘法是乘1/0（无效计算）。不会比 mul 更快。

### 方案 2: blend 覆盖（先 mul，再 blend 复数0）
- 先做标准 mul（4 次乘法），再用 blend 指令覆盖复数0
- 比 mul 更慢（额外的 blend 操作）

### 方案 3: 拆分 SIMD 向量
- 将 C2 拆为复数0和复数1分别处理
- 拆分开销大于节省的乘法

## 根本原因
- C2 = 2 个复数打包，编译器自动向量化为 __m256d
- 标准 mul 的 4 次 F2 乘法已是 SIMD 最优
- "复数0乘1"的优化在 SIMD 下无意义（乘1和乘旋转因子的吞吐量相同）

## 结论
- **WFTA 16/32 向量化不可行**：无法在保持 SIMD 的前提下减少乘法次数
- W8^1 的特殊性（实虚部绝对值相等）是 WFTA 8 成功的关键，无法推广到 W16/W32
- 函数定义保留在 mul.cpp 中供未来研究，但 dif 不调用

## 代码位置
- [mul.cpp#L416](file:///d:/precious_speed/mul.cpp#L416) C2::mul
- [mul.cpp#L307](file:///d:/precious_speed/mul.cpp#L307) Float2 定义
