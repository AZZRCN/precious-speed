#!/usr/bin/env python3
"""定位 D12 在 carry 对抗输入上的回归: 逐个开关 bisect。

已知 (来自 d12_carrybug_remote.py):
  l2=8192  pow10/all9 : orig/D4/D11 = OK, D12 = BAD   <- D12 独有回归, 本脚本目标
  l2=12287 pow10/all9 : 全家族 BAD                     <- 先天 bug, 不在本次范围

变体:
  d12_full   : 默认 (fft3 全开)
  d12_no3    : -DNO_FFT3            fft_ceil 退化为 int_ceil2, 应等价于 D11
  d12_nomn   : -DBISECT_MN_2POW     仅 mn 回退 2 幂 (cyclic 模数)
  d12_nocycm : -DBISECT_CYCM_2POW   仅 absDivMu 的 cyclic_m 回退 2 幂
  d12_nolin  : -DBISECT_LIN_2POW    仅线性卷积长度回退 2 幂
"""
import subprocess
import sys

sys.set_int_max_str_digits(100_000_000)

VARIANTS = ["div_D11", "d12_full", "d12_no3", "d12_nomn", "d12_nocycm", "d12_nolin"]
TMP = "/tmp/bisect.in"
LIMB = 4


def run_one(binname, a, b):
    with open(TMP, "w") as f:
        f.write("1\n%s %s\n" % (a, b))
    p = subprocess.run("/home/azzr/divbench/bin/%s < %s" % (binname, TMP),
                       shell=True, capture_output=True, text=True, timeout=600)
    return "RC=%d" % p.returncode if p.returncode != 0 else p.stdout.strip()


def probe(tag, a, b):
    truth = "%d %d" % (int(a) // int(b), int(a) % int(b))
    res = []
    for nm in VARIANTS:
        out = run_one(nm, a, b)
        res.append("%s=%s" % (nm.replace("div_", "").replace("d12_", ""),
                              "OK" if out == truth else "BAD"))
    print("[%-26s] %s" % (tag, "  ".join(res)), flush=True)


def main():
    # 聚焦 D12 独有回归的尺寸带 (l2 = 8192 附近), 外加一个全家族已知坏点做对照
    for l2 in (4096, 6143, 6144, 8191, 8192, 8193, 12287):
        nb = l2 * LIMB
        na = int(nb * 2.3)
        probe("l2=%d pow10/all9" % l2, "1" + "0" * (na - 1), "9" * nb)
        probe("l2=%d all9/pow10p1" % l2, "9" * na, "1" + "0" * (nb - 2) + "1")
    return 0


if __name__ == "__main__":
    sys.exit(main())
