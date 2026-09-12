# HEX div #393027 — FFT 主导性剖析与下一刀提案

> 实测日期：2026-08-18 | 基线：v27 (md5 5b7bafa1) 单巨例 103900÷51950 limb = **206,952,320 I refs**
> 度量：VM66 (Intel Ultra 9 285H) callgrind I refs —— 跨 x86-64 架构指令数与判官 AMD EPYC 7B13 (Zen3) 一致，为唯一真值。

## 1. 函数级 I refs 分布（self%，方向性）

| 函数 | self% | 角色 | 调用次数 |
|---|---|---|---|
| `fft::difRec` | 36.6% | 正变换（基-4 递归 + 叶 difFlat） | 15 |
| `fft::pointwise` | 24.6% | 频域复数乘（twiddle 查表 `cmulv`） | — |
| `fm_mul` | 10.4% | FixedFFT 乘法 | — |
| `split_b2` | 5.1% | BZ 分割 | — |
| `fft::ditRec` | 0.2% | 逆变换 | 9 |
| `fft::difRecZeroHi` | 1.2% | 半长正变换 | — |
| 未解析 `(id)` ≈10% | libm 内存/数学例程（非 FFT twiddle） | | |
| **FFT 总占比** | **≈ 80%** | | |

`ditRec` 自合占比低是**算法结构使然**（逆变换调用仅 9 次 vs 正变换 15 次），非 bug。

## 2. 已证伪 / 已排除的微架构优化

| 候选 | 结论 | 实测 |
|---|---|---|
| SoA 蝶形布局 | **证伪** | AoS 134.7M vs SoA 138.8M I refs（+3.0%，更差）；shuffle 在 Zen3 是 1-uop 廉价指令 |
| 即时 trig（twiddle） | **无空间** | `twbase[4096]` 64KB 表 + 启动期 `std::sin/cos` 填表；运行时 `twg(i)` 查表 `cmul`，零即时 sin/cos |
| AVX2 宽度 | **已极致** | `__m256d` 蝶形 + FMA + 混合 radix 3/5 + 2 级 L1 twiddle |

**结论：FFT 微架构（AVX2 指令级）已无红利。下一刀只能来自算法级。**

## 3. 下一刀候选（算法级）

### 3.1 Packed real FFT —— 最高价值，理论省 ~25–33% 总指令
- **事实**：大整数 limb 是**实数序列**，但 `mul_fft` 用 complex FFT 做实数卷积（虚部吞吐浪费）。
- **优化**：把两个实序列 A、B packed 成 `C = A + i·B`，一次 complex forward FFT 后由 Hermitian 对称提取 `A_dft`、`B_dft`，pointwise 仍做复数乘（频域复数乘不可避免），省一次 forward FFT。
- **收益上界**：当前 forward 15 次 / inverse 9 次。省 1 次 forward ≈ 削减总 FFT 的 ~1/3 ≈ **25% 总指令**（FFT 占 80% → 总指令 ~20% 降幅）。
- **风险**：需重写 `mul_fft` 的 forward / inverse / pointwise 接口（含 `cyc_mul_fixed` / `fm_mul` 路径）。正确性闸门 = 26-case oracle（`reducer/bench.py oracledir`，0 failures 才冻结）。
- **建议**：GLM-5.3 审查数学等价性（Hermitian 提取公式）后实施。

### 3.2 卷积复用（div 内）
- 正变换 15 次，BZ 除数正变换 / 商余 FFT 是否已部分复用？`invertappr` + BZ 的除数正变换是否可跨 Newton 迭代复用（待 `invertappr` 代码确认）。

### 3.3 pick_k / mixed radix 顶层
- `FFT_LEAF_LOG` 与 mixed radix 3/5 顶层选择是否最优（实测待定，可能影响常数）。

## 4. 风险与闸门（提交前）
- 改 `393027_opt.cpp` 前必留原件（v27 md5 5b7bafa1）。
- 任何 FFT 改动须过 **26-case oracle（0 failures）+ callgrind I refs 不增**。
- **AVX-512 默认关**（判官 Zen3 无 AVX-512，3× 惩罚）；仅 `gamble` flag 启用。
- 已知 blockers：v27 多 query 段错误（单 query 正常，提交前必修或确认 T=1）。

## 5. 复现命令
```bash
# VM 上（双 IP 自动探测）
python -c "import sys;sys.path.insert(0,'..');from tools.vm_ssh import put,run; \
put('D:/precious_speed/393027_opt.cpp','/tmp/hexcmp/src/393027_opt.cpp'); \
put('D:/precious_speed/reducer/cases.txt.big','/tmp/hexcmp/cases.txt.big'); \
run('cd /tmp/hexcmp && g++ -O3 -march=haswell -g -no-pie -fno-pie -std=c++17 -fno-stack-protector -o bin/ref_np src/393027_opt.cpp && valgrind --tool=callgrind --cache-sim=yes --callgrind-out-file=/tmp/hexcmp/cg_np.out ./bin/ref_np < /tmp/hexcmp/cases.txt.big')"
# 函数级聚合（addr2line 映射）
python -c "import sys;sys.path.insert(0,'..');from tools.vm_ssh import put,run; \
put('D:/precious_speed/reducer/aggregator.py','/tmp/hexcmp/aggregator.py'); \
run('cd /tmp/hexcmp && python3 aggregator.py')"
```
