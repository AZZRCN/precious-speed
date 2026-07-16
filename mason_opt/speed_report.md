# Original vs Optimized Speed Comparison Report

**Generated**: 2026-07-15 11:19:49 (Asia/Shanghai)

## File Information

| Version | File Name | Path |
|---------|-----------|------|
| Original | masonxiong.cpp | d:\precious_speed\masonxiong.cpp |
| Optimized | masonxiong_opt.cpp | d:\precious_speed\mason_opt\masonxiong_opt.cpp |

## Test Environment

- **Compiler**: g++ (MinGW-w64)
- **Compile options**: -O3 -mavx2 -mfma -funroll-loops
- **C++ standard**: default
- **Test program**: tle_duel.cpp
- **Rounds**: each case runs 3 rounds per version, median taken
- **Interleave strategy**: different modules alternate to reduce cache/temperature bias

## Test Matrix

Covers 13 operation modules x 4 scale tiers (1K/10K/100K/1M), 47 test cases total.

| Module | Op | Description |
|--------|-----|-------------|
| Construction | ctor | Construct UnsignedInteger from string |
| I/O | io | ostringstream output + istringstream readback |
| To string | tostr | Convert to const char* |
| Conversion | conv | long long / double / string (3 conversions) |
| Comparison | cmp | 6 comparison operators |
| Addition | add | a + b |
| Subtraction | sub | aBig - bSmall |
| Multiplication | mul | a * b |
| Square | sqr | Optimized uses square() dedicated path, original uses a*a |
| Triple mul | mul2 | a^2 * a, 2 FFT multiplications |
| Division | div | aBig / bSmall |
| Newton div | div2 | a^2 / a, Newton iteration |
| Modulo | mod | aBig % bSmall |

## Detailed Results

| Op | Scale | Inner | Orig(ms) | Opt(ms) | Orig per(ms) | Opt per(ms) | Speedup | Winner |
|-----|-------|-------|----------|---------|--------------|-------------|---------|--------|
| ctor | 1000 | 2000 | 0.725 | 0.441 | 0.0004 | 0.0002 | 1.644x | OPT |
| io | 1000 | 500 | 0.888 | 0.886 | 0.0018 | 0.0018 | 1.002x | TIE |
| tostr | 1000 | 5000 | 0.813 | 0.809 | 0.0002 | 0.0002 | 1.005x | TIE |
| conv | 1000 | 5000 | 5.794 | 5.617 | 0.0012 | 0.0011 | 1.032x | OPT |
| cmp | 1000 | 100000 | 3.486 | 3.651 | 0 | 0 | 0.955x | ORIG |
| add | 1000 | 100000 | 24.545 | 10.612 | 0.0002 | 0.0001 | 2.313x | OPT |
| sub | 1000 | 100000 | 20.737 | 12.033 | 0.0002 | 0.0001 | 1.723x | OPT |
| mul | 1000 | 2000 | 7.75 | 5.809 | 0.0039 | 0.0029 | 1.334x | OPT |
| sqr | 1000 | 3000 | 11.471 | 6.059 | 0.0038 | 0.002 | 1.893x | OPT |
| mul2 | 1000 | 1000 | 7.499 | 5.603 | 0.0075 | 0.0056 | 1.338x | OPT |
| div | 1000 | 500 | 1.391 | 0.587 | 0.0028 | 0.0012 | 2.37x | OPT |
| div2 | 1000 | 500 | 19.499 | 14.723 | 0.039 | 0.0294 | 1.324x | OPT |
| mod | 1000 | 500 | 1.398 | 0.589 | 0.0028 | 0.0012 | 2.374x | OPT |
| ctor | 10000 | 500 | 0.88 | 0.833 | 0.0018 | 0.0017 | 1.056x | OPT |
| io | 10000 | 100 | 0.892 | 0.966 | 0.0089 | 0.0097 | 0.923x | ORIG |
| tostr | 100000 | 100 | 1.104 | 1.124 | 0.011 | 0.0112 | 0.982x | TIE |
| conv | 100000 | 200 | 23.174 | 22.785 | 0.1159 | 0.1139 | 1.017x | TIE |
| cmp | 100000 | 5000 | 0.177 | 0.207 | 0 | 0 | 0.855x | ORIG |
| add | 10000 | 10000 | 11.142 | 7.743 | 0.0011 | 0.0008 | 1.439x | OPT |
| sub | 100000 | 1000 | 9.968 | 9.765 | 0.01 | 0.0098 | 1.021x | OPT |
| mul | 10000 | 200 | 14.022 | 12.036 | 0.0701 | 0.0602 | 1.165x | OPT |
| sqr | 10000 | 300 | 22.224 | 12.707 | 0.0741 | 0.0424 | 1.749x | OPT |
| mul2 | 10000 | 100 | 7.693 | 6.429 | 0.0769 | 0.0643 | 1.197x | OPT |
| div | 10000 | 50 | 0.653 | 0.463 | 0.0131 | 0.0093 | 1.41x | OPT |
| div2 | 10000 | 50 | 23.422 | 14.153 | 0.4684 | 0.2831 | 1.655x | OPT |
| mod | 100000 | 5 | 0.968 | 0.643 | 0.1936 | 0.1286 | 1.505x | OPT |
| ctor | 100000 | 100 | 1.676 | 1.653 | 0.0168 | 0.0165 | 1.014x | TIE |
| io | 100000 | 20 | 3.301 | 3.822 | 0.165 | 0.1911 | 0.864x | ORIG |
| tostr | 1000000 | 10 | 1.185 | 1.081 | 0.1185 | 0.1081 | 1.096x | OPT |
| conv | 1000000 | 20 | 22.203 | 21.876 | 1.1102 | 1.0938 | 1.015x | TIE |
| cmp | 1000000 | 500 | 0.022 | 0.019 | 0 | 0 | 1.158x | OPT |
| add | 100000 | 1000 | 10.818 | 8.377 | 0.0108 | 0.0084 | 1.291x | OPT |
| sub | 1000000 | 50 | 38.115 | 22.857 | 0.7623 | 0.4571 | 1.668x | OPT |
| mul | 100000 | 20 | 14.55 | 12.621 | 0.7275 | 0.631 | 1.153x | OPT |
| sqr | 100000 | 30 | 23.719 | 11.072 | 0.7906 | 0.3691 | 2.142x | OPT |
| mul2 | 100000 | 10 | 16.173 | 9.958 | 1.6173 | 0.9958 | 1.624x | OPT |
| div | 100000 | 5 | 0.962 | 0.599 | 0.1924 | 0.1198 | 1.606x | OPT |
| div2 | 100000 | 5 | 31.951 | 18.099 | 6.3902 | 3.6198 | 1.765x | OPT |
| mod | 1000000 | 1 | 2.537 | 1.477 | 2.537 | 1.477 | 1.718x | OPT |
| ctor | 1000000 | 10 | 1.969 | 1.873 | 0.1969 | 0.1873 | 1.051x | OPT |
| io | 1000000 | 3 | 5.238 | 5.447 | 1.746 | 1.8157 | 0.962x | ORIG |
| add | 1000000 | 50 | 44.867 | 22.538 | 0.8973 | 0.4508 | 1.991x | OPT |
| mul | 1000000 | 3 | 27.373 | 22.843 | 9.1243 | 7.6143 | 1.198x | OPT |
| sqr | 1000000 | 3 | 29.397 | 13.79 | 9.799 | 4.5967 | 2.132x | OPT |
| mul2 | 1000000 | 2 | 47.875 | 25.583 | 23.9375 | 12.7915 | 1.871x | OPT |
| div | 1000000 | 1 | 2.648 | 1.638 | 2.648 | 1.638 | 1.617x | OPT |
| div2 | 1000000 | 1 | 82.74 | 49.333 | 82.74 | 49.333 | 1.677x | OPT |

