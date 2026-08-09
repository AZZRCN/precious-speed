# precious-speed

High-performance arbitrary-precision integer arithmetic library, optimized for [Library Checker](https://judge.yosupo.jp/) big-integer problems.

高性能大整数运算库，针对 [Library Checker](https://judge.yosupo.jp/) 大整数问题优化。

## Results / 成绩

| Problem / 问题 | Best / 最优 | Submission / 提交 | Date / 日期 |
|---|---|---|---|
| [Division of Big Integers](https://judge.yosupo.jp/problem/division_of_big_integers) | **108 ms** (#1) | #385741 | 2026-07-16 |
| [Multiplication of Big Integers](https://judge.yosupo.jp/problem/multiplication_of_big_integers) | **36 ms** | #385663 | 2026-07-15 |
| [Addition of Big Integers](https://judge.yosupo.jp/problem/addition_of_big_integers) | **18 ms** | #385662 | 2026-07-15 |

> Division submission #385741 ranked **#1** on the leaderboard at submission time.
>
> 除法提交 #385741 在提交时排名**第一名**。

## Files / 文件

```
.
├── best/                      # Best LC submissions / 最优 LC 提交
│   ├── add.cpp                # Addition (18 ms) / 加法
│   ├── mul.cpp                # Multiplication (36 ms) / 乘法
│   └── div.cpp                # Division (108 ms, #1) / 除法
├── moptm.cpp                  # Hybrid: masonxiong_opt + Core2 chunked division / 混合版
├── masonxiong_opt.cpp         # Base: AVX2/FMA FFT + Newton-Raphson division / 基线版
└── README.md
```

## Build / 编译

Requires a C++20 compiler with AVX2 + FMA support (e.g. GCC 13+).

需要支持 AVX2 + FMA 的 C++20 编译器（如 GCC 13+）。

```bash
g++ -O2 -mavx2 -mfma -funroll-loops -o solution best/div.cpp
```

The code auto-detects the platform: on Linux it uses `mmap` for input and `fwrite` for output; on Windows it uses `fread` for input.

代码自动检测平台：Linux 下使用 `mmap` 输入 + `fwrite` 输出；Windows 下使用 `fread` 输入。

## Acknowledgements / 致谢

This project builds upon the work of:

本项目基于以下作者的工作：

- **[masonxiong](https://www.luogu.com.cn/user/446979)** & **[yuygfgg](https://www.luogu.com.cn/user/251551)** — `masonxiong_opt.cpp` base library (AVX2/FMA complex FFT, Newton-Raphson division, unbalanced multiplication split)
- **[With-Sky](https://github.com/With-Sky/HyperInt-mini)** — `HyperInt-mini` library, basis of the division optimization (`best/div.cpp`)
- **[AZZRCN](https://github.com/AZZRCN)** — Core2 chunked division path, high-speed I/O, and integration work

## License / 许可

See source file headers for original authorship. Optimization code by AZZRCN.
