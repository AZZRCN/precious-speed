#!/usr/bin/env python3
"""D40 定向 fuzz: 专打 radix-7 引入的新档位不连续点 7p/8。

背景
----
D40 的 fft_ceil(n, allow7):
    p  = int_ceil2(n)
    h  = 0.75p ; 若 h >= FFT3_MIN(192) 且 h >= n  -> 取 h        (radix-3)
    h7 = 0.875p; 若 allow7 且 h7 >= FFT7_MIN(448) 且 h7 >= n -> 取 h7 (radix-7)
    否则 p
=> 相对 D39 唯一的新分支在 **n == 0.875p**:
     n <= 0.75p          -> 3*2^j  (老路径, D39 已覆盖)
     0.75p < n <= 0.875p -> 7*2^j  (★ D40 全新路径)
     n > 0.875p          -> 2^k
   FFT7_MIN=448 => h7>=448 即 p>=512 起生效, 所以必须从 j=9 开始扫, 不能只扫大尺寸。

受影响的调用点 (limb 计):
  lin  : divisor_float_len = fft_ceil(len2 + in)   ← 默认开 7
         inv_float_len     = fft_ceil(2*in + 1)    ← 默认开 7
  mn   : absInvNewtonGMP 的 cyclic 模数 B^mn-1     ← 仅 -DFFT7_MN
  cycm : absDivMu 的 cyclic 模数                    ← 仅 -DFFT7_CYCM
mn/cycm 会改变"模数 B^m-1 的整数结构"与 unwrap 的 wrap 次数, 风险最高,
因此本脚本对 d40a (all-7) 必须跑满。

判据: 与黄金实现 div_orig 逐字节比对。

用法: python3 d40_fuzz_remote.py <bin_tag> [stages]
      stages ∈ {boundary,nearzero,carry}, 逗号分隔, 缺省全跑。
"""
import os
import random
import subprocess
import sys

sys.set_int_max_str_digits(100_000_000)
random.seed(20260805)

TAG = sys.argv[1] if len(sys.argv) > 1 else "d40"
# 参考实现: 默认 div_orig (黄金)。但 div_orig 自己在 carry 阶段的全 9 / 10^k 型
# 输入上有 **24 个已知错例** (D12 起我方比原版更对), 所以要判"本候选是否引入回归"
# 时应改用上一代已验证候选 (如 d39) 作参考 —— 逐字节 == 即证明零回归。
#   用法: python3 d40_fuzz_remote.py <cand> [stages] [ref_tag]
REF = sys.argv[3] if len(sys.argv) > 3 else "div_orig"
BIN_ORIG = "/home/azzr/divbench/bin/" + REF
BIN_CAND = "/home/azzr/divbench/bin/" + TAG
TMP = "/tmp/d40fz_%s_%s.in" % (TAG, REF)  # ★ 带 tag: 允许多个候选并发 fuzz 而不互踩
LIMB_DIGITS = 4  # BASE = 10^4

# ★ 官方约束 SUM_OF_CHARACTER_LENGTH = 4_000_002 (info.toml / params.h)。
#   超过它就跑出了**黄金实现 div_orig 自己的定义域** —— 它的读入缓冲按该上限开,
#   越界后是 UB, 测出来的任何"不一致"都是假失配 (2026-08-05 踩过: carry 阶段
#   单批冲到 ~5.2MB, d40 报 24 个 MISMATCH, 实为 orig 越界)。
#   因此: 所有阶段统一 3.5MB 上限, 且必须在 **append 之前** 预检, 不能事后补刷。
BATCH_LIMIT = 3_500_000
# 单个数的位数上限 (LOG_10_A_AND_B_MAX)。超了同样是 orig 的定义域外。
MAX_DIGITS = 2_000_000


def rand_digits(n):
    if n <= 0:
        return "0"
    s = [random.choice("0123456789") for _ in range(n)]
    s[0] = random.choice("123456789")
    return "".join(s)


def run_bin(binpath):
    p = subprocess.run("%s < %s" % (binpath, TMP), shell=True,
                       capture_output=True, text=True, timeout=900)
    return p.stdout, p.returncode, p.stderr


