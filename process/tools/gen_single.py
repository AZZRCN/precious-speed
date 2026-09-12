#!/usr/bin/env python3
# Single-pair (T=1) version of each HEX group's FIRST pair, emitted as BOTH:
#   cases_hex1/<g>.in  (hex, for HEX div.cpp)
#   cases_dec1/<g>.in  (decimal, for DEC div.cpp)  -- same integer, T=1
# DEC's binary misbehaves on T>1 inputs; single-pair avoids that and isolates per-division cost.
import os, sys
sys.set_int_max_str_digits(10_000_000)

SRC = os.path.join(os.path.dirname(__file__), "cases_hex")
OHEX = os.path.join(os.path.dirname(__file__), "cases_hex1")
ODEC = os.path.join(os.path.dirname(__file__), "cases_dec1")
os.makedirs(OHEX, exist_ok=True)
os.makedirs(ODEC, exist_ok=True)

GROUPS = ["length_ratio_0","length_ratio_1","length_ratio_2","length_ratio_3",
          "length_ratio_4","length_ratio_5","amax_0","amax_1","amax_2",
          "max_0","max_1","max_2","bzbound_0","large_0","rnear_2","medium_0"]

def main():
    for g in GROUPS:
        with open(os.path.join(SRC, g + ".in")) as f:
            lines = f.read().split("\n")
        T = int(lines[0])
        # first pair only
        a, b = lines[1].split()
        A = int(a, 16); B = int(b, 16)
        with open(os.path.join(OHEX, g + ".in"), "w") as f:
            f.write("1\n" + a + " " + b + "\n")
        with open(os.path.join(ODEC, g + ".in"), "w") as f:
            f.write("1\n" + str(A) + " " + str(B) + "\n")
        print(f"  {g:16s} hexA={len(a)} hexB={len(b)} decA={len(str(A))} pairs=1")
    print(f"DONE hex1={OHEX} dec1={ODEC}")

if __name__ == "__main__":
    main()
