# D3 patch: 放开 absDivMu 的真 cyclic (半尺寸 FFT) 到多块, 放宽 dispatch 护栏.
# 仅改两处, 单块行为不变. 正确性由 verify_official + fuzz 兜底.
import sys

SRC = "/home/azzr/divbench/src/div_D3.cpp"

def rd(p):
    with open(p) as f:
        return f.read()

s = rd(SRC)

# ---- 改动 1: absDivMu 内删除 est_blocks>10 的 cyclic_m 放大 ----
old1 = """            size_t est_blocks = (quotient.size + in - 1) / in;
            if (est_blocks > 10) {
                cyclic_m = int_ceil2(len2 + in + 1);
            }
"""
assert s.count(old1) == 1, ("old1 match", s.count(old1))
new1 = """            // D3: 不放大 cyclic_m, 真 cyclic (半尺寸 FFT) 对所有块数生效
"""
s = s.replace(old1, new1, 1)

# ---- 改动 2: dispatch 放宽 ab_safe ----
old2 = """                    bool ab_safe = (mu_in < len2) || ((quot_span.size + mu_in - 1) / mu_in <= 10);
                    if (mu_in <= len2 && ab_safe && mu_in >= 64)
"""
assert s.count(old2) == 1, ("old2 match", s.count(old2))
new2 = """                    // D3: mu_in 在 [64, len2] 内一律走 absDivMu (含多块), 复用 divisor DFT + 真 cyclic 提速
                    bool ab_safe = (mu_in < len2) || (mu_in >= 64);
                    if (mu_in <= len2 && ab_safe && mu_in >= 64)
"""
s = s.replace(old2, new2, 1)

with open(SRC, "w") as f:
    f.write(s)
print("PATCH_D3 OK")