## Module Summary

### Speedup > 1.02 (Optimized wins)

| Op | Scale | Speedup |
|-----|-------|---------|
| ctor | 1000 | 1.644x |
| conv | 1000 | 1.032x |
| add | 1000 | 2.313x |
| sub | 1000 | 1.723x |
| mul | 1000 | 1.334x |
| sqr | 1000 | 1.893x |
| mul2 | 1000 | 1.338x |
| div | 1000 | 2.37x |
| div2 | 1000 | 1.324x |
| mod | 1000 | 2.374x |
| ctor | 10000 | 1.056x |
| add | 10000 | 1.439x |
| sub | 100000 | 1.021x |
| mul | 10000 | 1.165x |
| sqr | 10000 | 1.749x |
| mul2 | 10000 | 1.197x |
| div | 10000 | 1.41x |
| div2 | 10000 | 1.655x |
| mod | 100000 | 1.505x |
| tostr | 1000000 | 1.096x |
| cmp | 1000000 | 1.158x |
| add | 100000 | 1.291x |
| sub | 1000000 | 1.668x |
| mul | 100000 | 1.153x |
| sqr | 100000 | 2.142x |
| mul2 | 100000 | 1.624x |
| div | 100000 | 1.606x |
| div2 | 100000 | 1.765x |
| mod | 1000000 | 1.718x |
| ctor | 1000000 | 1.051x |
| add | 1000000 | 1.991x |
| mul | 1000000 | 1.198x |
| sqr | 1000000 | 2.132x |
| mul2 | 1000000 | 1.871x |
| div | 1000000 | 1.617x |
| div2 | 1000000 | 1.677x |

### Speedup < 0.98 (Original wins)

| Op | Scale | Speedup |
|-----|-------|---------|
| cmp | 1000 | 0.955x |
| io | 10000 | 0.923x |
| cmp | 100000 | 0.855x |
| io | 100000 | 0.864x |
| io | 1000000 | 0.962x |

### Tied (0.98 ~ 1.02)

| Op | Scale | Speedup |
|-----|-------|---------|
| io | 1000 | 1.002x |
| tostr | 1000 | 1.005x |
| tostr | 100000 | 0.982x |
| conv | 100000 | 1.017x |
| ctor | 100000 | 1.014x |
| conv | 1000000 | 1.015x |

## Statistics

- **Total cases**: 47
- **Optimized wins**: 36 / 47
- **Original wins**: 5 / 47
- **Tied**: 6 / 47
- **Average speedup**: 1.423x
- **Max speedup**: 2.374x
- **Min speedup**: 0.855x

## Key Optimization Points

- **square() dedicated path** (sqr module): Optimized uses frequencyDomainPointwiseSquare, saving one DIF + half pointwise work
- **Unbalanced multiplication split** (mul/div2 large scale): splits when length >= 2*other.length
- **DigitAllocator memory pool**: bucket-based free list, reduces new/delete
- **__m256d butterfly**: FFT butterfly processes 2 complex numbers at once
- **DIF(other) cache**: reuses other's DIF result in unbalanced multiplication

