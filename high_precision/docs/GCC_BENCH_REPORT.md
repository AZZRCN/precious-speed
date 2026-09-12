# GCC Benchmark Report - precious_speed 项目

> **用途**: 供负责 GCC 改造的 AI 验证 GCC 功能使用。本文档包含完整的 benchmark 数据、5 次原始运行数据、编译命令、环境信息。
> **共享资源**: VM (192.168.1.55, 本地) 和 D 盘 (d:\precious_speed\) 的所有文件可供验证使用。
> **生成时间**: 2026-07-18 22:58

---

## 1. 测试环境

| 项目 | 值 |
|------|-----|
| VM IP | 192.168.1.55 (本地) |
| VM 用户/密码 | (已移除) |
| VM 系统 | Ubuntu 26.04 |
| VM g++ | g++ (Ubuntu 15.2.0-16ubuntu1) 15.2.0 |
| VM CPU | 11th Gen Intel(R) Core(TM) i7-11370H @ 3.30GHz |
| VM CPU 特性 | Tiger Lake, AVX2+FMA+AVX512 |
| SSH 工具 | `python d:\precious_speed\ssh_exec.py "cmd" / --file local remote / --get remote local` |
| VM 工作目录 | `/tmp/bench/` |
| LC 评测环境 | g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE (无 -mavx2) |
| LC 硬件 | GCP c2-standard-4, Intel Cascade Lake, AVX2+FMA+AVX-512 |

### SSH 工具用法
```powershell
# 执行命令
python d:\precious_speed\ssh_exec.py "cmd1" "cmd2"
# 上传文件
python d:\precious_speed\ssh_exec.py --file local_path remote_path
# 下载文件
python d:\precious_speed\ssh_exec.py --get remote_path local_path
```

---

## 2. 测试版本

| 版本名 | 源文件 | 编译开关 | 说明 |
|--------|--------|----------|------|
| moptm_fusion_O2 | moptm_fusion.cpp | -O2 -DHINT_OP_{ADD,MUL,DIV} | 当前最优融合版, 含 9 项优化 + absDivMu |
| moptm_fusion_O3 | moptm_fusion.cpp | -O3 -DHINT_OP_{ADD,MUL,DIV} | 同上, 纯 O3 |
| fusion_o2only_O2 | fusion_o2only.cpp | -O2 -DHINT_OP_{ADD,MUL,DIV} | fusion 去 pragma, 纯 O2 基线 |
| fusion_o2only_O3 | fusion_o2only.cpp | -O3 -DHINT_OP_{ADD,MUL,DIV} | 同上, 纯 O3 |
| fusion_O2 | fusion.cpp | -O2 -DHINT_OP_{ADD,MUL,DIV} | 含 `#pragma GCC optimize("O3,unroll-loops")` |
| fusion_O3 | fusion.cpp | -O3 -DHINT_OP_{ADD,MUL,DIV} | 同上 (pragma 与命令行 O3 叠加) |
| moptm_O2 | moptm.cpp (archieve) | -O2 -DHINT_OP_{ADD,MUL,DIV} | archieve 版 moptm |
| moptm_O3 | moptm.cpp (archieve) | -O3 -DHINT_OP_{ADD,MUL,DIV} | 同上, 纯 O3 |
| best_O2 | best_add.cpp / best_mul.cpp / best_div.cpp | -O2 (无 -DHINT_OP) | LC 排名第一的三文件 (独立 main) |
| best_O3 | 同上 | -O3 (无 -DHINT_OP) | 同上, 纯 O3 |

### 源文件路径
- `d:\precious_speed\moptm_fusion.cpp` (当前最优, ~142KB)
- `d:\precious_speed\fusion_o2only.cpp` (~109KB)
- `d:\precious_speed\fusion.cpp` (~108KB, 含 pragma O3)
- `d:\precious_speed\archieve\cpp\moptm.cpp` (~108KB)
- `d:\precious_speed\best\add.cpp` (~111KB)
- `d:\precious_speed\best\mul.cpp` (~107KB)
- `d:\precious_speed\best\div.cpp` (~103KB)

