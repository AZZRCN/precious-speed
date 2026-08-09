#!/usr/bin/env python3
"""
TFT (Truncated Fourier Transform) prototype.
Validates correctness of TFT/ITFT algorithms for multiplication.

Simplified scheme (avoids complex recursive ITFT):
  - TFT_DFT: compute first m coefficients of DFT_N(a) with input-side pruning.
  - ITFT: for real-valued convolution output c[0..m-1] with m > N/2,
    complete C[m..N-1] via conjugate symmetry C[N-k]=conj(C[k]),
    then standard IFFT. Savings come from TFT_DFT side.

Key insight (van der Hoeven 2004):
  For c = a * b with len(c) = L, choose N = next_pow2(L).
  - TFT_DFT(a, N, L): compute first L coefficients of DFT_N(a),
    exploiting a[j] = 0 for j >= len(a) (zero-padding).
  - Input-side pruning: when len(a) < N, deeper recursion levels have
    fewer non-zero inputs, reducing work.
"""

import numpy as np
import time


def w(N, k):
    """N-th root of unity to the k-th power: exp(-2*pi*i*k/N)."""
    return np.exp(-2j * np.pi * k / N)


def tft_dft_brute(a, N, m):
    """Brute-force TFT_DFT: compute first m coefficients of DFT_N(a).
    a: length-N array (zero-padded beyond len(a)).
    Returns: length-m complex array.
    O(m * N) for verification only.
    """
    a_full = np.zeros(N, dtype=complex)
    a_full[:len(a)] = a
    result = np.zeros(m, dtype=complex)
    for k in range(m):
        s = 0j
        for j in range(N):
            s += a_full[j] * w(N, k * j)
        result[k] = s
    return result


def tft_dft(a, N, m):
    """Recursive TFT_DFT: compute first m coefficients of DFT_N(a).
    a: length-N complex array (zero-padded).
    N: power of 2.
    m: 0 <= m <= N.
    Returns: length-m complex array.
    """
    if m == 0:
        return np.zeros(0, dtype=complex)
    if N == 1:
        return np.array([a[0]], dtype=complex)

    h = N // 2
    a_e = a[0::2]  # length h
    a_o = a[1::2]  # length h

    if m <= h:
        e_dft = tft_dft(a_e, h, m)
        o_dft = tft_dft(a_o, h, m)
        wk = np.array([w(N, k) for k in range(m)])
        return e_dft + wk * o_dft
    else:
        e_dft = tft_dft(a_e, h, h)
        o_dft_full = tft_dft(a_o, h, h)
        o_dft_part = tft_dft(a_o, h, m - h)
        result = np.zeros(m, dtype=complex)
        wk_half = np.array([w(N, k) for k in range(h)])
        result[:h] = e_dft + wk_half * o_dft_full
        wk_second = np.array([w(N, k) for k in range(m - h)])
        result[h:m] = e_dft[:m - h] - wk_second * o_dft_part
        return result


def tft_idft_brute(C, N, m):
    """Brute-force ITFT: solve V_m c = C via linear system.
    O(m^3) for verification only.
    """
    F = np.zeros((m, m), dtype=complex)
    for k in range(m):
        for j in range(m):
            F[k, j] = w(N, k * j)
    return np.linalg.solve(F, C[:m])


def tft_idft(C_partial, N, m):
    """ITFT via conjugate-completion + standard IFFT (simplified scheme).

    For real-valued convolution output c[0..m-1] with c[j]=0 for j>=m:
      1. C_partial = C[0..m-1] (first m DFT coefficients of c)
      2. For real c: C[N-k] = conj(C[k]), complete C[m..N-1] from C[1..N-m]
         (requires m > N/2 so that N-m < m, mirror indices are known)
      3. Standard IFFT on the full N-point C recovers c[0..N-1]; take c[0..m-1].

    Returns: length-m real array (the recovered c).
    """
    if m == 0:
        return np.zeros(0)
    if m == 1:
        # DFT_N(c)[0] = sum(c) = c[0] when c has length 1. No division by N.
        return np.array([C_partial[0].real])
    if m > N:
        raise ValueError(f"m={m} > N={N}")
    if m <= N // 2:
        # Cannot use conjugate completion (N-m >= m). Brute-force fallback.
        # Note: numerically unstable for large m; only use for small m.
        return tft_idft_brute(C_partial, N, m).real

    C_full = np.zeros(N, dtype=complex)
    C_full[:m] = C_partial
    for k in range(1, N - m + 1):
        C_full[N - k] = np.conj(C_partial[k])

    c = np.fft.ifft(C_full)
    return c[:m].real


def tft_mul(a, b):
    """TFT-based multiplication (real-valued).
    a, b: real-valued numpy arrays (limb sequences).
    Returns: convolution of a and b (length len(a) + len(b) - 1).
    """
    la, lb = len(a), len(b)
    if la == 0 or lb == 0:
        return np.zeros(0)
    L = la + lb - 1
    N = 1
    while N < L:
        N *= 2

    a_pad = np.zeros(N, dtype=complex)
    a_pad[:la] = a
    b_pad = np.zeros(N, dtype=complex)
    b_pad[:lb] = b

    A = tft_dft(a_pad, N, L)
    B = tft_dft(b_pad, N, L)

    C = A * B

    c = tft_idft(C, N, L)
    return c


