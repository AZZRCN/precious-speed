#!/usr/bin/env python3
# mtt_probe.py - 实证检验 HEX 库结构下 (A+iB) MTT 是否真为有效杠杆。
# 背景：HEX split_b2 已把每个操作数拆成半长复序列(偶/奇 digit 压进 1 个复点)，
#       即每个操作数内部已吃掉一次 2-for-1。标准 MTT 共轭恢复要求输入多项式为实数系数，
#       本库喂 DFT 的是已压成复数的 A_p/B_p，故 LEVEL-2(A_p+iB_p) 共轭恢复自由度不足。
# 本脚本用纯 cmath FFT 验证：
#   T1  LEVEL-1 实数 digit 打包 MTT(正确做法) -> 必对，但需全长度 DFT
#   T2  LEVEL-2 摘要公式(A_p+iB_p 共轭恢复)    -> 预期数学不成立 -> 复谱 != 真谱
import random, math, cmath

def fft(a, inv=False):
    n = len(a); out = list(a); j = 0
    for i in range(1, n):
        bit = n >> 1
        while j & bit:
            j ^= bit; bit >>= 1
        j ^= bit
        if i < j: out[i], out[j] = out[j], out[i]
    step = 2
    while step <= n:
        ang = 2*math.pi/step * (-1 if inv else 1)
        w = cmath.exp(1j*ang)
        for i in range(0, n, step):
            cur = 1+0j
            for k in range(step//2):
                t = out[i+k+step//2]*cur
                out[i+k+step//2] = out[i+k]-t
                out[i+k] = out[i+k]+t
                cur *= w
        step <<= 1
    if inv: out = [x/n for x in out]
    return out

def to_digits(x, k, n):
    m = (1 << k) - 1; d = []
    for _ in range(n):
        d.append(float(x & m)); x >>= k
    return d

def digits_to_int(d, k):
    r = 0
    for i, v in enumerate(d):
        r += int(round(v)) * ((1 << k) ** i)
    return r

def next_pow2(n):
    p = 1
    while p < n: p <<= 1
    return p

def conj(z): return z.conjugate()

# ---------------- T1: LEVEL-1 实数 digit MTT (正确) ----------------
def mtt_level1(a, b, k):
    total = len(a)
    L = next_pow2(2*total)
    P = [(a[j] if j < len(a) else 0.0) + 1j*(b[j] if j < len(b) else 0.0) for j in range(L)]
    Ph = fft(P)
    XA = [(Ph[i] + conj(Ph[(L-i) % L]))*0.5 for i in range(L)]
    XB = [(Ph[i] - conj(Ph[(L-i) % L])/(2j)) for i in range(L)]
    Y = [XA[i]*XB[i] for i in range(L)]
    y = fft(Y, inv=True)
    return [y[i].real for i in range(2*total-1)]

# ---------------- T2: LEVEL-2 摘要公式 (预期错) ----------------
def mtt_level2_probe(a, b, k):
    n = len(a); ts = n // 2
    FB = [a[2*j] + 1j*a[2*j+1] for j in range(ts)]
    GB = [b[2*j] + 1j*b[2*j+1] for j in range(ts)]
    C = [(a[2*j]-b[2*j+1]) + 1j*(a[2*j+1]+b[2*j]) for j in range(ts)]
    Chat = fft(C)
    L2_FA = [(Chat[j] + conj(Chat[(ts-j) % ts]))*0.5 for j in range(ts)]
    L2_FB = [(Chat[j] - conj(Chat[(ts-j) % ts])/(2j)) for j in range(ts)]
    true_FA = fft(FB); true_FB = fft(GB)
    Y = [L2_FA[j]*L2_FB[j] for j in range(ts)]
    y = fft(Y, inv=True)
    dig = []
    for j in range(ts):
        dig.append(y[j].real); dig.append(y[j].imag)
    return L2_FA, true_FA, dig[:2*ts-1]

def maxdiff(X, Y):
    return max(abs(X[i]-Y[i]) for i in range(len(X)))

random.seed(20260813)
K = 16
print("=== T1: LEVEL-1 实数 digit MTT (正确做法, 需全长度 DFT) ===")
for ndig in [32, 64, 96, 128, 192, 256]:
    A = random.getrandbits(ndig*K); B = random.getrandbits(ndig*K)
    a = to_digits(A, K, ndig); b = to_digits(B, K, ndig)
    conv = mtt_level1(a, b, K)
    got = digits_to_int(conv, K)
    ref = A*B
    ok = (got == ref)
    ts = ndig
    ratio = (2*(2*ts)*math.log2(2*ts)) / (3*ts*math.log2(ts)) if ts > 1 else 0
    print(f"  ndig={ndig:4d}  MTT_correct={ok}  inst_ratio(MTT/current)~{ratio:.3f}")

print("=== T2: LEVEL-2 摘要公式 (A_p+iB_p 共轭恢复) ===")
for ndig in [32, 64, 128, 256]:
    A = random.getrandbits(ndig*K); B = random.getrandbits(ndig*K)
    a = to_digits(A, K, ndig); b = to_digits(B, K, ndig)
    L2_FA, true_FA, dig = mtt_level2_probe(a, b, K)
    md = maxdiff(L2_FA, true_FA)
    got = digits_to_int(dig, K)
    ref = A*B
    print(f"  ndig={ndig:4d}  L2_FA_vs_true_FA_maxdiff={md:.3e}  L2_product==ref? {got==ref}")
print("结论: 若 T2 复谱差异巨大且乘积错误 -> LEVEL-2 公式数学不成立(即此前 div_molder 挂死根因)")
