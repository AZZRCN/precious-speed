# HEX div 指令数基准 (amax_1) — 2026-08-14

采集: VM .55 (i7-11370H) / g++ 15.2 / `-O3 -g -std=c++17 -march=znver3`
命令: `valgrind --tool=callgrind --cache-sim=no --branch-sim=no ./div_g.bin < cases_hex/amax_1.in`
输入: 1 组, A=100000 limbs, B=50000 limbs (HEX 道最重用例, LC 上对应 a_max_b_random_00 = 21ms)
**总 Ir (LEAF=9, 2026-08-14) = 318,905,679** (perf instructions:u, VM .55 i7-11370H/-march=znver3)。callgrind --cache-sim=no 基准 331,853,844 为 LEAF=8; 函数排行占比不变, 仅总 Ir 随 LEAF 微调。
符号映射: PIE 基址 0x4000000, `nm --numeric-sort` 手工回填 (callgrind 未解析主二进制符号)

| Ir | 占比 | 函数 | 备注 |
|---:|---:|---|---|
| 118,681,271 | 35.76% | `fft::difRec` (递归上下文) | 正向 radix-4 DIF 本体 |
| 58,415,604 | 17.60% | `fft::ditRec` (递归上下文) | 逆向 DIT 本体 |
| 37,778,106 | 11.38% | `mulg` | limb 乘法调度 (split/merge/carry) |
| 30,415,058 | 9.17% | `fft::pointwise_mixed` | 混合基逐点乘 |
| 14,668,859 | 4.42% | `fft::difRec` (顶层) | |
| 10,332,227 | 3.11% | `main` | |
| 10,072,464 | 3.04% | `fft::dif3StageR` | radix-3 级 |
| 9,671,527 | 2.91% | `fft::pointwise` | |
| 8,245,971 | 2.48% | `__memset_avx2_unaligned_erms` | 已优化过一轮 |
| 4,822,505 | 1.45% | `fft::idit3StageR` | |
| 4,165,832 | 1.26% | `split_b2` | snippet 已交外部超优化 |
| 4,004,785 | 1.21% | `fft::difRecZeroHi` | |
| 3,601,592 | 1.09% | `fft::ditRec` (顶层) | |
| 3,014,002 | 0.91% | `fft::dif5StageR` | |
| 3,432,720 | 1.04% | `invertappr` (递归+顶层) | Newton 迭代自身极轻 |
| 2,338,831 | 0.70% | `__memcpy_avx_unaligned_erms` | |
| 2,100,088 | 0.63% | `parse_limbs` | snippet 已交外部超优化 |
| 1,700,121 | 0.51% | `put_big` | snippet 已交外部超优化 |
| 1,419,269 | 0.43% | `fft::idit5StageR` | |

## 聚合

| 家族 | Ir | 占比 |
|---|---:|---:|
| FFT 蝶形 (difRec/ditRec/dif*StageR/idit*/difRecZeroHi) | 228,700,081 | 68.9% |
| FFT 逐点 (pointwise + pointwise_mixed) | 40,086,585 | 12.1% |
| **FFT 合计** | **268,786,666** | **81.0%** |
| mulg 调度 | 37,778,106 | 11.4% |
| I/O + 转换 (parse_limbs/put_big/split_b2/memcpy/memset) | 18,550,843 | 5.6% |
| Newton (invertappr) 自身 | 3,432,720 | 1.0% |

## 结论 / 优化落点排序 (2026-08-14 更新)

**已落地零风险优化 (amax_1, perf instructions:u):**
- **FFT_LEAF_LOG: 8 → 9**, 318.9M (−0.58% vs 320.8M@L8)。扫描 6..12 单调, L9 最优, L≥10 反弹。difFlat 参数化(按 n 定界)支持 512 点叶, 安全。已 freeze + verify 3/3 OK。
- **MULBF_MAX=48 / INV_BASE=48**: 扫描确认当前值最优 (调大即变差), 保持。

**已确认无空间 / 已关闭:**
1. **FFT 蝶形 68.9%** — 递归 radix-4 + AVX2/FMA 扁平叶已最优; 4-step (+158% Ir 且精度 FAIL) 与自写 radix-4 (数学 bug) 双重否决关闭; split-radix 理论省 ~3-5% 总 Ir 但需重写蝶形(刚吃 bug 教训, 需先建 FFT 单测)。
2. **pointwise 12.1%** — `cmul` 已 `_mm_fmaddsub_pd` (2 FMA+1 MUL, AVX2 复数乘最优), 指令级零空间。
3. **short product / 半尺寸卷积** — 代数证盈亏平衡 (CRT 两半≈1全) 或需高风险重写 (Hensel 已证伪); 不本轮。

**剩余真杠杆 (高风险, 需建 FFT 单测 + 隔离分支):** split-radix 蝶形重写 (~3-5% 总 Ir)。当前 div.cpp 已到算法地板 (FFT 核心与 DEC #1 同源结构)。建议: 交 LC 确认 LEAF=9 微收益, 或启动 split-radix 专门攻关。