def fft_mul(a, b):
    """Standard FFT multiplication for comparison."""
    la, lb = len(a), len(b)
    if la == 0 or lb == 0:
        return np.zeros(0)
    L = la + lb - 1
    N = 1
    while N < L:
        N *= 2
    A = np.fft.fft(a, N)
    B = np.fft.fft(b, N)
    C = A * B
    c = np.fft.ifft(C)
    return c.real[:L]


def test_tft_dft_correctness():
    """Verify tft_dft matches numpy.fft.fft for first m coefficients."""
    print("=== TFT_DFT Correctness Test ===")
    np.random.seed(42)
    test_cases = [
        (16, 8, 8), (16, 15, 10), (16, 16, 16), (16, 1, 1), (16, 7, 5),
        (256, 200, 150), (256, 128, 100), (256, 255, 200), (1024, 500, 400),
    ]
    all_pass = True
    for N, m, la in test_cases:
        a = np.random.randn(la) + 1j * np.random.randn(la)
        a_pad = np.zeros(N, dtype=complex)
        a_pad[:la] = a
        tft_result = tft_dft(a_pad, N, m)
        fft_full = np.fft.fft(a_pad, N)
        err = np.max(np.abs(tft_result - fft_full[:m]))
        status = "PASS" if err < 1e-9 else "FAIL"
        if status == "FAIL":
            all_pass = False
        print(f"  N={N:5d} m={m:5d} len_a={la:5d}: max_err={err:.2e} [{status}]")
    return all_pass


def test_tft_idft_correctness():
    """Verify tft_idft recovers c from partial DFT coefficients (real c)."""
    print("=== ITFT Correctness Test (real c, conjugate completion) ===")
    np.random.seed(42)
    test_cases = [
        (16, 1), (16, 9), (16, 12), (16, 15), (16, 16),
        (256, 130), (256, 200), (256, 255), (256, 256),
        (1024, 600), (1024, 1023), (1024, 1024),
    ]
    all_pass = True
    for N, m in test_cases:
        # Real c of length m
        c = np.random.randn(m)
        c_pad = np.zeros(N, dtype=complex)
        c_pad[:m] = c
        C_full = np.fft.fft(c_pad, N)
        c_recovered = tft_idft(C_full[:m], N, m)
        err = np.max(np.abs(c_recovered - c))
        status = "PASS" if err < 1e-6 else "FAIL"
        if status == "FAIL":
            all_pass = False
        print(f"  N={N:5d} m={m:5d}: max_err={err:.2e} [{status}]")
    return all_pass


def test_tft_mul_correctness():
    """Verify tft_mul matches fft_mul."""
    print("=== TFT Multiplication Correctness Test ===")
    np.random.seed(42)
    test_cases = [
        (1, 1), (5, 5), (10, 5), (100, 100), (255, 255),
        (256, 256), (200, 300), (1000, 1000), (500, 1500),
        (127, 127), (128, 128), (129, 129),
    ]
    all_pass = True
    for la, lb in test_cases:
        a = np.random.randint(0, 10000, la).astype(np.float64)
        b = np.random.randint(0, 10000, lb).astype(np.float64)
        c_fft = fft_mul(a, b)
        c_tft = tft_mul(a, b)
        err = np.max(np.abs(c_fft - c_tft))
        status = "PASS" if err < 1e-3 else "FAIL"
        if status == "FAIL":
            all_pass = False
        print(f"  la={la:5d} lb={lb:5d} L={la+lb-1:5d}: max_err={err:.2e} [{status}]")
    return all_pass


def bench_tft_vs_fft():
    """Benchmark TFT vs FFT for various sizes."""
    print("=== TFT vs FFT Performance Benchmark ===")
    np.random.seed(42)
    sizes = [
        (500, 500, "balanced ~512"),
        (1000, 1000, "balanced ~1024"),
        (2000, 2000, "balanced ~2048"),
        (50000, 50000, "balanced ~64K"),
        (500000, 500000, "balanced ~1M"),
        (450000, 50000, "unbalanced ~512K"),
    ]
    print(f"  {'case':<30s} {'L':>10s} {'N':>10s} {'FFT(ms)':>10s} {'TFT(ms)':>10s} {'ratio':>8s}")
    for la, lb, desc in sizes:
        a = np.random.randint(0, 10000, la).astype(np.float64)
        b = np.random.randint(0, 10000, lb).astype(np.float64)
        L = la + lb - 1
        N = 1
        while N < L:
            N *= 2
        t0 = time.time()
        for _ in range(3):
            c_fft = fft_mul(a, b)
        t_fft = (time.time() - t0) / 3 * 1000
        t0 = time.time()
        for _ in range(3):
            c_tft = tft_mul(a, b)
        t_tft = (time.time() - t0) / 3 * 1000
        ratio = t_tft / t_fft if t_fft > 0 else 0
        print(f"  {desc:<30s} {L:>10d} {N:>10d} {t_fft:>10.2f} {t_tft:>10.2f} {ratio:>7.2f}x")


if __name__ == "__main__":
    print("TFT Prototype Validation (simplified ITFT via conjugate completion)\n")
    dft_ok = test_tft_dft_correctness()
    print()
    idft_ok = test_tft_idft_correctness()
    print()
    mul_ok = test_tft_mul_correctness()
    print()
    if dft_ok and idft_ok and mul_ok:
        print("=== All correctness tests PASSED ===\n")
        bench_tft_vs_fft()
    else:
        print("=== Some tests FAILED ===")
