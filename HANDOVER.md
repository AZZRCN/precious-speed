# 交接文档 / Handover Document

## 1. 项目概述

**项目名称**：precious-speed
**目标**：在 [Library Checker](https://judge.yosupo.jp/) 大整数运算问题上冲击第一名
**GitHub 仓库**：https://github.com/AZZRCN/precious-speed
**作者**：AZZRCN (qazwsx233343@163.com)

---

## 2. 最终成绩

| 问题 | 最优成绩 | Submission ID | 提交文件 | 日期 |
|------|----------|---------------|----------|------|
| [除法 Division](https://judge.yosupo.jp/problem/division_of_big_integers) | **108 ms (#1)** | #385741 | `best/div.cpp` | 2026-07-16 |
| [乘法 Multiplication](https://judge.yosupo.jp/problem/multiplication_of_big_integers) | **36 ms** | #385663 | `best/mul.cpp` | 2026-07-15 |
| [加法 Addition](https://judge.yosupo.jp/problem/addition_of_big_integers) | **18 ms** | #385662 | `best/add.cpp` | 2026-07-15 |

**moptm 混合版成绩**（含 Core2 分块除法）：
- 除法 118ms / 乘法 37ms / 加法 19ms

---

## 3. 文件结构

```
d:\precious_speed\
├── best/                          # 最优 LC 提交文件（最终提交版）
│   ├── add.cpp                    # 加法 18ms，基于 masonxiong_opt
│   ├── mul.cpp                    # 乘法 36ms，基于 moptm
│   └── div.cpp                    # 除法 108ms (#1)，基于 HyperInt-mini (div_work)
│
├── moptm.cpp                      # 混合版：masonxiong_opt + Core2 分块除法
├── masonxiong_opt.cpp             # 基线：AVX2/FMA FFT + Newton-Raphson 除法
├── masonxiong.cpp                 # 原始 masonxiong 代码
├── mason_compat.hpp               # masonxiong 兼容层
│
├── mason_opt/                     # 开发工作目录
│   ├── div_1st.cpp                # 第一名原始代码基线（2122行）
│   ├── div_work.cpp               # div_1st 的除法优化版（108ms #1，约2326行）
│   ├── div_1st_716.cpp            # div_work 的 LC 提交记录（含各测试点耗时）
│   ├── div_mopt715.cpp            # masonxiong_opt 的 LC 提交记录（161ms）
│   ├── test_moptm.cpp             # moptm 测试程序（文件 I/O）
│   ├── test_mason.cpp             # masonxiong_opt 测试程序（文件 I/O）
│   ├── bench_mul.cpp              # 乘法基准测试
│   ├── bench_div_moptm.cpp        # 除法基准测试
│   ├── fuzz_gen.cpp               # 随机测试生成器
│   ├── fuzz_gen2.cpp              # LC 模式测试生成器
│   ├── gen_submit.py              # 单文件提交版本生成脚本
│   ├── gen_div_input.cpp          # 除法输入生成器
│   └── ...                        # 其他开发/测试文件
│
├── toolbox/                       # 辅助工具（SAM搜索、Everything搜索等）
├── bench_boost.cpp                # Boost.multiprecision 基准测试
├── bench_mason.cpp                # masonxiong 基准测试
├── dialog.txt                     # LC 提交记录表单
├── README.md                      # 项目说明（中英双语）
└── .gitignore
```

---

## 4. 核心技术

### 4.1 乘法（AVX2/FMA 复数 FFT）

- **基线**：masonxiong_opt 的 `__m256d` 蝶形运算，一次处理 2 个复数
- **BASE**：10^8，Limb 类型 uint32_t
- **关键路径**：
  - `decimationInFrequency` / `decimationInTime`：AVX2 蝶形
  - `frequencyDomainPointwiseMultiply`：频域点积
  - `frequencyDomainPointwiseSquare`：专用平方路径（省一次 DIF + 半次点积）
- **不平衡乘法拆分**：当长度差异大时，将长操作数分块，复用 DFT 结果

### 4.2 除法（Newton-Raphson + Core2 分块）

#### Core1 路径（len1 < len2*3，blocks <= 2）
- 一次性 `computeInverse(invPrecision)` + 一次性乘法 + 校正循环
- 适用于 blocks == 1 或 2

#### Core2 路径（len1 >= len2*3，blocks >= 3）
- **来源**：从 div_work（HyperInt-mini）移植到 moptm
- **算法**：
  1. 预计算 `inv = other.computeInverse(2*len2 + 5)`
  2. 从高位到低位分块，每块 len2 位
  3. `window = remainder.leftShift(len2) + block`
  4. `qhat = (window * inv) >> invPrecision`
  5. 校正循环：`while (qOther > window) --qhat; while (remainder >= other) ++qhat;`
- **性能**：blocks >= 3 时 -19%~-35%，blocks == 2 时 +40%（故阈值设 len1 >= len2*3）

### 4.3 高速 I/O

- **Linux**：`mmap` 输入 + `fwrite` 输出
- **Windows**：`fread` 输入（16MB 缓冲区）
- **解析**：SWAR（SIMD Within A Register）64位块扫描数字边界
- **输出**：32MB oBuffer + 查表（`detail::O` 预生成 10000 个 4 位数字串）

---

## 5. 已应用的优化

### 5.1 FFT 数组冗余零初始化去除

**位置**：moptm.cpp 中 5 处 `new __m128d[N]()` → `new __m128d[N]`
- `baseFactors`（resize 函数）
- `cachedFirst` / `cachedSecond`（不平衡乘法路径）
- `firstArray` / `secondArray`（平衡乘法路径）
- `sqArray`（平方路径）

**原因**：`()` 会零初始化整个数组，但 head 部分被 packing loop 立即覆写，tail 部分被 explicit `memset` 覆写。对大 FFT（1M 位，transformLength=262144）省去约 4MB 无意义清零。

**效果**：乘法 -4%~-7%

### 5.2 Core2 分块除法移植

从 div_work（HyperInt-mini，BASE=10^4/Limb=uint16_t）移植到 moptm（masonxiong_opt，BASE=10^8/Limb=uint32_t），处理数据类型转换差异。

---

## 6. 关键 Bug 修复

### 6.1 Core2 路径 blocks 计算错误（导致 LC WA）

**症状**：除法提交到 LC 后，medium_02、large_00/01、a_max_b_random、length_ratio_integer_02-05 全部 WA

**根因**：`blocks = len1 / len2`（整数除法），当 len1 不是 len2 的整数倍时，最高位数字丢失。

例如 len1=480, len2=125 时，blocks=3 只覆盖 digits[0..374]，digits[375..479] 完全未被处理。

**修复**：
```cpp
// 修复前
const std::uint32_t blocks = len1 / len2;

// 修复后（向上取整）
const std::uint32_t blocks = (len1 + len2 - 1) / len2;
```

最高块不足 len2 位时 qhat=0，remainder 正确传递到下一块。

**验证**：fuzz2（95 cases）+ lc_scale（8 cases 含 1M/100k）+ fuzz_5000（5000 cases）+ Python 独立验证全部通过。

### 6.2 Core2 阈值不当导致 blocks==2 变慢

**根因**：初始设 `len1 >= len2*2`（blocks>=2），但 blocks==2 时 Core2 比 Core1 慢 40%（2 次 2n×n 乘法 vs 1 次 2n×n）

**修复**：改为 `len1 >= len2*3`（blocks>=3），blocks==2 走 Core1

### 6.3 PowerShell 重定向问题

**根因**：PowerShell 不支持 `<` 重定向；`Out-File -Encoding ASCII` 改变文件格式导致 segfault

**修复**：所有测试程序改用 ifstream/ofstream 文件参数

### 6.4 test_moptm.exe 卡住（Core2 无限循环）

**根因**：
1. `computeInverse(len2+1)` 精度太低，len2>=96 时走 bruteforce 极慢
2. workBuf 窗口逻辑错误，用 `lowPart + newBlockRem` 重建 workBuf 没有正确传递余数

**修复**：
1. 改为 `computeInverse(2*len2+5)` 确保 Newton 迭代路径
2. 重写为 `window = remainder.leftShift(len2) + block`

---

## 7. 分析过但未应用的优化

### 7.1 32 字节对齐分配（收益小）
- `new __m128d[]` 只有 16 字节对齐
- `_mm_malloc(,32)` 可实现 32 字节，用 `_mm256_load_pd` 替代 `_mm256_loadu_pd`
- **不应用原因**：Skylake+ 上 unaligned load 不跨 cache line 时免费，收益极小

### 7.2 Branchless twiddle selection（收益小）
- pointwise multiply 中的 `forwardIndex & 1 ?` 三元可能是 50/50 不可预测分支
- **不应用原因**：编译器大概率已优化为 branchless，需汇编验证

### 7.3 Carry chain 优化（算法级，风险大）
- 串行依赖链是瓶颈，`% Base, / Base` 已被编译器用 reciprocal multiply 优化
- **不应用原因**：需 lazy-carry 重构才能突破，风险大

### 7.4 float FFT 替代 double FFT（精度风险）
- `__m256` 8 floats vs `__m256d` 4 doubles，2x 吞吐
- **不应用原因**：精度风险大，需重设计

---

## 8. 关键参数

| 参数 | 值 | 位置 | 说明 |
|------|----|------|------|
| Base | 100000000 (10^8) | moptm.cpp / masonxiong_opt.cpp | 每个 limb 存 8 位十进制数 |
| BruteforceThreshold | 96 | moptm.cpp / masonxiong_opt.cpp | len < 96 走 bruteforce 除法 |
| UnbalancedThreshold | 4096 | moptm.cpp / masonxiong_opt.cpp | 不平衡乘法拆分阈值 |
| TransformLimit | 4194304 | moptm.cpp / masonxiong_opt.cpp | FFT 最大长度 |
| Core2 阈值 | len1 >= len2*3 | moptm.cpp divisionAndModulus | blocks >= 3 走 Core2 |
| invPrecision | 2*len2 + 5 | moptm.cpp Core2 路径 | Newton 迭代精度 |

---

## 9. div.cpp vs moptm.cpp 的差异

`best/div.cpp`（108ms #1）和 `moptm.cpp`（118ms）是**不同的代码库**：

| 特性 | best/div.cpp | moptm.cpp |
|------|-------------|-----------|
| 基础 | HyperInt-mini (With-Sky) | masonxiong_opt |
| BASE | 10^4 | 10^8 |
| Limb | uint16_t | uint32_t |
| 除法 | 原生 Core2 分块 | 移植的 Core2 分块 |
| 乘法 | HyperInt-mini FFT | AVX2/FMA 复数 FFT |
| 成绩 | 108ms (#1) | 118ms |

**结论**：除法最快的是 `best/div.cpp`（HyperInt-mini 基础），乘法/加法最快的是 masonxiong_opt 基础。

---

## 10. 编译与提交

### 编译命令
```bash
g++ -O2 -mavx2 -mfma -funroll-loops -o solution best/div.cpp
g++ -O2 -mavx2 -mfma -funroll-loops -o solution best/mul.cpp
g++ -O2 -mavx2 -mfma -funroll-loops -o solution best/add.cpp
```

### LC 提交注意
- LC 的 g++ 支持 `-O2 -mavx2 -mfma`
- 代码自动检测平台：Linux 走 mmap，Windows 走 fread
- `#ifndef MOPTM_NO_IO` 保护仅用于本地测试，LC 提交不影响

### 单文件提交版本生成
```bash
cd d:\precious_speed\mason_opt
python gen_submit.py
# 生成 lc_add_moptm_submit.cpp 和 lc_mul_moptm_submit.cpp
```

---

## 11. 对拍测试方法

### 快速对拍
```bash
cd d:\precious_speed\mason_opt
# 生成测试数据
.\fuzz_gen.exe 42 5000 fuzz_5000.txt
# 运行两个程序
.\test_moptm_fair.exe fuzz_5000.txt out_moptm.txt
.\test_mason_new.exe fuzz_5000.txt out_mason.txt
# 对比（PowerShell）
$a = Get-Content -Raw out_moptm.txt; $b = Get-Content -Raw out_mason.txt
if ($a -eq $b) { Write-Host "MATCH" } else { Write-Host "MISMATCH" }
```

### Python 独立验证
```python
import sys
sys.set_int_max_str_digits(10000000)
with open('lc_scale.txt', 'r') as f:
    t = int(f.readline())
    cases = [(f.readline().strip(), f.readline().strip()) for _ in range(t)]
with open('out_moptm_lc.txt', 'r') as f:
    lines = f.read().strip().split('\n')
for i, (a_str, b_str) in enumerate(cases):
    a, b = int(a_str), int(b_str)
    q, r = divmod(a, b)
    mq, mr = map(int, lines[i].split())
    assert mq == q and mr == r, f"Case {i} FAILED"
```

---

## 12. 后续优化方向（如需继续）

1. **除法**：差距 10ms（118ms vs 108ms）。div_work 的 HyperInt-mini 基础在除法上更快，可考虑进一步融合
2. **乘法**：差距 1ms（37ms vs 36ms）。代码与 masonxiong_opt 相同，差异来自二进制体积/I-cache 压力
3. **加法**：差距 1ms（19ms vs 18ms）。可能是噪声
4. **float FFT**：2x 吞吐但精度风险大
5. **lazy-carry**：算法级优化，突破 carry chain 串行依赖

---

## 13. 致谢

- **[masonxiong](https://www.luogu.com.cn/user/446979)** & **[yuygfgg](https://www.luogu.com.cn/user/251551)** — masonxiong_opt 基础库
- **[With-Sky](https://github.com/With-Sky/HyperInt-mini)** — HyperInt-mini 库（除法优化基础）
- **[AZZRCN](https://github.com/AZZRCN)** — Core2 分块除法移植、高速 I/O、集成工作

---

*交接文档生成时间：2026-07-17*