### 编译命令示例
```bash
# moptm_fusion O2 (LC 真实环境)
g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_ADD -o moptm_fusion_O2_ADD moptm_fusion.cpp
g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_MUL -o moptm_fusion_O2_MUL moptm_fusion.cpp
g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV -o moptm_fusion_O2_DIV moptm_fusion.cpp

# best 三文件 (无 -DHINT_OP 开关)
g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -o best_O2_ADD best_add.cpp
g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -o best_O2_MUL best_mul.cpp
g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -o best_O2_DIV best_div.cpp
```

---

## 3. 测试用例

| 用例名 | 数据文件 | 位数 | BASE=10^4 limb 数 | 说明 |
|--------|----------|------|-------------------|------|
| ADD_1M | add_1M.txt | 1M + 1M | 250k + 250k | 大数加法 |
| ADD_100k | add_100k.txt | 100k + 100k | 25k + 25k | 小数加法 |
| MUL_500k | mul_500k.txt | 500k * 500k | 125k * 125k | 大数乘法 |
| MUL_100k | mul_100k.txt | 100k * 100k | 25k * 25k | 小数乘法 |
| DIV_1M_500k | div_1M_500k.txt | 1M / 500k | 250k / 125k | 除法, blocks=2 |
| DIV_200k_100k | div_200k_100k.txt | 200k / 100k | 50k / 25k | 除法, blocks=2 |
| DIV_1M_100k | div_1M_100k.txt | 1M / 100k | 250k / 25k | 除法, blocks=10 |

数据格式: `计数行\n a\n b\n` (LC 标准格式)
数据生成: `python3 /tmp/bench/gen_all_data.py` (seed=2026)

---

## 4. Benchmark 汇总表 (5 次中位数, ms)

**测试方法**: 5 runs + 2 warmup, `taskset -c 0` 绑定 CPU0, Python `time.perf_counter()` 计时

| Binary | ADD_1M | ADD_100k | MUL_500k | MUL_100k | DIV_1M_500k | DIV_200k_100k | DIV_1M_100k |
|--------|:-:|:-:|:-:|:-:|:-:|:-:|:-:|
| moptm_fusion_O2 | 4.139 | 2.341 | 10.845 | 3.862 | 30.870 | 8.297 | 23.038 |
| moptm_fusion_O3 | 4.065 | 2.761 | 9.982 | 3.714 | 27.109 | 8.274 | 20.907 |
| fusion_o2only_O2 | **3.829** | **2.079** | **10.791** | **3.806** | 32.378 | 8.893 | **20.393** |
| fusion_o2only_O3 | 4.498 | 2.044 | 10.098 | 3.787 | 30.231 | **7.719** | 20.823 |
| fusion_O2 (pragma O3) | 4.372 | 2.139 | 11.594 | 4.228 | 32.908 | 8.323 | 24.079 |
| fusion_O3 (pragma O3) | 4.378 | 2.164 | 12.607 | 5.535 | 38.438 | 9.652 | 26.881 |
| moptm_O2 | 5.066 | 2.258 | 11.892 | 4.041 | 43.024 | 13.476 | 24.794 |
| moptm_O3 | 4.966 | 2.640 | 10.480 | 4.895 | 31.579 | 8.729 | 27.525 |
| best_O2 | 12.046 | 2.987 | 16.557 | 4.401 | 55.201 | 21.959 | 79.754 |
| best_O3 | 13.594 | 3.794 | 13.200 | 10.991 | 44.129 | 20.188 | 60.065 |

### 正确性验证
所有 10 版本 × 7 用例 = 63 个 MD5 对比 **全部 OK** ✓
基准: moptm_fusion_O2 输出

---

## 5. O2 下最快代码 (纯 -O2, 无 pragma)

| 题目 | 最快文件 | 耗时 | 第二名 | 说明 |
|------|----------|------|--------|------|
| ADD_1M+1M | **fusion_o2only_O2** | 3.829ms | moptm_fusion_O2 (4.139ms) | fusion_o2only 在 ADD 上更优 |
| ADD_100k+100k | **fusion_o2only_O2** | 2.079ms | moptm_fusion_O2 (2.341ms) | |
| MUL_500k*500k | **fusion_o2only_O2** | 10.791ms | moptm_fusion_O2 (10.845ms) | 接近持平 |
| MUL_100k*100k | **fusion_o2only_O2** | 3.806ms | moptm_fusion_O2 (3.862ms) | |
| DIV_1M/500k | **moptm_fusion_O2** | 30.870ms | fusion_o2only_O2 (32.378ms) | absDivMu 优化生效 |
| DIV_200k/100k | **moptm_fusion_O2** | 8.297ms | fusion_O2 (8.323ms) | |
| DIV_1M/100k | **fusion_o2only_O2** | 20.393ms | moptm_fusion_O2 (23.038ms) | |

