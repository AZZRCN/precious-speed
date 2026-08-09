#!/usr/bin/env python3
"""D12 退化除数回归扫描 —— 基准 = div_orig (LC #389360 AC 的真原版)。

★ 判据修正 (2026-08-03):
   早先版本用 dmp_cyc vs dmp_nocyc 自比, 会把**家族先天 bug**也报成 bad:
   例如 a=10^N / b=10^M-1 时 div_orig / D4 / D10 / D11 全部算错 (absDivMu
   的 cyclic unwrap 近似 + 10 次修正上限失效), 只有关掉 cyclic 的 nocyc 对。
   那不是 D12 引入的回归, 不该阻塞提交。

   正确判据 = "相对 div_orig 有没有回归":
     REG   : cyc 与 orig 输出不一致 (含 rc!=0)      -> 必须修, 阻塞提交
     PRE   : cyc == orig, 但 nocyc != orig          -> 家族先天 bug, 仅记录
     ok    : cyc == orig == nocyc

重点打击"退化除数": 归一化后 m = 5000*B^(k-1) 或 8000*B^(k-1) 之类,
只含质因子 2/5, 会让 B^2k mod m == 0 (GMP off-by-one 判据失效),
以及 inv0 整数位 = 2 (GMP 布局假设失效)。

用法: python3 d12_invdiff_remote.py [l2list] [kinds]
"""
import subprocess
import sys

BIN = "/home/azzr/divbench/bin"
ORIG = f"{BIN}/div_orig"
CYC = f"{BIN}/dmp_cyc"
NOC = f"{BIN}/dmp_nocyc"
TMP = "/tmp/invdiff.in"

KINDS = {
    "pow10":      lambda nb: "1" + "0" * (nb - 1),
    "2pow10":     lambda nb: "2" + "0" * (nb - 1),
    "4pow10":     lambda nb: "4" + "0" * (nb - 1),
    "8pow10":     lambda nb: "8" + "0" * (nb - 1),
    "5pow10":     lambda nb: "5" + "0" * (nb - 1),
    "25pow10":    lambda nb: "25" + "0" * (nb - 2),
    "125pow10":   lambda nb: "125" + "0" * (nb - 3),
    "1024pow10":  lambda nb: "1024" + "0" * (nb - 4),
    "pow10p1":    lambda nb: "1" + "0" * (nb - 2) + "1",
    "all9":       lambda nb: "9" * nb,
    "half":       lambda nb: "5" + "0" * (nb - 2) + "1",
}

DIVIDENDS = {
    "all9":   lambda na: "9" * na,
    "pow10":  lambda na: "1" + "0" * (na - 1),
    "9then0": lambda na: "9" * (na // 2) + "0" * (na - na // 2),
}


def run_one(path, inp):
    with open(inp, "rb") as f:
        p = subprocess.run([path], stdin=f, capture_output=True)
    return p.returncode, p.stdout


def main():
    l2s = [int(x) for x in sys.argv[1].split(",")] if len(sys.argv) > 1 else \
        [4096, 6143, 6144, 6145, 8191, 8192, 8193, 10000, 12000, 16384]
    kinds = sys.argv[2].split(",") if len(sys.argv) > 2 else list(KINDS)

    total = reg = pre = 0
    for l2 in l2s:
        nb = l2 * 4
        for kn in kinds:
            if kn not in KINDS:
                continue
            b = KINDS[kn](nb)
            for dn, dgen in DIVIDENDS.items():
                for ratio in (1.5, 2.0, 2.3):
                    na = int(nb * ratio)
                    a = dgen(na)
                    if (len(a) + len(b)) > 4_000_000:
                        continue
                    with open(TMP, "w") as f:
                        f.write("1\n%s %s\n" % (a, b))
                    total += 1
                    rc0, o0 = run_one(ORIG, TMP)
                    rc1, o1 = run_one(CYC, TMP)
                    rc2, o2 = run_one(NOC, TMP)
                    tag = "l2=%d %s/%s r=%.1f" % (l2, dn, kn, ratio)
                    if rc1 != rc0 or o1 != o0:
                        reg += 1
                        pos = next((i for i, (x, y) in enumerate(zip(o0, o1)) if x != y),
                                   min(len(o0), len(o1)))
                        print("[REG  ] %-34s orig_rc=%d cyc_rc=%d  first_diff@%d len %d vs %d"
                              % (tag, rc0, rc1, pos, len(o0), len(o1)), flush=True)
                    elif rc2 != rc0 or o2 != o0:
                        pre += 1
                        print("[PRE  ] %-34s (家族先天 bug: nocyc 与 orig 不同, cyc 与 orig 一致)"
                              % tag, flush=True)
                    else:
                        print("[ok   ] %-34s" % tag, flush=True)
    print("\n=== total=%d REG=%d PRE=%d ===" % (total, reg, pre), flush=True)
    return 1 if reg else 0


if __name__ == "__main__":
    sys.exit(main())
