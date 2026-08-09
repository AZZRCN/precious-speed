#!/usr/bin/env python3
"""D12 定向 fuzz: 专打 fft_ceil 引入的新档位分支点 + 对抗性余数。

背景
----
fft_ceil(n): p = int_ceil2(n); h = 0.75p; 若 h >= 192 且 h >= n 取 h, 否则取 p.
  => 在 n == 0.75p 处存在一个**新的不连续点**: n <= 0.75p 走 radix-3 (3*2^j),
     n > 0.75p 退回 2 幂. 这是 D12 相对 D11 唯一的新分支.

三个受影响的调用点 (limb 计):
  absInvNewtonGMP : mn            = fft_ceil(k + 1)              <- cyclic 模数 B^mn-1
  absDivMu        : cyclic_m      = max(fft_ceil(len2+1), fft_ceil((len2+in)/2+1))
                    divisor_float_len = fft_ceil(len2 + in)
                    inv_float_len     = fft_ceil(2*in + 1)

策略: 对每个 2 幂 p, 把 len2 密集扫过 {p/2, 2p/3, 0.75p, p} 各 ±4 的邻域, 再乘若干
      len1/len2 比值 -> 上述四个表达式的实参会分别扫过各自的 0.75p 边界.
      叠加 r≈0 (a = q*b [+tiny]) 与全 9 / 10^k+1 型进位对抗输入.

判据: 与黄金实现 div_orig 逐字节比对.
"""
import os
import random
import subprocess
import sys

sys.set_int_max_str_digits(100_000_000)
random.seed(20260803)

BIN_ORIG = "/home/azzr/divbench/bin/div_orig"
BIN_CAND = "/home/azzr/divbench/bin/" + (sys.argv[1] if len(sys.argv) > 1 else "div_D12")
TMP = "/tmp/d12fz.in"
LIMB_DIGITS = 4  # BASE = 10^4


def rand_digits(n):
    if n <= 0:
        return "0"
    s = [random.choice("0123456789") for _ in range(n)]
    s[0] = random.choice("123456789")
    return "".join(s)


def run_bin(binpath):
    p = subprocess.run("%s < %s" % (binpath, TMP), shell=True,
                       capture_output=True, text=True, timeout=600)
    return p.stdout, p.returncode, p.stderr


