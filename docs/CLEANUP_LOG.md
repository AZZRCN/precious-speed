# 目录整理记事

> **任务**: 阅读 d:\precious_speed 下所有 CPP 文件（按 add/mul/div 方向），确认无可借鉴点后删除。
> **保留方向**: O2 最快 + O3 最快（moptm_fusion.cpp 一个文件两个编译方向）
> **约束**: best/ 三文件保留（融合源头，只读参考）
> **完成时间**: 2026-07-19

---

## 分析结论

### 基线 moptm_fusion.cpp 的 9 项优化（独有）
1. 64KB ParseTable 查表 parse
2. 16 字节 SSE2 SIMD str16to4limbs
3. Barrett reduction divBASE
4. absAdd/absSub AVX2 双肢打包 (scoped pragma)
5. writeTo 表移 + SSE2 4× 展开
6. DIV blocks≥2 fast path
7. absDivMu GMP mu_div_qr 风格
8. fftMulModBm1 cyclic FFT (未通过正确性测试)
9. fftMulModBm1Pre 预计算 DFT (未通过正确性测试)

### 保留文件（有独有优化）

| 文件 | 底座 | 独有优化点 | 价值 |
|------|------|-----------|------|
| moptm_fusion.cpp | hint 10^4 | 9 项优化（基线） | O2/O3 主线 |
| moptm_v1_original.cpp | mason 10^8 | AVX2 __m256d 蝶形 FFT (L213-264)、frequencyDomainPointwiseSquare 平方专用点乘、融合 Newton-Raphson 求逆 | MUL 方向 AVX2 FFT 参考 |
| archieve/cpp/OM.cpp | mason+hint 混合 | masonxiong AVX2 复 FFT (__m256d 蝶形) | MUL 方向参考 |
| archieve/cpp/masonxiong_opt.cpp | mason 10^8 | DIV 截断乘法 (L793-813)、融合 Newton (L736-771)、专用平方路径 (L1391)、DigitAllocator 内存池 (L545-597) | DIV/MUL/MEM 参考 |
| archieve/cpp/masonxiong.cpp | mason 10^8 | 多平台 FFT (SSE2/NEON/complex 回退)、conjugateMask XOR 技巧 | 跨平台参考 |
| archieve/cpp/moptm_mudiv.cpp | hint 10^4 | 全逆预计算 absDivNewtonMu_hint (L106-184) | DIV 超长除法策略参考 |

### 保留的工具文件（archieve/cpp/）
- bench.cpp (10 次中位数计时器)
- gen_data.cpp (测试数据生成)
- gmp_bench.cpp (GMP 参考基准)
- auto_vec_test.cpp (LC auto-vec 探测)
- run_prof.cpp (Windows CreateProcess 计时器)
- test_unbalanced.cpp (不平衡乘法数据生成)
- test_pragma.cpp (AVX2/FMA pragma 探测)
- test_small.cpp (uint16 溢出测试)

