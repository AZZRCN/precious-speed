# HEX div — fresh perf hotspots（instructions:u 唯一真值）

- 平台：VM `192.168.1.55` / azzr@192.168.1.55（VMware Ubuntu 7.0, g++-15 15.2.0, perf 在位）
- 编译：`g++-15 -O3 -std=c++20 -march=znver3 -mtune=znver3 -g -fno-omit-frame-pointer -w`
- 目标：`div_base16_zn.cpp`（= P0 主源 `div_base16.cpp`, 354275B, 未污染）
- 输入：`/tmp/one_case.txt`（单用例, 400k / 206k limbs）
- 指标：`perf stat -e instructions:u`（x86-64 指令数 Intel/AMD 一致 = EPYC 7B13 真值；铁律不测墙钟/cycles）

## 1. 单用例指令数基线
```
REPS=1:  426,360,266 instructions:u   (~0.24s wall)
```

## 2. 热点分布（REPS 放大除法内核, self-overhead, perf record -e instructions:u -F 990）

| 排名 | 函数 | self% | 类别 | rdot 融合可降 instructions:u？ |
|---|---|---|---|---|
| 1 | `FFT<double>::difC4<0>` | 26.83% | 前向 FFT（AVX2 向量化） | 否 |
| 2 | `FFT<double>::iditC4<0>` | 15.23% | 逆变换（AVX2 向量化） | 否 |
| 3 | `FFT<double>::difDispatch<false>` | 12.53% | 前向 FFT 分发 | 否 |
| 4 | `Integer::carryPropSeg` | 9.73% | 进位传播（cache-miss 第 1） | 否（仅降 cycles, 不降指令） |
| 5 | `real_dot_binrev3<double>` | **6.77%** | **rdot 重建（fft3）** | **否（强制频域点乘, 融合仅省带宽）** |
| 6 | `FFT<double>::difSmall<false>` | 5.79% | 前向 FFT 小长度 | 否 |
| 7 | `FFT<double>::iditDispatch<false>` | 5.38% | 逆变换分发 | 否 |
| 8 | `dif3StageC4<true>` | 3.85% | 前向 radix-3（AVX2） | 否 |
| 9 | `Integer::absSub` | 3.25% | 大整数减法 | 否 |
| 10 | `Integer::absInvNewton` | 2.15% | Newton 倒数 | 否 |
| 11 | `idit3StageC4<true>` | 1.98% | 逆 radix-3 | 否 |
| 12 | `FFT<double>::iditSmall<false>` | 1.98% | 逆变换 小长度 | 否 |
| 13 | `copyU16ToF64AndFill` | 1.85% | 输入 limb→double 转换 | 否 |
| 14 | `Integer::absAdd` | 1.15% | 大整数加法 | 否 |
| 15 | `real_dot_binrev2<double>` | 0.53% | rdot 重建（2-power） | 否 |

top-15 合计 ≈ 94.6%。

## 3. 结论（实测侧坐实构造定理）

- **唯一与 rdot 相关的热点** = `real_dot_binrev3`(6.77%) + `real_dot_binrev2`(0.53%) = 7.3%。
  二者是 real-FFT 卷积的**强制频域点乘打包**（Hermitian 对称自乘 + 1/N 缩放），算术不可消除。
- **融合只省内存往返、不省指令**：把它从独立遍历挪进 `idit` 蝶形循环，总重建层数 = 总蝶形级数完全不变，
  指令条数对拍一致；省下的只是 cache 带宽（`instructions:u` 不测带宽 → 零收益）。
- top-15 中**其余全不沾 rdot**：FFT 主体（AVX2 已向量化最优）、进位（cache-bound 仅 cycles）、
  `absSub/absAdd/absInvNewton`（大整数运算）。→ **没有任何热点可借 rdot 融合降指令数**。
- 与先前 #14 / #18 预测画像**逐行吻合**，实测与构造分析双重证伪 rdot 融合路径。

→ **P0（-2.74%）是 HEX div 在 `instructions:u` 真值下的硬收口线**。`div_base16_rdot.cpp` 为失败 PoC，归档。
