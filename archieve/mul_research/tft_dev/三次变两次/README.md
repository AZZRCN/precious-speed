# 三次变两次优化 调研结论

## 状态：已在现有代码中实现（无需额外工作）

## 调研发现

### absSqr (自平方) 路径
- `fftSqr` (line 2326) 调用 `real_conv(v, v, float_len)` （v == v）
- `real_conv` 在 in_out1 == in2 时走自平方路径：1 DFT + 1 DOT + 1 IDFT = 2 FFT
- **已是自平方最优**，无需三次变两次

### fftMul (普通乘法) 路径
- `fftMul` (line 2241) 调用 `real_conv(v1, v2, float_len)` （v1 != v2）
- `real_conv` 在 in_out1 != in2 时调用 `dif_two<true>` 或 2x `dif<true>`
- **dif<true> 的 true 参数 = RIRI_IN**：表示输入是 RIRI 打包格式
- RIRI 打包 = 三次变两次的变体：两个实数序列打包为复数序列，1 次复数 DFT

### RIRI 打包 vs 经典三次变两次
| 方法 | DFT | 中间操作 | IDFT | 总 FFT |
|------|-----|----------|------|--------|
| RIRI 打包（当前） | 1 | 分离 A,B + 点积 A*B | 1 | 2 |
| 经典三次变两次 | 1 | F^2 + 取虚部/2 | 1 | 2 |

两者都是 1 DFT + 1 IDFT，性能等价。当前代码已用 RIRI 打包实现最优。

## 结论
- **Task 5 已在现有代码中实现**：RIRI 打包（dif<true>）即三次变两次
- 无需额外实现
- LC 测试点走 fftMul（普通乘法），已享受 RIRI 打包优化

## 代码位置
- [mul.cpp#L2241](file:///d:/precious_speed/mul.cpp#L2241) fftMul
- [mul.cpp#L1207](file:///d:/precious_speed/mul.cpp#L1207) real_conv
- [mul.cpp#L1228](file:///d:/precious_speed/mul.cpp#L1228) dif_two<true> (RIRI 打包)
