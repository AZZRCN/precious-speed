# precious-speed

High-performance C++ big integer library with AVX2/FMA-optimized FFT multiplication and fused Newton division.

## Credits & License

**Original library by**: [masonxiong](https://www.luogu.com.cn/user/446979) and [yuygfgg](https://www.luogu.com.cn/user/251551)

**Original source**: https://www.luogu.com.cn/article/c70gmlxt (published 2025-09-01)

**This repository** is an optimized fork by [AZZRCN](https://github.com/AZZRCN), with performance improvements including AVX2/FMA-accelerated FFT, fused Newton division, dedicated square path, and memory pool optimization.

All credit for the original design and implementation goes to masonxiong and yuygfgg. This optimized version is shared for the benefit of the competitive programming community. If you are the original author and have any concerns, please open an issue.

## Overview

`masonxiong_opt.cpp` is an optimized big integer arithmetic library supporting arbitrary-precision integer operations. The core innovation is a **complex-number FFT** multiplication engine accelerated with **AVX2/FMA intrinsics**, combined with a **fused Newton-Raphson division** that merges shift + double + subtract into a single pass.

Base = 10^8, with configurable thresholds for bruteforce/FFT crossover.

## Performance

Benchmarked against GMP 6.3.0, Boost.Multiprecision 1.89.0, and the original unoptimized version. All tests use identical random inputs (seed=42) and `chrono::high_resolution_clock` timing.

### 1M-digit single-round (ms, lower is better)

| Operation | ORIG | **OPT** | GMP | Boost |
|-----------|------|---------|-----|-------|
| mul       | 10.77 | **5.54**  | 17.34 | 353.7 |
| sqr       | 10.87 | **5.44**  | 17.30 | 360.0 |
| div2      | 83.45 | **38.60** | 55.92 | 9174.9 |

### Key results

- **OPT is fastest in all operations at 100K-1M scale**, no exceptions
- **2.0x faster than ORIG** across all operations (validates optimization effectiveness)
- **1.45x-3.18x faster than GMP** at 1M scale
- **64-80x faster than Boost** at 1M scale

## Key optimizations

| Optimization | Impact | Description |
|-------------|--------|-------------|
| AVX2 butterfly (`__m256d`) | mul/sqr | FFT butterfly processes 2 complex numbers per instruction |
| Dedicated square path | sqr | Saves one DIF + half pointwise multiply |
| Unbalanced mul split + DIF cache | 2n*n mul | FFT length 4n->2n, cache-friendly |
| Fused Newton iteration | div2 | Merges shift+double+subtract into single operation |
| Truncate dividend low digits | div2 | Reduces multiply size when shiftBack==0 |
| Memory pool (size-class buckets) | all | Avoids repeated malloc/free |
| BruteforceThreshold = 96 | small scale | Tuned FFT/bruteforce crossover |

## Build

Requires a compiler with AVX2/FMA support.

```bash
g++ -O3 -mavx2 -mfma -funroll-loops -o your_program your_program.cpp
```

### Hardware requirements

- CPU: AVX2 + FMA (Intel Haswell 2013+ / AMD Piledriver 2012+)
- Note: AVX-512 provides no additional benefit (1 FMA port on current consumer CPUs)

## File structure

```
precious-speed/
├── masonxiong.cpp            # Original version (baseline)
├── masonxiong_opt.cpp        # Optimized version (main library)
├── mason_compat.hpp          # Compatibility header
├── comprehensive_test.cpp     # Correctness tests (all scales)
├── verify.cpp                # Quick verification
├── tle_duel.cpp              # A/B testing framework
├── bench_all.cpp             # Multi-library benchmark (ORIG/OPT/GMP/Boost)
├── speed_test.ps1            # Full speed comparison script
├── benchmark_report.md       # Latest benchmark report
└── speed_report.md           # Detailed speed comparison
```

## Usage

```cpp
#include "masonxiong_opt.cpp"

UnsignedInteger a("123456789012345678901234567890");
UnsignedInteger b("987654321098765432109876543210");

UnsignedInteger sum = a + b;
UnsignedInteger product = a * b;
UnsignedInteger quotient = a / b;
UnsignedInteger remainder = a % b;
UnsignedInteger squared = a.square();
```

## Testing

### Correctness

```bash
g++ -O3 -mavx2 -mfma -funroll-loops -o ctest_opt.exe comprehensive_test.cpp
./ctest_opt.exe
```

### Benchmark

```bash
# ORIG
g++ -O3 -mavx2 -mfma -funroll-loops -DUSE_ORIGINAL -o bench_orig.exe bench_all.cpp

# OPT
g++ -O3 -mavx2 -mfma -funroll-loops -o bench_opt.exe bench_all.cpp

# Run
./bench_orig.exe 5 1000000 mul orig_1M_mul
./bench_opt.exe 5 1000000 mul opt_1M_mul
```

## License

This project is for educational and competitive programming use. The original code by masonxiong and yuygfgg does not include an explicit open-source license. All optimizations in this repository are shared with attribution to the original authors.
