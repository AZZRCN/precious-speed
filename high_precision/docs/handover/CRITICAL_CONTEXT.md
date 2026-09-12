# Critical Context (防止 AI 上下文压缩丢失)

## Architecture
- `moptm.cpp`（~2620 行）单文件 OJ 提交
- BASE=10000, uint16_t Limb, uint32_t Limb2, radix-4 auto-vec double FFT (Complex2<Float> packs 2 complex/SIMD)
- 三模式 via `-DHINT_OP_ADD/MUL/DIV`
- I/O: `cin >> string` (static reuse) + oBuffer (32MB) + `fwrite` 末尾一次性输出
- 编译: `g++ -O3 -mavx2 -mfma -funroll-loops`

## Current Performance (10-run median, vs best)
- **ADD 1M+1M:** 14.0ms (best 19.5, **moptm 0.72×** → 领先 28%)
- **MUL 500k*500k:** 16.5ms (best 23.0, **moptm 0.72×** → 领先 28%)
- **DIV 1M/500k:** 39.8ms (best 37.7, **moptm 1.06×** → 落后 6%)

## Applied Optimizations (chronological)
1. DIV FFT 阈值 blocks>=3 → blocks>=2 (已提交 4296f96)
2. `operator>>` static string 复用
3. `fromString` → `fromCharRange` 重构
4. `add_half/sub_half` bool& → Limb& + 三元运算
5. `absAdd/absSub` 4× 循环展开 + Limb 进位
6. FFT 进位循环 4× 展开 (fftMul/fftSqr/fftMulPre)
7. `operator+/operator*` NRVO 避免返回时拷贝（拆成 lhs+=rhs; return lhs）
8. ADD/MUL main: `a + b` → `a += b`, `a * b` → `a *= b`（省一次参数拷贝）

## Failed Attempts (don't repeat)
- **fread 全量读入**: Windows 上反而慢（原因不明，疑似 page fault / CRT 缓冲问题），比 cin>> 慢 3×
- **近似逆 (GMP 方式)**: 3 种方案均未通过正确性验证；函数 `absDivNewtonWithInvLoose` 存在但不调用
- **无 profile 工具**: 只能跑 bench.exe 盲测，无法精确定位热点

## Key Files
- `moptm.cpp` — 主工作文件
- `best/add.cpp,mul.cpp,div.cpp` — LC #1 参考实现
- `docs/handover/HANDOVER_GMP_APPROX_INV.md` — GMP 近似逆推导
- `bench.cpp` — 基准测试框架 (auto-compile + 10-run median + cmp)
- `gen_data.cpp` — 测试数据生成器
- `mason_opt/` — 旧实验目录（含 div_work.cpp, div_1st.cpp, masonxiong_opt.cpp 等）
- `toolbox/` — AI Prompt 工具集（不相关）

## Potential Next Directions (未尝试)
1. **`__m256d` explicit butterfly**: best 用的手动 __m256d 蝶形，可能比 auto-vec 快
2. **内存池**: best 有 size-class DigitAllocator，避免 malloc/free
3. **mmap I/O**: best 支持 mmap 读入（但 Windows 上可能不适用）
4. **增大 BASE**: best 用 BASE=10^8 (uint32_t)，limb 数减半，FFT 尺寸减半
5. **展开 DIF 内层循环**: FFT 核心循环可进一步手工 SIMD 化
6. **basicMul 8× 展开**: 高分位数乘法可尝试更大展开因子
7. **Newton DIV 近似逆**: 重新尝试，重点是 inv0 精度要够（>= dn+2 bits）

## Benchmark Method
```bash
gen_data.exe all <seed>   # 生成测试数据
bench.exe add --rebuild   # 全量重新编译+跑分
bench.exe cmp add         # 正确性验证
```

## 重要提醒
- 本地机器比 LC 服务器慢；moptm/best 相对比例才有意义
- bench.exe 用 CreateProcess + 文件重定向 stdin，不经过 pipe
- 每次优化后要同时跑 cmp（正确性）和 bench（性能），确保不退化