def compare(name, cases):
    """cases: list of (a_str, b_str). 逐字节比对, 返回 True/False。"""
    with open(TMP, "w") as f:
        f.write(str(len(cases)) + "\n")
        for a, b in cases:
            f.write(a + " " + b + "\n")
    o, orc, oerr = run_bin(BIN_ORIG)
    d, drc, derr = run_bin(BIN_CAND)
    try:
        os.remove(TMP)  # /tmp 是 ~1.7G tmpfs, 边跑边删
    except OSError:
        pass
    if orc != 0 or drc != 0:
        print("[%s] RC FAIL orig=%d cand=%d" % (name, orc, drc), flush=True)
        if oerr:
            print("   orig_stderr:", oerr[:200])
        if derr:
            print("   cand_stderr:", derr[:200])
        return False
    if o != d:
        ol, dl = o.split("\n"), d.split("\n")
        for i in range(min(len(ol), len(dl))):
            if ol[i] != dl[i]:
                a, b = cases[i // 2] if False else cases[min(i // 2, len(cases) - 1)]
                print("[%s] MISMATCH out-line %d (len_a=%d len_b=%d)"
                      % (name, i, len(a), len(b)), flush=True)
                print("   orig:", ol[i][:100])
                print("   cand:", dl[i][:100])
                break
        return False
    print("[%s] PASS (%d cases)" % (name, len(cases)), flush=True)
    return True


def flush(name, cases, state):
    if not cases:
        return
    state[0] += len(cases)
    if not compare(name, cases):
        state[1] += 1
    cases.clear()


# ---- 边界扫描: len2 落在 {p/2, 2p/3, 0.75p, p} 邻域 -------------------------
def boundary_shapes():
    shapes = []
    for j in (13, 14, 15, 16, 17):        # p = 8192 .. 131072 limbs
        p = 1 << j
        anchors = (p // 2, (2 * p) // 3, (3 * p) // 4, p)
        for anc in anchors:
            for d in (-2, -1, 0, 1, 2):
                l2 = anc + d
                if l2 < 64:
                    continue
                for ratio in (1.02, 1.34, 2.01, 3.01, 5.01):
                    l1 = int(l2 * ratio) + 1
                    shapes.append((l1, l2))
    return shapes


def main():
    state = [0, 0]  # [total_cases, failed_batches]
    # 阶段过滤: 第二个参数给逗号分隔的阶段名 (boundary/nearzero/carry), 缺省全跑
    want = set((sys.argv[2] if len(sys.argv) > 2 else
                "boundary,nearzero,carry").split(","))

    # 0) sanity
    flush("sanity", [("100", "7"), ("7", "100"), ("0", "5"), ("5", "5"),
                     ("123456789" * 50, "987654321" * 7)], state)

    # 1) 边界扫描 (随机数字) —— 按数据量分批, 避免 /tmp 撑爆
    batch, budget = [], 0
    shapes = boundary_shapes() if "boundary" in want else []
    random.shuffle(shapes)
    print("boundary shapes: %d" % len(shapes), flush=True)
    for idx, (l1, l2) in enumerate(shapes):
        a = rand_digits(l1 * LIMB_DIGITS)
        b = rand_digits(l2 * LIMB_DIGITS)
        batch.append((a, b))
        budget += len(a) + len(b)
        if budget > 24_000_000:
            flush("boundary#%d" % idx, batch, state)
            budget = 0
    flush("boundary#tail", batch, state)

    # 2) 对抗性余数: a = q*b (+tiny), 最压 cyclic unwrap / r-based 修正
    batch, budget = [], 0
    for j in ((13, 14, 15, 16) if "nearzero" in want else ()):
        p = 1 << j
        for anc in (p // 2, (3 * p) // 4, p):
            for d in (-1, 0, 1):
                l2 = anc + d
                if l2 < 64:
                    continue
                for qr in (1.01, 2.0, 3.5):
                    b = int(rand_digits(l2 * LIMB_DIGITS))
                    if b % 2 == 0:
                        b += 1
                    q = int(rand_digits(max(1, int(l2 * qr)) * LIMB_DIGITS))
                    for r in (0, 1, 7, b - 1):
                        a = q * b + r
                        batch.append((str(a), str(b)))
                    budget += (len(str(q)) + len(str(b))) * 5
                    if budget > 24_000_000:
                        flush("nearzero#%d" % l2, batch, state)
                        budget = 0
    flush("nearzero#tail", batch, state)

    # 3) 进位对抗: 全 9 / 10^k+1 / 半满
    #    ★ 必须分批: 官方最大输入 ~5.3MB, 两个二进制的读入缓冲都按该上限开.
    #      整批灌入 >20MB 会让 **黄金实现 div_orig 也段错误** (越界 UB), 测出来的
    #      任何"不一致"都无意义. 这里把单批总量压到 3.5MB 以内, 落在合法定义域.
    batch, budget = [], 0
    for j in ((13, 14, 15, 16) if "carry" in want else ()):
        for anc in ((1 << j) // 2, (3 * (1 << j)) // 4, 1 << j):
            for d in (-1, 0, 1):
                l2 = anc + d
                if l2 < 64:
                    continue
                nb = l2 * LIMB_DIGITS
                na = int(nb * 2.3)
                for bs in ("9" * nb, "1" + "0" * (nb - 1), "1" + "0" * (nb - 2) + "1"):
                    for a in ("9" * na, "1" + "0" * (na - 1)):
                        batch.append((a, bs))
                        budget += na + nb
                        if budget > 3_500_000:
                            flush("carry#l2=%d" % l2, batch, state)
                            budget = 0
    flush("carry#tail", batch, state)

    print("\n=== d12 fuzz 汇总: cases=%d  failed_batches=%d ==="
          % (state[0], state[1]), flush=True)
    return 0 if state[1] == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
