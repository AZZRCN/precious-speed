#!/usr/bin/env python3
"""生成单个 (a,b) 用例文件, 供手工/gdb 复现。

用法: python3 d12_mkcase.py <out> <l2> <shape> [ratio]
"""
import sys

LIMB = 4


def shape_pair(kind, na, nb):
    if kind == "pow10_all9":
        return "1" + "0" * (na - 1), "9" * nb
    if kind == "all9_all9":
        return "9" * na, "9" * nb
    if kind == "all9_pow10":
        return "9" * na, "1" + "0" * (nb - 1)
    if kind == "all9_pow10p1":
        return "9" * na, "1" + "0" * (nb - 2) + "1"
    raise ValueError(kind)


def main():
    out, l2, kind = sys.argv[1], int(sys.argv[2]), sys.argv[3]
    ratio = float(sys.argv[4]) if len(sys.argv) > 4 else 2.3
    nb = l2 * LIMB
    na = int(nb * ratio)
    a, b = shape_pair(kind, na, nb)
    with open(out, "w") as f:
        f.write("1\n%s %s\n" % (a, b))
    print("wrote %s  na=%d nb=%d" % (out, na, nb))


if __name__ == "__main__":
    main()