### best/ 目录（保留，融合源头）
- add.cpp (LC ADD #1, 18ms)
- mul.cpp (LC MUL #1, 36ms)
- div.cpp (LC DIV #1, 108ms)

---

## 删除清单（共 24 个 CPP + 1 个 HPP + 辅助文件）

### 根目录（9 个，均为 moptm_fusion 子集或一次性脚本）
1. test_o3.cpp - GCC -fo3-pre-expand 测试用例，与大数运算无关
2. o3_submit.cpp - 验证 LC pragma O3 的一次性脚本，使命已完成
3. o3_verify.cpp - 同上
4. bench.cpp - Windows QPC 计时器，被 archieve/cpp/bench.cpp 替代
5. fusion.cpp - moptm_fusion 子集 + pragma O3（LC 无效），无独有
6. fusion_o2only.cpp - moptm_fusion 子集（无 9 项优化），无独有
7. moptm_v3_phase3.cpp - moptm_fusion 早期版本，缺 absDivMu/Barrett/ParseTable
8. moptm_v4_divopt.cpp - moptm_fusion 早期版本，缺 absDivMu
9. moptm_v5_latest.cpp - moptm_v4 代码风格整理版，无新优化

### archieve/cpp/（5 个，均被 moptm_fusion 完全覆盖）
10. moptm.cpp - moptm_fusion 前身，特性全子集
11. moptm_noomp.cpp - moptm.cpp 去 OpenMP 版，无独有
12. moptm_attempt1_approx_inv.cpp - absInvNewtonApprox 与基线 absDivMu 切片等价
13. moptm_blocks2_fft.cpp - blocks≥2 fast path 已在基线
14. hint_all.cpp - 纯 hint 库基础版，完全被覆盖

### archieve/mason_opt/（7 个，全部冗余）
15. moptm.cpp - 与 OM.cpp mason 部分重叠
16. masonxiong_opt.cpp - 与 archieve/cpp/masonxiong_opt.cpp 重复
17. masonxiong.cpp - 与 archieve/cpp/masonxiong.cpp 重复
18. div_1st_716.cpp - WithInv/Fast 拆分已被 absDivMu 合并
19. div_mopt715.cpp - masonxiong_opt + 不平衡 mul，冗余
20. div_work.cpp - Core2 DIV 源头，已被 absDivMu 合并
21. div_1st.cpp - 简化版无 DFT 复用，完全被覆盖
22. mason_compat.hpp - 与 archieve/cpp/mason_compat.hpp 重复

### benchmark_20260715_174849/（旧 benchmark，masonxiong 重复）
23. masonxiong.cpp - 与 archieve/cpp/masonxiong.cpp 重复
24. masonxiong_opt.cpp - 与 archieve/cpp/masonxiong_opt.cpp 重复
25. tle_duel.cpp - 旧 TLE 对决脚本

---

## 删除后文件结构

```
d:\precious_speed\
├── moptm_fusion.cpp          # O2/O3 主线（基线）
├── moptm_v1_original.cpp     # mason 底座，AVX2 蝶形 FFT 参考
├── HANDBOOK.MD
├── CLEANUP_LOG.md            # 本文件
├── GCC_BENCH_REPORT.md
├── MOPTM_FUSION_OPTIMIZATIONS.md
├── ssh_exec.py
├── best/                     # 融合源头（只读）
│   ├── add.cpp
│   ├── mul.cpp
│   └── div.cpp
├── archieve/
│   ├── cpp/
│   │   ├── OM.cpp                    # mason AVX2 复 FFT
│   │   ├── masonxiong_opt.cpp        # DIV 截断乘法/融合 Newton/平方/内存池
│   │   ├── masonxiong.cpp            # 多平台 FFT
│   │   ├── moptm_mudiv.cpp           # 全逆预计算 DIV
│   │   ├── mason_compat.hpp
│   │   ├── bench.cpp                 # 工具
│   │   ├── gen_data.cpp              # 工具
│   │   ├── gmp_bench.cpp             # 工具
│   │   ├── auto_vec_test.cpp         # 工具
│   │   ├── run_prof.cpp              # 工具
│   │   ├── test_unbalanced.cpp       # 工具
│   │   ├── test_pragma.cpp           # 工具
│   │   └── test_small.cpp            # 工具
│   └── (mason_opt/ 目录清空后删除)
└── toolbox/                  # 自定义工具箱
```

---

## 质疑点（供后续开发参考）

1. **moptm_v1_original 的 frequencyDomainPointwiseSquare**：基线 fftSqr 用 real_conv(v,v) 自卷积，可能已隐含点乘优化，需 benchmark 验证
2. **v1 的 AVX2 __m256d 蝶形**：基线 radix-4 标量 FFT 在 -O3 -mavx2 下编译器自动向量化后性能差异未知，需 benchmark
3. **v1 的融合 Newton-Raphson**：基线 absDivMu 已是更先进的 GMP mu_div_qr 风格，v1 的 Newton 可能已被超越
4. **fusion_o2only 在 O2 ADD/MUL 上比 moptm_fusion 快**：说明 moptm_fusion 的某些优化（可能是 absAdd_avx2 或 ParseTable）在 ADD/MUL 上是负优化，需定位并修复