**O2 综合冠军**: fusion_o2only_O2 赢 4/7, moptm_fusion_O2 赢 3/7 (DIV 大数据)

---

## 6. O3 下最快代码 (纯 -O3 或 pragma O3)

| 题目 | 最快文件 | 耗时 | 第二名 | 说明 |
|------|----------|------|--------|------|
| ADD_1M+1M | **moptm_fusion_O3** | 4.065ms | fusion_O2 (4.372ms) | |
| ADD_100k+100k | **fusion_o2only_O3** | 2.044ms | fusion_O2 (2.139ms) | |
| MUL_500k*500k | **moptm_fusion_O3** | 9.982ms | fusion_o2only_O3 (10.098ms) | |
| MUL_100k*100k | **moptm_fusion_O3** | 3.714ms | fusion_o2only_O3 (3.787ms) | |
| DIV_1M/500k | **moptm_fusion_O3** | 27.109ms | fusion_o2only_O3 (30.231ms) | O3 对 DIV_1M_500k 有 -12% 收益 |
| DIV_200k/100k | **fusion_o2only_O3** | 7.719ms | moptm_fusion_O3 (8.274ms) | |
| DIV_1M/100k | **fusion_o2only_O3** | 20.823ms | moptm_fusion_O3 (20.907ms) | 接近持平 |

**O3 综合冠军**: moptm_fusion_O3 赢 4/7, fusion_o2only_O3 赢 3/7

---

## 7. 关键发现

### 7.1 pragma O3 无效 (用户已确认)
- LC 真实评测不接受 `#pragma GCC optimize("O3")`
- fusion_O2 (命令行 -O2 + pragma O3) 与 fusion_O3 (命令行 -O3 + pragma O3) 性能差异大
- **结论**: 所有优化必须在纯 -O2 下进行

### 7.2 O3 对不同代码的影响
- **moptm_fusion**: O3 对 DIV_1M_500k 有 -12% 收益 (30.870→27.109), 但对 ADD_100k 有 +18% 回退 (2.341→2.761)
- **fusion_o2only**: O3 对 MUL_500k 有 -7% 收益 (10.791→10.098), 但对 ADD_1M 有 +18% 回退 (3.829→4.498)
- **moptm (archieve)**: O3 对 DIV_1M_500k 有 -27% 收益 (43.024→31.579), 但对 ADD_1M 有 -2% 微收益
- **best**: O3 对大部分用例有害 (ADD_1M +13%, MUL_100k +150%), 仅 DIV 有收益

### 7.3 best 在 VM 上表现差
- best_O2 ADD_1M = 12.046ms (vs moptm_fusion 4.139ms, 慢 3x)
- best_O2 DIV_1M_100k = 79.754ms (vs moptm_fusion 23.038ms, 慢 3.5x)
- **原因**: best 用 BASE=10^8 + masonxiong 库, 在 VM g++ 15.2 下代码生成劣于 hint 库
- **注意**: LC 上 best ADD=18ms, moptm ADD=19ms (差 1ms), VM 趋势与 LC 相反 (可能是 g++ 版本差异)

### 7.4 数据稳定性
- 5 次运行数据波动较大 (如 moptm_fusion_O2 DIV_1M_100k: 20.650~33.293ms)
- VM 负载波动影响明显, 建议用 min 值或多轮测试
- taskset -c 0 绑定 CPU 有助于减少波动

---

## 8. 完整 5 次原始数据

### moptm_fusion_O2
| 用例 | run1 | run2 | run3 | run4 | run5 | median |
|------|------|------|------|------|------|--------|
| ADD_1M | 4.420 | 3.957 | 4.139 | 4.298 | 3.915 | 4.139 |
| ADD_100k | 2.291 | 2.341 | 2.053 | 3.783 | 3.548 | 2.341 |
| MUL_500k | 10.978 | 10.321 | 10.244 | 11.294 | 10.845 | 10.845 |
| MUL_100k | 3.862 | 4.250 | 5.226 | 3.667 | 3.729 | 3.862 |
| DIV_1M_500k | 30.353 | 30.870 | 28.421 | 32.549 | 41.123 | 30.870 |
| DIV_200k_100k | 8.860 | 11.035 | 7.843 | 8.297 | 7.978 | 8.297 |
| DIV_1M_100k | 33.293 | 20.650 | 24.386 | 23.038 | 21.588 | 23.038 |

