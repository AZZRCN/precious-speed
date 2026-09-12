#!/usr/bin/env python3
"""对指定用例用 Python 大整数做权威裁判, 比较 div_orig / D4 / D10 / D11 / D12(cyc,nocyc)。

用法: python3 d12_judge_remote.py "l2:dividendKind:divisorKind:ratio" [...]
输出紧凑: 只报 OK / rc / 差值的量级与形状。
"""
import subprocess
import sys

sys.set_int_max_str_digits(20_000_000)

BIN = "/home/azzr/divbench/bin"
TMP = "/tmp/judge.in"

KINDS = {
    "pow10":     lambda n: "1" + "0" * (n - 1),
    "2pow10":    lambda n: "2" + "0" * (n - 1),
    "4pow10":    lambda n: "4" + "0" * (n - 1),
    "8pow10":    lambda n: "8" + "0" * (n - 1),
    "5pow10":    lambda n: "5" + "0" * (n - 1),
    "25pow10":   lambda n: "25" + "0" * (n - 2),
    "125pow10":  lambda n: "125" + "0" * (n - 3),
    "1024pow10": lambda n: "1024" + "0" * (n - 4),
    "pow10p1":   lambda n: "1" + "0" * (n - 2) + "1",
    "all9":      lambda n: "9" * n,
    "half":      lambda n: "5" + "0" * (n - 2) + "1",
    "9then0":    lambda n: "9" * (n // 2) + "0" * (n - n // 2),
}

import os
CANDS = os.environ.get("CANDS", "div_orig,div_D4,div_D10,div_D11,dmp_nocyc,dmp_cyc").split(",")


def brief(x):
    """把巨大的整数差压缩成可读形状。"""
    if x == 0:
        return "0"
    s = str(abs(x))
    sg = "-" if x < 0 else "+"
    nd = len(s)
    # 是否形如 m * 10^e
    st = s.rstrip("0")
    tz = nd - len(st)
    if len(st) <= 12:
        return "%s%s*10^%d (nd=%d)" % (sg, st, tz, nd)
    return "%s%s...%s (nd=%d, tz=%d)" % (sg, s[:8], s[-8:], nd, tz)


def main():
    for spec in sys.argv[1:]:
        l2, dk, bk, ratio = spec.split(":")
        l2 = int(l2)
        nb = l2 * 4
        na = int(nb * float(ratio))
        a, b = KINDS[dk](na), KINDS[bk](nb)
        with open(TMP, "w") as f:
            f.write("1\n%s %s\n" % (a, b))

        ia, ib = int(a), int(b)
        tq, tr = ia // ib, ia % ib
        print("=== %s   (na=%d nb=%d  |q|=%d |r|=%d)"
              % (spec, na, nb, len(str(tq)), len(str(tr))), flush=True)
        for c in CANDS:
            try:
                with open(TMP, "rb") as f:
                    p = subprocess.run(["%s/%s" % (BIN, c)], stdin=f,
                                       capture_output=True, timeout=900)
            except Exception as e:
                print("   %-10s EXC %s" % (c, str(e)[:80]), flush=True)
                continue
            if p.returncode != 0:
                print("   %-10s rc=%d ABORT %s" % (c, p.returncode,
                      p.stderr.decode(errors="replace").strip()[-160:]), flush=True)
                continue
            got = p.stdout.decode(errors="replace").split()
            if len(got) != 2:
                print("   %-10s BAD ntok=%d" % (c, len(got)), flush=True)
                continue
            gq, gr = int(got[0]), int(got[1])
            if gq == tq and gr == tr:
                print("   %-10s OK" % c, flush=True)
            else:
                chk = "consistent" if gq * ib + gr == ia else "INCONSISTENT"
                print("   %-10s BAD dq=%s dr=%s [%s]"
                      % (c, brief(gq - tq), brief(gr - tr), chk), flush=True)
        print(flush=True)


if __name__ == "__main__":
    main()
