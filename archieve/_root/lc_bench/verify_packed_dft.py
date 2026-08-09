import numpy as np

def real_conv_current(a, b):
    n = max(len(a), len(b))
    n = 2 ** int(np.ceil(np.log2(2 * n)))
    A = np.fft.fft(a, n)
    B = np.fft.fft(b, n)
    # NOTE: np.fft.ifft already includes 1/n normalization, so no /n here.
    C = A * B
    return np.fft.ifft(C).real[:len(a) + len(b) - 1]

def real_conv_packed(a, b):
    n = max(len(a), len(b))
    n = 2 ** int(np.ceil(np.log2(2 * n)))
    # Pack two real sequences into one complex sequence c = a + i*b (zero-padded to n).
    c = np.zeros(n, dtype=complex)
    c[:len(a)] = a
    c[:len(b)] += 1j * b
    C = np.fft.fft(c)  # length is already n
    # Recover A = fft(a), B = fft(b) from C using conjugate-symmetry of real-sequence DFTs:
    #   C[k]      = A[k] + i*B[k]
    #   conj(C[n-k]) = A[k] - i*B[k]   (since A[n-k]=conj(A[k]), B[n-k]=conj(B[k]))
    A = np.zeros(n, dtype=complex)
    B = np.zeros(n, dtype=complex)
    for k in range(n):
        Cnk = np.conj(C[(n - k) % n])
        A[k] = (C[k] + Cnk) / 2
        B[k] = (C[k] - Cnk) / (2j)
    # No /n: ifft includes 1/n normalization.
    D = A * B
    return np.fft.ifft(D).real[:len(a) + len(b) - 1]

tests = [
    ([1, 2, 3, 4], [5, 6, 7, 8]),
    ([1], [2]),
    ([1, 2], [3, 4]),
    ([1, 0, 0, 0], [0, 1, 0, 0]),
    (list(range(100)), list(range(100))),
    # Edge cases:
    ([1, 2, 3, 4, 5, 6, 7], [10, 20]),               # unequal length, a longer
    ([10, 20], [1, 2, 3, 4, 5, 6, 7]),               # unequal length, b longer
    ([0, 0, 0], [1, 2, 3]),                            # one side all zero
    ([1, 2, 3], [0, 0, 0]),                            # other side all zero
    ([5], [5]),                                        # both single element
    ([1, 1, 1, 1, 1, 1, 1, 1], [1, 1, 1, 1, 1, 1, 1, 1]),  # power-of-2 length
    (list(range(1, 257)), list(range(1, 257))),        # large input (256 elems)
    ([1000000, 2000000, 3000000], [4000000, 5000000]), # large values
    ([1, 0], [0, 1]),                                  # minimal non-trivial
]

all_ok = True
for a, b in tests:
    a = np.array(a, dtype=float)
    b = np.array(b, dtype=float)
    cur = real_conv_current(a, b)
    pkd = real_conv_packed(a, b)
    exp = np.convolve(a, b)
    ok = np.allclose(cur, exp) and np.allclose(pkd, exp) and np.allclose(cur, pkd)
    if not ok:
        all_ok = False
        print(f"FAIL: len(a)={len(a)} len(b)={len(b)}")
        print(f"  exp[:5]={exp[:5]}")
        print(f"  cur[:5]={cur[:5]}")
        print(f"  pkd[:5]={pkd[:5]}")
        # Show max abs diff
        print(f"  max|pkd-exp| = {np.max(np.abs(pkd - exp))}")
    else:
        print(f"OK: len(a)={len(a):>3} len(b)={len(b):>3}")

print(f"\n{'ALL PASS' if all_ok else 'SOME FAILED'}")
