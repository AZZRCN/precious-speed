#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
mk_precprobe.py : v10c -> 精度探针版

作用
----
1. `pick_k` 可被环境变量 FORCE_K 覆盖，用于扫描"某个 u 下 k 最大能到多少"
2. `merge_b2` 里统计 max |g[j] - round(g[j])|，程序结束打到 stderr
3. 顺带打印实际用到的 (k, coeffs, lm, ts, hiA/hiB)

用途：找 gk 表的真实精度边界。安全线 max_err < 0.25（留 2x 余量）。
"""
import io
import sys

src = sys.argv[1] if len(sys.argv) > 1 else "work/mul/v10c.cpp"
dst = sys.argv[2] if len(sys.argv) > 2 else "work/mul/_v10c_prec.cpp"
s = io.open(src, encoding="utf-8").read()

# ---- 1. 全局探针状态 ----
A1 = "// ============================ bit split / merge ============================"
assert s.count(A1) == 1
s = s.replace(A1, """// ---------------- 精度探针 ----------------
#include <cstdlib>
#include <cmath>
static double PP_maxerr = 0.0;
static int    PP_forcek = 0;
static int    PP_k = 0;
static unsigned PP_coeffs = 0, PP_lm = 0, PP_ts = 0;
static int    PP_hiA = 0, PP_hiB = 0;
static long   PP_calls = 0;

""" + A1)

# ---- 2. merge_b2 里插桩 ----
OLD = """static void merge_b2(u64* f, const double* g, size_t n, int k) {
    size_t i = 0, j = 0;
    int w = 0;
    u128 tmp = 0;
    while (i < n) {
        while (w < 64) { tmp += (u128)(u64)(int64_t)(g[j++] + 0.5) << w; w += k; }"""
NEW = """static void merge_b2(u64* f, const double* g, size_t n, int k) {
    size_t i = 0, j = 0;
    int w = 0;
    u128 tmp = 0;
    while (i < n) {
        while (w < 64) {
            const double gx = g[j];
            const double gr = (double)(int64_t)(gx + 0.5);
            const double ge = std::fabs(gx - gr);
            if (ge > PP_maxerr) PP_maxerr = ge;
            tmp += (u128)(u64)(int64_t)(g[j++] + 0.5) << w; w += k; }"""
assert s.count(OLD) == 1, "merge_b2 anchor"
s = s.replace(OLD, NEW)

# ---- 3. pick_k 可覆盖 ----
OLD2 = """    int i = 0;
    while (u > gk[i]) ++i;
    return 19 - i;
}"""
NEW2 = """    if (PP_forcek) return PP_forcek;
    int i = 0;
    while (u > gk[i]) ++i;
    return 19 - i;
}"""
assert s.count(OLD2) == 1, "pick_k anchor"
s = s.replace(OLD2, NEW2)

# ---- 4. mul_fft 记录参数 ----
OLD3 = """    fft::resize(ts);
    if (hiA) fft::difRecZeroHi((fft::cpx*)FB, ts); else fft::difRec((fft::cpx*)FB, ts, 0);"""
NEW3 = """    PP_k = k; PP_coeffs = coeffs; PP_lm = lm; PP_ts = ts;
    PP_hiA = (int)hiA; PP_hiB = (int)hiB; ++PP_calls;
    fft::resize(ts);
    if (hiA) fft::difRecZeroHi((fft::cpx*)FB, ts); else fft::difRec((fft::cpx*)FB, ts, 0);"""
assert s.count(OLD3) == 1, "mul_fft anchor"
s = s.replace(OLD3, NEW3)

# ---- 5. main 读环境变量 + 收尾打印 ----
OLD4 = """int main() {
    hugify(FB, sizeof FB);"""
NEW4 = """int main() {
    if (const char* e = getenv("FORCE_K")) PP_forcek = atoi(e);
    hugify(FB, sizeof FB);"""
assert s.count(OLD4) == 1, "main anchor"
s = s.replace(OLD4, NEW4)

OLD5 = """        if (w <= 0) break;
        off += w;
    }
    return 0;
}"""
NEW5 = """        if (w <= 0) break;
        off += w;
    }
    fprintf(stderr, "PROBE k=%d coeffs=%u lm=%u ts=%u hiA=%d hiB=%d calls=%ld maxerr=%.6f\\n",
            PP_k, PP_coeffs, PP_lm, PP_ts, PP_hiA, PP_hiB, PP_calls, PP_maxerr);
    return 0;
}"""
assert s.count(OLD5) == 1, "return anchor"
s = s.replace(OLD5, NEW5)

if "#include <cstdio>" not in s:
    s = s.replace("#include <cstring>", "#include <cstring>\n#include <cstdio>", 1)

io.open(dst, "w", encoding="utf-8", newline="\n").write(s)
print("wrote", dst, len(s))
