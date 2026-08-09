#!/usr/bin/env python3
"""D12 回归扫描: 以 div_D4 (当前 LC 已提交 75ms 版本) 为参照, 逐字节比对输出。

判据 (只关心"相对 D4 的回归", 不关心家族先天 bug):
    SAME     : D11/D12 与 D4 输出完全一致        -> 无回归
    REG      : D11 或 D12 与 D4 不一致 (含 RC!=0) -> 必须修

只有出现 REG 时才请 Python 做裁判 (int(str) 对 60 万位是 O(n^2), 很贵)。

用法:
    python3 d12_reg_remote.py [l2list] [shapes] [ratio]
        l2list : 逗号分隔; 缺省用内置的档位邻域扫描
        shapes : 逗号分隔, 取值 pow10_all9 / all9_all9 / all9_pow10 / all9_pow10p1
        ratio  : a/b 位数比, 缺省 2.3
"""
import subprocess
import sys

sys.set_int_max_str_digits(200_000_000)

BINDIR = "/home/azzr/divbench/bin"
REF = "div_D4"
CANDS = ["div_D11", "div_D12"]
TMP = "/tmp/d12reg.in"
LIMB = 4


def run_one(binname, path):
    p = subprocess.run("%s/%s < %s" % (BINDIR, binname, path),
                       shell=True, capture_output=True, text=True, timeout=900)
    if p.returncode != 0:
        return "RC=%d" % p.returncode
    return p.stdout.strip()


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


def probe(l2, kind, ratio):
    nb = l2 * LIMB
    na = int(nb * ratio)
    a, b = shape_pair(kind, na, nb)
    with open(TMP, "w") as f:
        f.write("1\n%s %s\n" % (a, b))
    ref = run_one(REF, TMP)
    outs = {nm: run_one(nm, TMP) for nm in CANDS}
    bad = [nm for nm in CANDS if outs[nm] != ref]
    tag = "l2=%-6d %-13s" % (l2, kind)
    if not bad:
        print("[%s] SAME (vs D4)" % tag, flush=True)
        return None
    detail = "  ".join("%s=%s" % (nm.replace("div_", ""),
                                  "SAME" if outs[nm] == ref else "DIFF")
                       for nm in CANDS)
    print("[%s] *** REG *** %s" % (tag, detail), flush=True)
    # 请 Python 裁判, 弄清是 D4 错还是候选错
    truth = "%d %d" % (int(a) // int(b), int(a) % int(b))
    verdict = {"D4": "OK" if ref == truth else "BAD"}
    for nm in CANDS:
        verdict[nm.replace("div_", "")] = "OK" if outs[nm] == truth else "BAD"
    print("      judge: %s" % "  ".join("%s=%s" % kv for kv in verdict.items()),
          flush=True)
    for nm in bad:
        print("      %-6s -> %s" % (nm.replace("div_", ""), outs[nm][:60]), flush=True)
    print("      %-6s -> %s" % ("D4", ref[:60]), flush=True)
    print("      %-6s -> %s" % ("truth", truth[:60]), flush=True)
    return (l2, kind)


def default_l2_list():
    out = []
    for j in range(12, 18):           # 4096 .. 131072
        p = 1 << j
        for anc in (p // 2, (p * 5) // 8, (3 * p) // 4, (p * 7) // 8, p):
            for d in (-1, 0, 1):
                v = anc + d
                if v >= 256:
                    out.append(v)
    return sorted(set(out))


def main():
    l2list = ([int(x) for x in sys.argv[1].split(",")]
              if len(sys.argv) > 1 and sys.argv[1] != "-" else default_l2_list())
    shapes = (sys.argv[2].split(",") if len(sys.argv) > 2 and sys.argv[2] != "-"
              else ["pow10_all9", "all9_pow10p1", "all9_all9", "all9_pow10"])
    ratio = float(sys.argv[3]) if len(sys.argv) > 3 else 2.3

    regs = []
    for l2 in l2list:
        for kind in shapes:
            r = probe(l2, kind, ratio)
            if r:
                regs.append(r)
    print("\n=== 回归扫描结束: %d 个 REG ===" % len(regs), flush=True)
    for r in regs:
        print("   l2=%d %s" % r)
    return 1 if regs else 0


if __name__ == "__main__":
    sys.exit(main())