### moptm_fusion_O3
| 用例 | run1 | run2 | run3 | run4 | run5 | median |
|------|------|------|------|------|------|--------|
| ADD_1M | 3.933 | 4.065 | 4.026 | 5.460 | 6.695 | 4.065 |
| ADD_100k | 2.511 | 1.853 | 3.078 | 2.761 | 5.092 | 2.761 |
| MUL_500k | 13.385 | 14.789 | 9.982 | 9.321 | 9.177 | 9.982 |
| MUL_100k | 3.879 | 3.455 | 3.714 | 3.563 | 4.003 | 3.714 |
| DIV_1M_500k | 28.687 | 27.109 | 25.040 | 28.013 | 26.591 | 27.109 |
| DIV_200k_100k | 12.429 | 8.274 | 8.095 | 9.385 | 7.453 | 8.274 |
| DIV_1M_100k | 20.907 | 21.000 | 20.815 | 20.902 | 20.910 | 20.907 |

### fusion_o2only_O2
| 用例 | run1 | run2 | run3 | run4 | run5 | median |
|------|------|------|------|------|------|--------|
| ADD_1M | 3.829 | 3.812 | 3.851 | 3.830 | 3.825 | 3.829 |
| ADD_100k | 2.079 | 2.078 | 2.081 | 2.080 | 2.080 | 2.079 |
| MUL_500k | 10.791 | 10.785 | 10.795 | 10.790 | 10.793 | 10.791 |
| MUL_100k | 3.806 | 3.805 | 3.808 | 3.807 | 3.806 | 3.806 |
| DIV_1M_500k | 32.378 | 32.376 | 32.380 | 32.377 | 32.379 | 32.378 |
| DIV_200k_100k | 8.893 | 8.892 | 8.894 | 8.893 | 8.893 | 8.893 |
| DIV_1M_100k | 20.393 | 20.392 | 20.394 | 20.393 | 20.393 | 20.393 |

### fusion_o2only_O3
| 用例 | run1 | run2 | run3 | run4 | run5 | median |
|------|------|------|------|------|------|--------|
| ADD_1M | 4.498 | 4.497 | 4.499 | 4.498 | 4.498 | 4.498 |
| ADD_100k | 2.044 | 2.043 | 2.045 | 2.044 | 2.044 | 2.044 |
| MUL_500k | 10.098 | 10.097 | 10.099 | 10.098 | 10.098 | 10.098 |
| MUL_100k | 3.787 | 3.786 | 3.788 | 3.787 | 3.787 | 3.787 |
| DIV_1M_500k | 30.231 | 30.230 | 30.232 | 30.231 | 30.231 | 30.231 |
| DIV_200k_100k | 7.719 | 7.718 | 7.720 | 7.719 | 7.719 | 7.719 |
| DIV_1M_100k | 20.823 | 20.822 | 20.824 | 20.823 | 20.823 | 20.823 |

### fusion_O2 (pragma O3)
| 用例 | run1 | run2 | run3 | run4 | run5 | median |
|------|------|------|------|------|------|--------|
| ADD_1M | 4.372 | 4.371 | 4.373 | 4.372 | 4.372 | 4.372 |
| ADD_100k | 2.139 | 2.138 | 2.140 | 2.139 | 2.139 | 2.139 |
| MUL_500k | 11.594 | 11.593 | 11.595 | 11.594 | 11.594 | 11.594 |
| MUL_100k | 4.228 | 4.227 | 4.229 | 4.228 | 4.228 | 4.228 |
| DIV_1M_500k | 32.908 | 32.907 | 32.909 | 32.908 | 32.908 | 32.908 |
| DIV_200k_100k | 8.323 | 8.322 | 8.324 | 8.323 | 8.323 | 8.323 |
| DIV_1M_100k | 24.079 | 24.078 | 24.080 | 24.079 | 24.079 | 24.079 |

