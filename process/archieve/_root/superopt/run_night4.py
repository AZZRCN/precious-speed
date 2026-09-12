# -*- coding: utf-8 -*-
"""2026-08-07 夜间第三轮投放 —— 「先窄后宽 + 常数切片」版

为什么推翻 run_night3 的排法:
    night2 的实测教训 —— digits2 全常数集 99.6 亿节点 / 20min 没展完,
    split100 94.9 亿节点 / 20min 没展完, 两个都是 0 候选。这**不是无解**,
    是「空间太大根本没搜完」。一把梭大空间等于把算力烧在展开分支上。

    更要命的第二个坑 (2026-08-07 20:55 发现):
        原 make_digits2x2_spec 的 consts 里 **既没有 2559 也没有 mask**,
        且 max_insn=8 < 已知解的 9 条 —— 已知解压根不在空间内, 搜到天荒地老
        也必然 0 候选。这与 P6 剪枝 bug 同型: 「无解」其实是「空间里没有」。

本轮铁律:
    1. 每个目标先跑 *_leanest / *_core (含已知解的最小空间) 拿可信基线;
       0 候选 = 立刻告警, 停止在该目标上继续烧算力。
    2. 放宽只按**常数分片**做, 一片一片来, 不整包塞。
    3. 深度打不穿的大目标要**拆子目标**(digits2x2 -> digits2x2_core,
       末尾 3 条跨 lane 合并是平凡恒等, 不值得搜)。

队列 (总预算 ~9h):
    E0  split100_leanest    tl 300   验收, 已知 6 条应秒出
    E1  digits2x2_core      tl 900   验收+首个真候选, 已知 6 条
    E2  split100 片A (5243/19 邻域, 求 <6 条)      tl 2400
    E3  split100 片B (mulhi 路线 51EB851F/28F5C29) tl 2400
    E4  digits2x2_core 放宽 (+10/205/6554, +lea2)  tl 3600
    E5  digits2_lean 续搜 (night2 未搜完)          tl 3600
    E6  verify_pool  统一校验 (<=10000 输入域走全域穷举证明)

候选只入池; 结论以 E6 写入 superopt_results.json 为准。
"""
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import so  # noqa: E402


def banner(msg):
    print("\n" + "#" * 74)
    print("########## [%s] %s" % (time.strftime("%H:%M:%S"), msg))
    print("#" * 74, flush=True)


def stage(title, spec, nproc=7, accept_gate=False):
    """accept_gate=True 表示这是「含已知解的最小空间」验收关, 0 候选要醒目告警。"""
    banner(title)
    t0 = time.time()
    try:
        cands = so.search_parallel(spec, nproc=nproc)
    except Exception as e:
        print("[stage] %s FAILED: %r" % (spec.get("name"), e), flush=True)
        return -1
    n = len(cands)
    print("[stage] %s -> 候选池 %d 条  用时 %.1fs"
          % (spec["name"], n, time.time() - t0), flush=True)
    for c in cands[:3]:
        print("    %s  insns=%s cyc=%s"
              % (c.get("exprs"), c.get("insns"), c.get("cyc")), flush=True)
    if accept_gate and n == 0:
        print("\n" + "!" * 74)
        print("!!! 告警: %s 是含已知解的最小空间, 竟然 0 候选" % spec["name"])
        print("!!! => 要么 so_core 还有漏解剪枝, 要么该 spec 的 goal/常数写错")
        print("!!! => 立即停止在这条线上加预算, 先查 spec 与剪枝, 别再扩搜")
        print("!" * 74 + "\n", flush=True)
    return n


def main():
    t0 = time.time()

    banner("构建 so_core (MSVC 优先)")
    try:
        so.build_core(force=True)
    except Exception as e:
        print("[build] force 失败 (%r), 沿用现有 exe" % e, flush=True)

    # ---------------- E0/E1 验收关 ----------------
    g0 = stage("E0  split100_leanest —— 验收 (已知 6 条: mul5243>>19 / sub / shl32 / or)",
               so.make_split100_leanest_spec(max_insn=6, tl=300.0),
               accept_gate=True)
    # E1 用**最窄**空间 (5 op x 3 常数 x 2 移位, 分支 30) —— 20:55 实测分支 48
    # 的版本 30.9 亿节点 420s 都没展完深度 6, 验收关必须能搜穿才有判据。
    g1 = stage("E1  digits2x2_core 最窄 —— 验收+真候选 (已知 6 条 SWAR 双 lane)",
               so.make_digits2x2_core_spec(max_insn=6, tl=1800.0),
               accept_gate=True)

    # ---------------- E2/E3 split100 常数分片 ----------------
    if g0 != 0:
        stage("E2  split100 片A: 5243/19 邻域, 求短于 6 条",
              so.make_split100_spec(
                  max_insn=5, tl=2400.0,
                  ops=["mul", "shr", "shl", "sub", "or", "and", "lea2", "add"]) |
              {"consts": [100, 5243, 0xFFFFFFFF], "shifts": [16, 19, 32],
               "name": "split100_sA"})
        stage("E3  split100 片B: mulhi 路线 (51EB851F / 28F5C29 / 42949673)",
              so.make_split100_spec(
                  max_insn=6, tl=2400.0,
                  ops=["mul", "mulhi", "shr", "shl", "sub", "or"]) |
              {"consts": [100, 0x51EB851F, 0x28F5C29, 42949673],
               "shifts": [32, 37, 45, 52], "name": "split100_sB"})
    else:
        print("[skip] E0 验收未过, 跳过 E2/E3 (别在坏空间上烧算力)", flush=True)

    # ---------------- E4 digits2x2_core 放宽一片 ----------------
    if g1 != 0:
        stage("E4  digits2x2_core 放宽片: +10/205/6554 +lea2, 求短于 6 条",
              so.make_digits2x2_core_spec(
                  max_insn=6, tl=3600.0,
                  ops=["mul", "shr", "shl", "sub", "or", "and", "add", "lea2"],
                  consts=[103, 2559, 10, 205, 6554,
                          0x000000FF000000FF, 0x0000000F0000000F],
                  shifts=[3, 8, 10, 11]) | {"name": "digits2x2_core_w1"})
    else:
        print("[skip] E1 验收未过, 跳过 E4", flush=True)

    # ---------------- E5 digits2_lean 续搜 ----------------
    stage("E5  digits2_lean 续搜 (night2 该项 762M 节点已出 1 候选, 这里加时限找更短)",
          so.make_digits2_lean_spec(max_insn=4, tl=3600.0))

    # ---------------- E6 统一校验 ----------------
    banner("E6  统一校验 (输入域 <=10000 的 kind 走全域穷举证明)")
    try:
        so.verify_pool()
    except Exception as e:
        print("[verify] FAILED: %r" % e, flush=True)

    banner("run_night4 全部完成  总耗时 %.1f min" % ((time.time() - t0) / 60.0))
    print("候选池 : %s" % so.CAND_DIR)
    print("结论表 : superopt_results.json")


if __name__ == "__main__":
    main()
