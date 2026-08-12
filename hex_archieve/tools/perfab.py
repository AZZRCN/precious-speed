#!/usr/bin/env python3
# AZZRCN
# https://github.com/AZZRCN
"""perfab.py — 基于 perf 的确定性 A/B（替代墙钟）

用法:
    python3 perfab.py <datadir> <binA> <binB> [binC ...] [-r REPEAT]

指标:
    IR   instructions:u          — 近确定性, 跨机器可迁移
    L1M  L1-dcache-load-misses   — L1d 缺失
    LLM  cache-misses            — 泛化事件, 虚拟化下抖动可达 45%, 只看数量级
    CYC  cycles:u                — 微架构量, 不可外推 Zen3, 只作探针

测量口径（两位前辈一致意见, 2026-08-11 定稿）:
  * 每个指标**独立**取 REPEAT 次的 min（噪声单边: 干扰只会让程序变慢）
  * ratio-of-mins：先各自取 min 再相除。禁 min-of-ratios（会给出 -7~-22% 虚假增益）
  * REPEAT 默认 10（判 1% 级差异 3 次远不够）
  * **不绑核**：VM 里 vCPU↔物理核映射由 hypervisor 决定, taskset 只是把噪声换个形状,
    还让该 vCPU 独吞中断且无法迁移, 引入系统性偏差

判优口径:
  * **MAX 行才是 LC headline**（docs/HANDBOOK.MD:903「LC 计分取最长测试点 CPU 时间」）
  * TOTAL 仅供参考, 它会把"某些长度档砍 25~37%、其它档 0%"的分布抹平

每个 case 都做 .exp md5 校验; 任何不匹配用 `!` 标出并计入 bad。
"""
import hashlib
import os
import subprocess
import sys

EVENTS = "instructions:u,cycles:u,L1-dcache-load-misses,cache-misses"
KEYS = [("instructions:u", "IR"),
        ("L1-dcache-load-misses", "L1M"),
        ("cache-misses", "LLM"),
        ("cycles:u", "CYC")]


def one(binpath, data, expmd5):
    r = subprocess.run(["perf", "stat", "-x,", "-e", EVENTS, binpath],
                       input=data, capture_output=True)
    ok = (expmd5 is None) or (hashlib.md5(r.stdout).hexdigest() == expmd5)
    m = {}
    for line in r.stderr.decode("utf-8", "replace").split("\n"):
        p = line.split(",")
        if len(p) > 2 and p[0].strip().replace(".", "").isdigit():
            m[p[2].strip()] = int(float(p[0]))
    return m, ok


def main():
    argv = sys.argv[1:]
    rep = 10
    if "-r" in argv:
        i = argv.index("-r")
        rep = int(argv[i + 1])
        del argv[i:i + 2]
    datadir, bins = argv[0], argv[1:]
    cases = sorted(f[:-3] for f in os.listdir(datadir) if f.endswith(".in"))

    inputs, exps = {}, {}
    for c in cases:
        inputs[c] = open(os.path.join(datadir, c + ".in"), "rb").read()
        ef = os.path.join(datadir, c + ".exp")
        exps[c] = hashlib.md5(open(ef, "rb").read()).hexdigest() if os.path.exists(ef) else None

    res, bad = {}, {}
    for b in bins:
        res[b], bad[b] = {}, 0
        for c in cases:
            acc, okall = {}, True
            for _ in range(rep):
                m, ok = one(b, inputs[c], exps[c])
                okall &= ok
                for k, _lbl in KEYS:          # 每个指标独立取 min
                    v = m.get(k)
                    if v is not None and (k not in acc or v < acc[k]):
                        acc[k] = v
            res[b][c] = acc
            if not okall:
                bad[b] += 1

    base = bins[0]
    for key, label in KEYS:
        print(f"\n===== {label}  ({key})   ratio = min(A)/min(B)   <1 = B 更差, >1 = B 更好 =====")
        hdr = "%-18s %14s" % ("case", os.path.basename(base))
        for b in bins[1:]:
            hdr += " %14s %8s" % (os.path.basename(b), "ratio")
        print(hdr)
        print("-" * len(hdr))
        tot = {b: 0 for b in bins}
        mx = {b: (0, "") for b in bins}
        for c in cases:
            va = res[base][c].get(key, 0)
            tot[base] += va
            if va > mx[base][0]:
                mx[base] = (va, c)
            row = "%-18s %14d" % (c, va)
            for b in bins[1:]:
                vb = res[b][c].get(key, 0)
                tot[b] += vb
                if vb > mx[b][0]:
                    mx[b] = (vb, c)
                row += " %14d %8s" % (vb, ("%.4f" % (va / vb)) if vb else "-")
            print(row)
        print("-" * len(hdr))
        # TOTAL（参考）
        row = "%-18s %14d" % ("TOTAL(ref)", tot[base])
        for b in bins[1:]:
            row += " %14d %8s" % (tot[b], ("%.4f" % (tot[base] / tot[b])) if tot[b] else "-")
        print(row)
        # MAX = LC headline（判优）
        row = "%-18s %14d" % ("MAX*headline", mx[base][0])
        for b in bins[1:]:
            row += " %14d %8s" % (mx[b][0], ("%.4f" % (mx[base][0] / mx[b][0])) if mx[b][0] else "-")
        print(row)
        row = "%-18s %14s" % ("  ^ which case", mx[base][1])
        for b in bins[1:]:
            row += " %14s %8s" % (mx[b][1], "")
        print(row)

    print("\n===== correctness =====")
    for b in bins:
        print("  %-20s bad=%d / %d" % (os.path.basename(b), bad[b], len(cases)))
    print("\n注: 判优看 MAX*headline 行 (LC 计分=最长测试点 CPU 时间); TOTAL 仅参考。")


if __name__ == "__main__":
    main()
