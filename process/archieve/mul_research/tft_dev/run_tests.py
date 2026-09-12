#!/usr/bin/env python3
"""Run TFT correctness tests only (skip slow benchmark)."""
import sys
sys.path.insert(0, 'd:/precious_speed/tft_dev')
from tft_proto import test_tft_dft_correctness, test_tft_idft_correctness, test_tft_mul_correctness

ok1 = test_tft_dft_correctness()
print()
ok2 = test_tft_idft_correctness()
print()
ok3 = test_tft_mul_correctness()
print()
print('=== ALL PASS ===' if ok1 and ok2 and ok3 else '=== FAILED ===')
