#!/usr/bin/env python3
# gen_cases.py - generate random big-integer division cases in Library Checker format.
# Format (matching 393027_opt.cpp main()):
#   line0: T            (number of queries)
#   next T lines: "A B" (hex, big-endian, no 0x prefix)
# The binary computes Q=A//B, R=A%B per query and prints "Q R" hex per line.
#
# We deliberately cover the FFT-dominated regime (both operands large) plus a few
# small/edge cases so the oracle exercises every code path.
import sys, os, random

def rand_hex(bits, rng):
    # produce a hex string of ~bits bits, big-endian, no leading zeros
    nbytes = (bits + 7) // 8
    raw = bytearray(rng.getrandbits(8) for _ in range(nbytes))
    # ensure high bit set so length is stable
    raw[0] |= 0x80
    return raw.hex()

def main():
    rng = random.Random(0xC0FFEE)
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 40
    out = sys.argv[2] if len(sys.argv) > 2 else "cases.txt"
    lines = [str(n)]
    for _ in range(n):
        r = rng.random()
        if r < 0.10:
            # small operands (scalar / Knuth D paths)
            bits = rng.randint(1, 64 * 16)
        elif r < 0.20:
            # medium, quotient small (nb close to na)
            bits_a = rng.randint(64 * 200, 64 * 1200)
            bits_b = rng.randint(bits_a - 64 * 50, bits_a)
        else:
            # large FFT regime: both big, quotient moderate
            bits_a = rng.randint(64 * 1000, 64 * 103900)
            bits_b = rng.randint(64 * 200, 64 * (bits_a // 64 - 1))
        a = rand_hex(bits_a, rng)
        b = rand_hex(bits_b, rng)
        lines.append(f"{a} {b}")
    with open(out, "w") as f:
        f.write("\n".join(lines) + "\n")
    # also write a "big only" variant for the heavy I-refs benchmark
    big = [str(1), f"{rand_hex(64*103900, rng)} {rand_hex(64*51950, rng)}"]
    with open(out + ".big", "w") as f:
        f.write("\n".join(big) + "\n")
    print(f"wrote {out} ({n} cases) and {out}.big (1 huge case)")

if __name__ == "__main__":
    main()
