#!/usr/bin/env python3
# Convert HEX faithful test inputs (hex strings) -> DEC decimal inputs (same integers).
# Same mathematical division problem, only I/O encoding differs. Limb count identical,
# so FFT-core instruction cost depends purely on implementation quality (the "big head" find).
import os, sys
# HEX cases reach ~640K decimal digits; lift the 3.11+ int<->str safety cap.
sys.set_int_max_str_digits(10_000_000)

SRC = os.path.join(os.path.dirname(__file__), "cases_hex")
OUT = os.path.join(os.path.dirname(__file__), "cases_dec")
os.makedirs(OUT, exist_ok=True)

def main():
    names = sorted(f for f in os.listdir(SRC) if f.endswith(".in"))
    total_chars = 0
    for fn in names:
        with open(os.path.join(SRC, fn)) as f:
            lines = f.read().split("\n")
        T = int(lines[0])
        pairs = []
        for ln in lines[1:T+1]:
            ln = ln.strip()
            if not ln:
                continue
            a, b = ln.split()
            # hex -> integer -> decimal string (same value fed to DEC div.cpp)
            A = int(a, 16)
            B = int(b, 16)
            pairs.append((str(A), str(B)))
        with open(os.path.join(OUT, fn), "w") as f:
            f.write(f"{len(pairs)}\n")
            for a, b in pairs:
                f.write(f"{a} {b}\n")
        total_chars += sum(len(a) + len(b) for a, b in pairs)
        print(f"  {fn:26s} pairs={len(pairs):3d} dec_chars={total_chars//len(names)//1000}K(avg)")
    print(f"DONE -> {OUT}  total_dec_chars={total_chars//1000}K")

if __name__ == "__main__":
    main()
