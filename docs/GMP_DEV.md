# GMP Cyclic Newton 开发文档

## 目标
默认启用 GMP 风格 cyclic Newton 逆 + 2NXN cyclic 卷积除法，获得 8-10% 性能提升。

## 当前状态 (2026-07-21)

### 正确性
| 配置 | PASS 率 | 问题 |
|------|---------|------|
| baseline (无GMP无cyclic) | 200/200 | 无 |
| GMP fallback (CYCLIC_MIN_K=∞) | 200/200 | 无 |
| GMP cyclic (CYCLIC_MIN_K=4096) | 199/200 | absInvNewtonGMP 正剩余类路径 1 FAIL |
| absDivMu cyclic | ~120/200 | 80+ FAIL + 段错误 |

### 性能
- GMP fallback (不走 cyclic) 比 baseline 慢 ~20%
- 性能提升必须来自 cyclic 路径 (FFT 大小减半)
- 尚未获得正确的 cyclic 性能数据

## 代码位置 (moptm_fusion.cpp)

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

## 修复路线图

### Phase 1: absInvNewtonGMP cyclic (风险低)
1. 生成 1 个 FAIL case 的详细数据
2. 对比 GMP mu_invertappr.c 源码，逐行检查组合逻辑
3. 重点检查:
   - L2815: copy_backward 是否正确
   - L2830-L2847: cy 计算 (cy=0/1/2/3 分支)
   - L2850-L2855: mpn_cmp + sub_n
   - L2862-L2868: xp_high = m[s..k-1] - xp[s..k-1] - borrow_in
   - L2888-L2915: mul_n + 组合 + 进位传播

### Phase 2: absDivMu cyclic (风险高)
1. 修复段错误 (可能是缓冲区溢出)
2. 重新设计修正逻辑:
   - 方案 A: r-based 修正 (GMP mu_divappr_q.c L234-L270) — 已尝试，PASS 率下降
   - 方案 B: 基于 borrow 的双向修正 — 当前方案，80+ FAIL
   - 方案 C: 完整 unwrap (计算真实 product) — 牺牲部分性能
3. 对比 GMP mu_divappr_q.c 源码，检查 unwrap 逻辑

### Phase 3: 性能验证
1. 修复正确性后，做准确性能基准测试
2. 对比 baseline vs GMP+cyclic
3. 调优 CYCLIC_MIN_K 阈值

## GMP 源码参考
- `E:\gmp-6.3.0\gmp-6.3.0\mpn\generic\mu_invertappr.c` — Newton 逆
- `E:\gmp-6.3.0\gmp-6.3.0\mpn\generic\mu_divappr_q.c` — 除法
- `E:\gmp-6.3.0\gmp-6.3.0\mpn\generic\sbpi1_div_qr.c` — 基础除法

## 测试脚本
- `fuzz_full_verify.py` — 4 配置全面验证
- `fuzz_carry_fix.py` — default vs disable 对比 + Python 正确答案
- `debug_4fail.py` — FAIL case 捕获
- `benchmark3.py` — 性能基准 (需修复 bench.sh)
