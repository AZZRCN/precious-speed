#!/usr/bin/env python3
# gen_cases_dir.py - emit N single-query case files into a directory (one "A B" per file,
# wrapped as "1\nA B\n"). Single-query avoids the v27 multi-query segfault and gives
# independent equivalence tests for the reducer's oracle.
import sys, os, random

def rand_hex(bits, rng):
    nbytes = (bits + 7) // 8
    raw = bytearray(rng.getrandbits(8) for _ in range(nbytes))
    raw[0] |= 0x80
    return raw.hex()

def main():
    rng = random.Random(0xC0FFEE ^ 7)
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 60
    outdir = sys.argv[2] if len(sys.argv) > 2 else "cases_dir"
    os.makedirs(outdir, exist_ok=True)
    for i in range(n):
        r = rng.random()
        if r < 0.15:
            bits_a = rng.randint(1, 64 * 16)
            bits_b = rng.randint(1, max(1, bits_a - 1))
        elif r < 0.30:
            bits_a = rng.randint(64 * 200, 64 * 1200)
            bits_b = rng.randint(bits_a - 64 * 50, bits_a)
        else:
            bits_a = rng.randint(64 * 1000, 64 * 103900)
            bits_b = rng.randint(64 * 200, 64 * (bits_a // 64 - 1))
        a = rand_hex(bits_a, rng)
        b = rand_hex(bits_b, rng)
        with open(os.path.join(outdir, f"case_{i:04d}.in"), "w") as f:
            f.write(f"1\n{a} {b}\n")
    print(f"wrote {n} single-query cases to {outdir}")

if __name__ == "__main__":
    main()
