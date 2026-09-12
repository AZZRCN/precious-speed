#!/usr/bin/env python3
import sys
sys.set_int_max_str_digits(10_000_000)

def verify(hexin, decout):
    with open(hexin) as f:
        lines = f.read().split("\n")
    a, b = lines[1].split()
    A = int(a, 16); B = int(b, 16)
    with open(decout) as f:
        ol = f.read().split("\n")
    q, r = ol[0].split()
    Q = int(q); R = int(r)
    ok = (Q * B + R == A) and (0 <= R < B)
    print(f"{hexin}: A_limbs~{len(a)//16} B_limbs~{len(b)//16}  Q*B+R==A:{ok}  0<=R<B:{0<=R<B}  Q_digits={len(q)} R_digits={len(r)}")

verify("cases_hex1/length_ratio_2.in", "dec_length_ratio_2.out")
verify("cases_hex1/amax_1.in", "dec_amax_1.out")
verify("cases_hex1/amax_0.in", "dec_amax_0.out")