def compare(name, cases):
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
                a, b = cases[min(i // 2, len(cases) - 1)]
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


class Batcher:
    """加入前预检的分批器: 保证单批总字符数 <= BATCH_LIMIT。"""

    def __init__(self, state, prefix):
        self.state, self.prefix, self.cases, self.used, self.n = state, prefix, [], 0, 0
        self.skipped = 0

    def add(self, a, b):
        if len(a) > MAX_DIGITS or len(b) > MAX_DIGITS:
            self.skipped += 1     # 定义域外, 丢弃 (否则是假失配)
            return
        c = len(a) + len(b) + 2
        if self.cases and self.used + c > BATCH_LIMIT:
            self.flush()
        self.cases.append((a, b))
        self.used += c

    def flush(self, suffix=""):
        if not self.cases:
            if self.skipped:
                print("[%s] skipped %d oversized cases" % (self.prefix, self.skipped),
                      flush=True)
            return
        self.n += 1
        flush("%s#%d%s" % (self.prefix, self.n, suffix), self.cases, self.state)
        self.used = 0
        if self.skipped:
            print("[%s] skipped %d oversized cases (>%d digits)"
                  % (self.prefix, self.skipped, MAX_DIGITS), flush=True)
            self.skipped = 0


# ---- 边界扫描: len2 落在 {0.75p, 0.875p, p} 邻域 (0.875p 为 D40 新增) --------
# j 从 9 起 (p=512, h7=448=FFT7_MIN 恰好开闸) 到 17 (p=131072)。
J_RANGE = (9, 10, 11, 12, 13, 14, 15, 16, 17)


def boundary_shapes():
    shapes = []
    for j in J_RANGE:
        p = 1 << j
        anchors = ((3 * p) // 4, (7 * p) // 8, p)
        for anc in anchors:
            for d in (-3, -2, -1, 0, 1, 2, 3):
                l2 = anc + d
                if l2 < 64:
                    continue
                # ratio 让 len2+in / 2*in+1 / (len2+in)/2+1 分别扫过各自的 7p/8
                for ratio in (1.02, 1.15, 1.34, 1.75, 2.01, 3.01, 5.01):
                    l1 = int(l2 * ratio) + 1
                    shapes.append((l1, l2))
    return shapes


def main():
    state = [0, 0]  # [total_cases, failed_batches]
    want = set((sys.argv[2] if len(sys.argv) > 2 else
                "boundary,nearzero,carry").split(","))

    flush("sanity", [("100", "7"), ("7", "100"), ("0", "5"), ("5", "5"),
                     ("123456789" * 50, "987654321" * 7)], state)

    # 1) 边界扫描 (随机数字)
    if "boundary" in want:
        bt = Batcher(state, "boundary")
        shapes = boundary_shapes()
        random.shuffle(shapes)
        print("boundary shapes: %d" % len(shapes), flush=True)
        for l1, l2 in shapes:
            bt.add(rand_digits(l1 * LIMB_DIGITS), rand_digits(l2 * LIMB_DIGITS))
        bt.flush()

    # 2) 对抗性余数: a = q*b (+tiny), 最压 cyclic unwrap / r-based 修正
    #    ★ 这一段是 mn/cycm 开 7 档 (d40a) 的主要风险面。
    if "nearzero" in want:
        bt = Batcher(state, "nearzero")
        for j in J_RANGE:
            p = 1 << j
            for anc in ((3 * p) // 4, (7 * p) // 8, p):
                for d in (-1, 0, 1):
                    l2 = anc + d
                    if l2 < 64:
                        continue
                    for qr in (1.01, 1.6, 2.0, 3.5):
                        b = int(rand_digits(l2 * LIMB_DIGITS))
                        if b % 2 == 0:
                            b += 1
                        q = int(rand_digits(max(1, int(l2 * qr)) * LIMB_DIGITS))
                        bstr = str(b)
                        for r in (0, 1, 7, b - 1):
                            bt.add(str(q * b + r), bstr)
        bt.flush()

    # 3) 进位对抗: 全 9 / 10^k+1 / 半满
    if "carry" in want:
        bt = Batcher(state, "carry")
        for j in J_RANGE:
            p = 1 << j
            for anc in ((3 * p) // 4, (7 * p) // 8, p):
                for d in (-1, 0, 1):
                    l2 = anc + d
                    if l2 < 64:
                        continue
                    nb = l2 * LIMB_DIGITS
                    na = int(nb * 2.3)
                    for bs in ("9" * nb, "1" + "0" * (nb - 1), "1" + "0" * (nb - 2) + "1"):
                        for a in ("9" * na, "1" + "0" * (na - 1)):
                            bt.add(a, bs)
        bt.flush()

    print("\n=== d40 fuzz [%s vs ref %s] 汇总: cases=%d  failed_batches=%d ==="
          % (TAG, REF, state[0], state[1]), flush=True)
    return 0 if state[1] == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
