#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Upload ADD candidates to VM, compile, and report sizes.

Usage: python push_add.py [name ...]     (default: all add_A*.cpp in lc_bench/exe)
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import run, put  # noqa: E402

PS = r"D:\precious_speed"
EXE = os.path.join(PS, "lc_bench", "exe")
FLAGS = "-O2 -std=c++23 -march=x86-64-v3"
REMOTE = "/home/azzr/addbench"


def main():
    names = sys.argv[1:]
    if not names:
        names = sorted(f[:-4] for f in os.listdir(EXE)
                       if f.startswith("add_A") and f.endswith(".cpp"))

    for n in names:
        local = os.path.join(EXE, n + ".cpp")
        if not os.path.exists(local):
            print(f"SKIP {n}: no such file")
            continue
        put(local, f"{REMOTE}/src/{n}.cpp")
        print(f"uploaded {n}.cpp")

    # compile all in one shell round-trip
    cmds = []
    for n in names:
        cmds.append(
            f'echo "== {n} =="; '
            f'g++ {FLAGS} -o {REMOTE}/bin/{n} {REMOTE}/src/{n}.cpp 2>&1 | head -30; '
            f'echo "rc=$?"'
        )
    rc, out, err = run("; ".join(cmds), timeout=900)
    print(out)
    if err.strip():
        print("STDERR:", err[:2000])

    rc, out, err = run(f"ls -la {REMOTE}/bin/")
    print(out)


if __name__ == "__main__":
    main()
