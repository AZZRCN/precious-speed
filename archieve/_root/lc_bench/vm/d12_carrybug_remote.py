#!/usr/bin/env python3
"""隔离 carry 对抗输入下的错误: 是 D12(fft_ceil) 引入的, 还是 D 系列早已潜伏?

对每个单独的 (a, b) 形状**单独成文件**跑, 逐个二进制比对, 避免批次互相污染。
Python 的 // 与 % 作为独立第三方裁判 (不依赖 div_orig)。
"""
import subprocess
import sys

sys.set_int_max_str_digits(100_000_000)

BINS = sys.argv[1].split(",") if len(sys.argv) > 1 else \
    ["div_orig", "div_D4", "div_D11", "div_D12"]
TMP = "/tmp/carrybug.in"
LIMB = 4


def run_one(binname, a, b):
    with open(TMP, "w") as f:
        f.write("1\n%s %s\n" % (a, b))
    p = subprocess.run("/home/azzr/divbench/bin/%s < %s" % (binname, TMP),
                       shell=True, capture_output=True, text=True, timeout=600)
    if p.returncode != 0:
        return "RC=%d" % p.returncode
    return p.stdout.strip()


def probe(tag, a, b):
    ai, bi = int(a), int(b)
    truth = "%d %d" % (ai // bi, ai % bi)          # Python 独立裁判
    outs = {nm: run_one(nm, a, b) for nm in BINS}
    verdict = {nm: ("OK" if outs[nm] == truth else "BAD") for nm in BINS}
    line = "  ".join("%s=%s" % (nm.replace("div_", ""), verdict[nm]) for nm in BINS)
    bad = [nm for nm in BINS if verdict[nm] == "BAD"]
    print("[%-34s] %s" % (tag, line), flush=True)
    if bad:
        for nm in bad:
            o = outs[nm]
            print("      %-9s -> %s" % (nm, o[:70] if o.startswith("RC=") else o[:70]))
        print("      %-9s -> %s" % ("truth", truth[:70]))
    return not bad


def main():
    ok = True
    # 覆盖 fft_ceil 两侧: l2 取 2^j/2, 0.75*2^j, 2^j 附近
    for j in (14, 15, 16):
        p = 1 << j
        for anc in (p // 2, (3 * p) // 4, p):
            for d in (-1, 0, 1):
                l2 = anc + d
                if l2 < 64:
                    continue
                nb = l2 * LIMB
                na = int(nb * 2.3)
                pairs = [
                    ("all9/all9",   "9" * na,                 "9" * nb),
                    ("pow10/all9",  "1" + "0" * (na - 1),      "9" * nb),
                    ("all9/pow10",  "9" * na,                 "1" + "0" * (nb - 1)),
                    ("all9/pow10p1", "9" * na,                "1" + "0" * (nb - 2) + "1"),
                ]
                for nm, a, b in pairs:
                    ok &= probe("l2=%d %s" % (l2, nm), a, b)
    print("\n=== carry bug 隔离结论: %s ===" % ("全部 OK" if ok else "存在失败"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
