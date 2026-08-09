# GMP Cyclic Newton 开发文档

## 目标
默认启用 GMP 风格 cyclic Newton 逆 + 2NXN cyclic 卷积除法，获得 8-10% 性能提升。

## 当前状态 (2026-07-25 更新)

### absDivMu 2NXN cyclic — 已成功（默认启用）
- **正确性**: 100 组随机 fuzz + 标准测试全部 MD5 匹配
- **性能** (VM 独占, 15 runs × 50 loops, 中位数):
  - 1M/500k: 25.8ms → 21.8ms (**-15.7%**)
  - 200k/100k: 6.1ms → 5.3ms (**-13.4%**)
- **宏开关**: `DISABLE_2NXN_CYCLIC`（默认启用，定义此宏禁用）
- **已集成到** `div.cpp`（O2 #1 成品）

### absInvNewtonGMP cyclic — 未启用（条件编译，默认关闭）
- **正确性**: 199/200 PASS，1 个 FAIL（正剩余类路径精度问题）
- **性能**: 即使修正正确性，理论分析表明 cyclic 额外开销（串行进位传播）抵消 FFT 减半收益，无净提升
- **宏开关**: `USE_GMP_NEWTON`（默认关闭）
- **状态**: 保留代码供参考，暂不启用

### 已解决问题
- absDivMu cyclic 80+ FAIL + 段错误 → **已修复**（buffer 溢出 + unwrap 逻辑修正）
- fftMulModBm1Pre carry wrap-around → **已修复**（保留 carry=1 并正确传播）

### 未解决问题
- absInvNewtonGMP 正剩余类 1 FAIL → **未修复**（精度条件 `|2X| < B^mn-1` 在某些尺寸下不满足，无净性能收益，暂不投入）

## 代码位置 (archieve/cpp/moptm_fusion.cpp，已归档)

### GMP cyclic 相关函数
- `fftMulModBm1`: L2287-L2402 — cyclic convolution mod (B^m-1)
- `fftMulModBm1Pre`: L2404-L2500 — 预计算 DFT 版本
- `absInvNewtonGMP`: L2689-L2920 — GMP 风格 Newton 逆 (cyclic + fallback)
- `absDivMu` cyclic 路径: L3193-L3514 — 2NXN cyclic 卷积除法

### 宏开关
- `USE_GMP_NEWTON`: 启用 absInvNewtonGMP (替代 absInvNewton)
- `CYCLIC_MIN_K`: absInvNewtonGMP 中启用 cyclic 的阈值 (默认 4096)
- `DISABLE_2NXN_CYCLIC`: 禁用 absDivMu 的 cyclic 路径

## 已知问题

### 1. absInvNewtonGMP cyclic 正剩余类 1 FAIL
- **症状**: CYCLIC_MIN_K=4096 时，199/200 PASS，1 个 FAIL
- **位置**: absInvNewtonGMP 正剩余类路径 (L2811-L2918)
- **推测原因**: GMP 两步分解中的组合逻辑有细微偏差
- **临时方案**: 设 CYCLIC_MIN_K=1000000 (走 fallback，无性能提升)

### 2. absDivMu cyclic 80+ FAIL + 段错误
- **症状**: 启用 cyclic 路径后，~120/200 PASS，80+ FAIL，偶发段错误
- **位置**: absDivMu cyclic 修正逻辑 (L3363-L3418)
- **推测原因**: cyclic unwrap 后 product 是近似的 (mod B^m-1)，修正逻辑基于 borrow 判断方向，但近似误差导致 borrow 判断不可靠
- **临时方案**: 定义 DISABLE_2NXN_CYCLIC (走线性卷积，无性能提升)

### 3. fftMulModBm1Pre carry wrap-around (已修复)
- **原 bug**: carry 绕圈后错误清零 (B^m mod B^m-1 = 1, 不是 0)
- **修复**: 保留 carry=1 并正确传播 (L2385-L2397, L2485-L2496)

## 修复路线图（截至 2026-07-25）

### Phase 1: absInvNewtonGMP cyclic — 暂停
- 精度问题未修复（正剩余类 1 FAIL）
- 理论分析表明即使修复也无性能收益（cyclic 额外开销抵消 FFT 减半）
- 暂不投入，保留条件编译代码供未来参考

### Phase 2: absDivMu 2NXN cyclic — 已完成
- buffer 溢出已修复（`prod_len_max = max(prod_len_max, cyclic_m)`）
- unwrap 逻辑已修正（GMP mu_div_qr.c L288-303 的 2NXN 技巧）
- 正确性验证通过（100 fuzz + 标准测试）
- 性能验证完成（-15.7% / -13.4%）
- 已集成到 div.cpp 并提交 LC（#1, 87ms）

### Phase 3: 性能验证 — 已完成
见 Phase 2 结果

## GMP 源码参考
- `E:\gmp-6.3.0\gmp-6.3.0\mpn\generic\mu_invertappr.c` — Newton 逆
- `E:\gmp-6.3.0\gmp-6.3.0\mpn\generic\mu_divappr_q.c` — 除法
- `E:\gmp-6.3.0\gmp-6.3.0\mpn\generic\sbpi1_div_qr.c` — 基础除法

## 测试脚本
- `fuzz_full_verify.py` — 4 配置全面验证
- `fuzz_carry_fix.py` — default vs disable 对比 + Python 正确答案
- `debug_4fail.py` — FAIL case 捕获
- `benchmark3.py` — 性能基准 (需修复 bench.sh)