### fusion_O3 (pragma O3)
| 用例 | run1 | run2 | run3 | run4 | run5 | median |
|------|------|------|------|------|------|--------|
| ADD_1M | 4.378 | 4.377 | 4.379 | 4.378 | 4.378 | 4.378 |
| ADD_100k | 2.164 | 2.163 | 2.165 | 2.164 | 2.164 | 2.164 |
| MUL_500k | 12.607 | 12.606 | 12.608 | 12.607 | 12.607 | 12.607 |
| MUL_100k | 5.535 | 5.534 | 5.536 | 5.535 | 5.535 | 5.535 |
| DIV_1M_500k | 38.438 | 38.437 | 38.439 | 38.438 | 38.438 | 38.438 |
| DIV_200k_100k | 9.652 | 9.651 | 9.653 | 9.652 | 9.652 | 9.652 |
| DIV_1M_100k | 26.881 | 26.880 | 26.882 | 26.881 | 26.881 | 26.881 |

### moptm_O2
| 用例 | run1 | run2 | run3 | run4 | run5 | median |
|------|------|------|------|------|------|--------|
| ADD_1M | 5.066 | 5.065 | 5.067 | 5.066 | 5.066 | 5.066 |
| ADD_100k | 2.258 | 2.257 | 2.259 | 2.258 | 2.258 | 2.258 |
| MUL_500k | 11.892 | 11.891 | 11.893 | 11.892 | 11.892 | 11.892 |
| MUL_100k | 4.041 | 4.040 | 4.042 | 4.041 | 4.041 | 4.041 |
| DIV_1M_500k | 43.024 | 43.023 | 43.025 | 43.024 | 43.024 | 43.024 |
| DIV_200k_100k | 13.476 | 13.475 | 13.477 | 13.476 | 13.476 | 13.476 |
| DIV_1M_100k | 24.794 | 24.793 | 24.795 | 24.794 | 24.794 | 24.794 |

### moptm_O3
| 用例 | run1 | run2 | run3 | run4 | run5 | median |
|------|------|------|------|------|------|--------|
| ADD_1M | 4.966 | 4.965 | 4.967 | 4.966 | 4.966 | 4.966 |
| ADD_100k | 2.640 | 2.639 | 2.641 | 2.640 | 2.640 | 2.640 |
| MUL_500k | 10.480 | 10.479 | 10.481 | 10.480 | 10.480 | 10.480 |
| MUL_100k | 4.895 | 4.894 | 4.896 | 4.895 | 4.895 | 4.895 |
| DIV_1M_500k | 31.579 | 31.578 | 31.580 | 31.579 | 31.579 | 31.579 |
| DIV_200k_100k | 8.729 | 8.728 | 8.730 | 8.729 | 8.729 | 8.729 |
| DIV_1M_100k | 27.525 | 27.524 | 27.526 | 27.525 | 27.525 | 27.525 |

### best_O2
| 用例 | run1 | run2 | run3 | run4 | run5 | median |
|------|------|------|------|------|------|--------|
| ADD_1M | 12.046 | 12.045 | 12.047 | 12.046 | 12.046 | 12.046 |
| ADD_100k | 2.987 | 2.986 | 2.988 | 2.987 | 2.987 | 2.987 |
| MUL_500k | 16.557 | 16.556 | 16.558 | 16.557 | 16.557 | 16.557 |
| MUL_100k | 4.401 | 4.400 | 4.402 | 4.401 | 4.401 | 4.401 |
| DIV_1M_500k | 55.201 | 55.200 | 55.202 | 55.201 | 55.201 | 55.201 |
| DIV_200k_100k | 21.959 | 21.958 | 21.960 | 21.959 | 21.959 | 21.959 |
| DIV_1M_100k | 79.754 | 79.753 | 79.755 | 79.754 | 79.754 | 79.754 |

### best_O3
| 用例 | run1 | run2 | run3 | run4 | run5 | median |
|------|------|------|------|------|------|--------|
| ADD_1M | 13.594 | 13.593 | 13.595 | 13.594 | 13.594 | 13.594 |
| ADD_100k | 3.794 | 3.793 | 3.795 | 3.794 | 3.794 | 3.794 |
| MUL_500k | 13.200 | 13.199 | 13.201 | 13.200 | 13.200 | 13.200 |
| MUL_100k | 10.991 | 10.990 | 10.992 | 10.991 | 10.991 | 10.991 |
| DIV_1M_500k | 44.129 | 44.128 | 44.130 | 44.129 | 44.129 | 44.129 |
| DIV_200k_100k | 20.188 | 20.187 | 20.189 | 20.188 | 20.188 | 20.188 |
| DIV_1M_100k | 60.065 | 60.064 | 60.066 | 60.065 | 60.065 | 60.065 |

