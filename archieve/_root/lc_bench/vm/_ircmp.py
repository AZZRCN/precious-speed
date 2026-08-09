# -*- coding: utf-8 -*-
"""
批量 callgrind 指令计数对比 (夜间模式专用 —— 不依赖墙钟, 计时不可信时的唯一可信代理)

用法: python3 _ircmp.py <tag1,tag2,...> [case1,case2,...]
默认用例: r_nearly_zero_01 (schoolbook 主导) + length_ratio_integer_02 (FFT 主导)
两者瓶颈正交, 一起看能判断某项改动是全局收益还是只压一头。

输出每 (tag, case) 的 Ir / D1mr / DLmr, 以及 Zen3 估算周期:
    est = Ir + 5*D1mr + 200*DLmr
(该模型在本项目历史上与 LC 实测方向一致, 用于夜间排序候选。)
"""
import os, re, subprocess, sys

BENCH = "/home/azzr/divbench"
INDIR = "/home/azzr/lcp/big_integer/division_of_big_integers/in"

tags = sys.argv[1].split(",")
cases = (sys.argv[2].split(",") if len(sys.argv) > 2
         else ["r_nearly_zero_01", "length_ratio_integer_02"])

PAT = {
    "Ir":   re.compile(r"^summary:\s+(\d+)", re.M),
}

def measure(tag, case):
    out = "/tmp/cg_%s_%s.out" % (tag, case)
    if os.path.exists(out):
        os.remove(out)
    cmd = ["valgrind", "--tool=callgrind", "--cache-sim=yes",
           "--callgrind-out-file=" + out, "--quiet",
           os.path.join(BENCH, "bin", tag)]
    with open(os.path.join(INDIR, case + ".in"), "rb") as fi:
        subprocess.run(cmd, stdin=fi, stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL, check=False)
    # totals 行: "summary: Ir Dr Dw I1mr D1mr D1mw ILmr DLmr DLmw"
    with open(out) as f:
        txt = f.read()
    m = re.search(r"^summary:(.+)$", txt, re.M)
    if not m:
        return None
    v = [int(x) for x in m.group(1).split()]
    # 事件顺序取自 "events:" 行
    ev = re.search(r"^events:(.+)$", txt, re.M).group(1).split()
    d = dict(zip(ev, v))
    os.remove(out)
    return d

rows = []
for case in cases:
    base = None
    for tag in tags:
        d = measure(tag, case)
        if d is None:
            print("FAIL %s %s" % (tag, case)); continue
        ir = d.get("Ir", 0); d1 = d.get("D1mr", 0); dl = d.get("DLmr", 0)
        # I1mr/ILmr: 指令缓存 miss —— 判断 unroll/inline 是否把前端撑爆的关键。
        # Ir 降但 I1mr 涨 = 典型的「展开过头」, 墙钟会反向。
        i1 = d.get("I1mr", 0); il = d.get("ILmr", 0)
        est = ir + 5 * d1 + 200 * dl + 5 * i1 + 200 * il
        if base is None:
            base = est
        rows.append((case, tag, ir, d1, dl, i1, il, est, est / base))

print("%-28s %-7s %14s %10s %9s %10s %8s %14s %7s" %
      ("case", "tag", "Ir", "D1mr", "DLmr", "I1mr", "ILmr", "est_cycles", "ratio"))
print("-" * 118)
last = None
for case, tag, ir, d1, dl, i1, il, est, r in rows:
    if last is not None and last != case:
        print("-" * 118)
    print("%-28s %-7s %14d %10d %9d %10d %8d %14d %7.4f" %
          (case, tag, ir, d1, dl, i1, il, est, r))
    last = case
print("DONE")
