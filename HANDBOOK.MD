# HANDBOOK - precious_speed O3 开发周期

> **用途**：防止对话压缩导致上下文丢失。每次新会话或上下文被压缩时，先读本文件。
> **更新原则**：每完成一个里程碑就追加一节，不要删除历史记录。

---

## 0. 通用规则

- **参考资料查找**：如果遇到参考资料的问题，请先去 `E:\` 找一圈（根目录及子目录），再考虑网络搜索或问用户。E:\ 盘可能存有历史代码、论文 PDF、相关工具等参考资料。
- **O2 HANDBOOK**：`archieve/docs/HANDBOOK_O2.md` 保存了 O2 周期的完整优化记录。

---

## 1. 项目目标

在 LC (Library Checker, judge.yosupo.jp) 真实评测约束下，让单一文件在 ADD/MUL/DIV 三个问题上同时达到或超越 `best/` 目录下三个独立文件的成绩。

### LC 评测环境（详见 limits.md）
- 编译：`g++ -x c++ -O2 -std=gnu++20 -static -ONLINE_JUDGE`
- **无 -mavx2**（默认不启用高级指令集，需 `#pragma GCC target` 显式开启）
- 硬件：GCP c2-standard-4, Intel Cascade Lake, AVX2+FMA+AVX-512
- 计分：CPU 时间（用户态+内核态总和），非墙钟时间
- 内存：2GB
- 单核调度（多线程无加速收益）
- 源码 ≤ 256KB，编译 ≤ 30s

### 三题地址
- ADD: https://judge.yosupo.jp/problem/addition_of_big_integers
- MUL: https://judge.yosupo.jp/problem/multiplication_of_big_integers
- DIV: https://judge.yosupo.jp/problem/division_of_big_integers

### O2 最终战绩（2026-07-22）

| 题目 | 提交ID | 耗时 | 排名 | best 耗时 |
|---|---|---|---|---|
| ADD | #387304 | **16 ms** | **#1** | 18ms |
| MUL | #387374 | **36 ms** | **#1 (tied)** | 36ms |
| DIV | #387301 | **87 ms** | **#1** | 108ms |

**O2 三题全部 #1。**

---

## 2. O3 开发周期目标

- 在 O2 成果基础上，探索 O3 编译优化带来的性能提升
- 用户既定路线："先开发 O2，O2 到极限之后再开发 O3"
- O3 预扩展产物是"全球独一份的小礼物"（用户原话）
- 注意：`#pragma GCC optimize("O3")` 在 LC 无效，需用命令行 -O3 或其他方式

### O3 关键约束
- LC 编译命令固定为 `-O2`，`#pragma GCC optimize("O3")` 实测无效
- `#pragma GCC target("avx2,bmi,bmi2,popcnt,lzcnt")` 可用（控制指令集）
- O3 优化需通过代码层面手工展开/向量化实现，而非依赖编译器开关

---

## 3. 工作区结构

```
d:\precious_speed\
├── best/              # 融合源头（只读参考）
│   ├── add.cpp        # best ADD 18ms
│   ├── mul.cpp        # best MUL 36ms
│   └── div.cpp        # best DIV 108ms
├── add.cpp            # O2 ADD #1 (16ms)
├── mul.cpp            # O2 MUL #1 tied (36ms)
├── div.cpp            # O2 DIV #1 (87ms)
├── fast_io.cpp        # 快读快写独立 benchmark
├── toolbox/           # 自定义工具箱（tbx.exe, sam/es/split 命令）
├── archieve/          # 已归档的历史文件
│   ├── cpp/           # 旧 .cpp/.hpp/.s
│   ├── scripts/       # 旧 .sh/.py
│   └── docs/          # 旧交接文档 + HANDBOOK_O2.md
├── scripts/           # 当前脚本
├── HANDBOOK.MD        # 本文件
├── ssh_exec.py        # SSH 工具（在 scripts/ 下）
├── limits.md          # LC 环境清单
└── .gitignore
```

---

## 4. 虚拟机访问

- **SSH**: `192.168.1.55`
- **用户**: `azzr`
- **密码**: `1234`
- **系统**: Ubuntu 7.0.0-28-generic, 2 核
- **CPU**: Intel i7-11370H @ 3.30GHz (Tiger Lake, AVX2+FMA+AVX512)

### SSH 工具用法
```powershell
# 执行命令
python d:\precious_speed\scripts\ssh_exec.py "cmd1" "cmd2"
# 上传文件
python d:\precious_speed\scripts\ssh_exec.py --file local_path remote_path
# 下载文件
python d:\precious_speed\scripts\ssh_exec.py --get remote_path local_path
```

---

## 5. O2 关键技术成果（供 O3 参考）

### 算法
- **DIV**: GMP 风格 mu_div_qr 近似逆分块除法，cyclic (mod B^m-1) 卷积加速
- **MUL**: basicMul BASE=10^8 打包 (4x less bruteforce work)，FFT AlignedVec32 twiddle + assume_aligned
- **ADD**: int64 快速路径 (<=18 digit)，SWAR V7 parse (rogeryoungh swar_64 风格)

### I/O
- mmap 输入 + MADV_SEQUENTIAL|WILLNEED
- mmap oBuffer + MADV_HUGEPAGE (32MB)
- write() syscall 替代 fwrite (flushOutput)
- V7 SWAR parse: 8字节 umask + ctz + shift-align

### 已知瓶颈
- **MUL**: FFT carry chain serial dependency（真正瓶颈，非除法指令）
- **MUL**: BASE=10^4 vs best BASE=10^8 结构性劣势（limb 数 2x）
- **DIV**: absInvNewton 占 50%+（递归常数因子高）
- **ADD**: 小数据 per-call 开销（parse + write 累积）

### O3 可能方向
1. 手工 radix-8/split-radix FFT 蝶形（减少指令数）
2. 递归 FFT → 迭代 FFT（消除递归开销，改善向量化的 memory clobber）
3. carry chain SIMD 并行化（多 limb 同时进位）
4. 查表法/跳表法替代分支
5. 内联关键路径函数（absInvNewton, absDivMu blocks loop）

---

## 6. 提交节奏

- LC 提交需 5 秒间隔，避免违规检测
- 用户偏好：记录 problem ID、result、score、Record ID
- 提交文件：add.cpp / mul.cpp / div.cpp（已拆分为三个独立文件）