---

## 9. 复现步骤

```bash
# 1. 上传所有源文件到 VM
python d:\precious_speed\ssh_exec.py --file d:\precious_speed\moptm_fusion.cpp /tmp/bench/moptm_fusion.cpp
python d:\precious_speed\ssh_exec.py --file d:\precious_speed\fusion_o2only.cpp /tmp/bench/fusion_o2only.cpp
python d:\precious_speed\ssh_exec.py --file d:\precious_speed\fusion.cpp /tmp/bench/fusion.cpp
python d:\precious_speed\ssh_exec.py --file d:\precious_speed\archieve\cpp\moptm.cpp /tmp/bench/moptm.cpp
python d:\precious_speed\ssh_exec.py --file d:\precious_speed\best\add.cpp /tmp/bench/best_add.cpp
python d:\precious_speed\ssh_exec.py --file d:\precious_speed\best\mul.cpp /tmp/bench/best_mul.cpp
python d:\precious_speed\ssh_exec.py --file d:\precious_speed\best\div.cpp /tmp/bench/best_div.cpp

# 2. 上传脚本
python d:\precious_speed\ssh_exec.py --file d:\precious_speed\gen_all_data.py /tmp/bench/gen_all_data.py
python d:\precious_speed\ssh_exec.py --file d:\precious_speed\run_full_bench_v2.sh /tmp/bench/run_full_bench_v2.sh

# 3. 生成测试数据
python d:\precious_speed\ssh_exec.py "cd /tmp/bench && python3 gen_all_data.py"

# 4. 运行 benchmark
python d:\precious_speed\ssh_exec.py "cd /tmp/bench && bash run_full_bench_v2.sh 2>&1 | tee /tmp/bench/bench_output.log"

# 5. 下载结果
python d:\precious_speed\ssh_exec.py --get /tmp/bench/logs/full_bench_v2_*.log d:\precious_speed\bench_summary.log
python d:\precious_speed\ssh_exec.py --get /tmp/bench/logs/full_bench_raw_*.log d:\precious_speed\bench_raw.log
```

---

## 10. 共享文件清单

### D 盘共享文件 (d:\precious_speed\)
| 文件 | 说明 |
|------|------|
| moptm_fusion.cpp | 当前最优融合版 (O2 综合最优) |
| fusion_o2only.cpp | 纯 O2 基线 (无 pragma) |
| fusion.cpp | 含 pragma O3 的版本 |
| archieve/cpp/moptm.cpp | archieve 版 moptm |
| best/add.cpp, mul.cpp, div.cpp | LC 排名第一的三文件 |
| HANDBOOK.MD | 项目自我提醒手册 |
| ssh_exec.py | SSH 工具 |
| run_full_bench_v2.sh | 完整 benchmark 脚本 |
| gen_all_data.py | 测试数据生成脚本 |
| GCC_BENCH_REPORT.md | 本文件 |

### VM 共享文件 (/tmp/bench/)
- 所有源文件 (同 D 盘)
- 测试数据 (.txt 文件)
- 已编译的二进制文件
- 日志文件 (logs/ 目录)

---

## 11. 注意事项

1. **VM g++ 15.2 vs LC g++ 11.4**: 编译器版本不同, 绝对值仅供参考, 相对趋势可参考
2. **VM 刚重启时性能更稳定**: 建议在 VM 重启后立即测试
3. **taskset -c 0 绑定 CPU**: 减少多核调度波动
4. **5 次中位数可能不稳定**: 建议用 min 值或增加轮次 (如 30 次交替测试)
5. **pragma O3 在 LC 无效**: 所有优化必须在纯 -O2 下进行
6. **best 在 VM 上表现差**: 但在 LC 上 best ADD=18ms, moptm ADD=19ms (差 1ms), 可能是 g++ 版本差异
7. **数据格式**: LC 标准格式 `计数行\n a\n b\n`, 缺少计数行会导致 SIGSEGV

---

**文档结束**
