#!/usr/bin/env python3
"""Debug TFT_DFT by comparing with numpy FFT."""
import numpy as np
import sys
sys.path.insert(0, 'd:/precious_speed/tft_dev')
from tft_proto import tft_dft

# Small test: N=16, m=9, la=5
np.random.seed(42)
N, m, la = 16, 9, 5
a = np.random.randn(la) + 1j * np.random.randn(la)
a_pad = np.zeros(N, dtype=complex)
a_pad[:la] = a

# TFT result
tft_result = tft_dft(a_pad, N, m)

# numpy reference
fft_full = np.fft.fft(a_pad, N)

print(f"N={N}, m={m}, la={la}")
print(f"TFT result (first {m}):")
for k in range(m):
    print(f"  [{k}] tft={tft_result[k]:.6f}  fft={fft_full[k]:.6f}  diff={abs(tft_result[k]-fft_full[k]):.2e}")
print(f"\nmax_err = {np.max(np.abs(tft_result - fft_full[:m])):.2e}")
