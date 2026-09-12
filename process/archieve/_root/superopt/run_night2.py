# -*- coding: utf-8 -*-
"""2026-08-07 夜间无人值守投放 (P6 剪枝 off-by-miss 修复后的第一轮)

背景 —— 本轮修了一个**静默漏解**的真 bug:
    so_core.cpp 的 P6 死代码前瞻剪枝原判据 `unused > remaining`,
    忽略了「产出 goal 的那一步净减 2 个悬空值」, 正确判据是
    `unused > remaining + miss`。
    后果: 所有「深子树 + 末步合并两个悬空值」的程序被整类剪掉。
    典型反例 digits2:  sub( shl(y,8), mul(2559, shr(mul(y,103),10)) )
    —— 5 条合法解, 旧搜索在深度 5 「搜完全空间」却报 0 候选。

因此本轮的第一要务不是找新解, 而是**重开此前被误判为「无解 / 已到头」的门**:
    swar3      : 曾判「3 条借位链不存在」
    carrystep  : 曾判「carry 核已到头」
    divmod_r   : 曾以 2 条式定案
    digits4    : 曾烧 653 亿节点判无解 (该结论现已不可信)

阶段按「价值 x 成功率」降序; 前面的阶段先出结果, 后面的超时也不影响前面。
候选只存池 (cand_pool/), 统一由 `python so.py verify` 夜间校验。
"""
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import so  # noqa: E402


def banner(msg):
    print("\n" + "#" * 74)
    print("########## [%s] %s" % (time.strftime("%H:%M:%S"), msg))
    print("#" * 74, flush=True)


def stage(title, spec, nproc=7):
    banner(title)
    try:
        cands = so.search_parallel(spec, nproc=nproc)
        print("[stage] %s -> 候选池 %d 条" % (spec["name"], len(cands)), flush=True)
    except Exception as e:            # 单阶段炸掉不许拖垮整轮投放
        print("[stage] %s FAILED: %r" % (spec.get("name"), e), flush=True)


def main():
    t0 = time.time()
    banner("构建 so_core (MSVC 优先; 已修 __int128 -> _umul128)")
    so.build_core(force=True)

    # ---------- 阶段 A: 重开被误判的门 (最高价值, 成本低) ----------
    # 旧结论全部产生于有 bug 的剪枝之下, 必须重判。
    stage("A1  swar3 重判 —— 3 条借位链真的不存在吗",
          so.make_swar_spec(max_insn=3, tl=600.0))
    stage("A2  swar4 重搜 —— 修复后是否有更短/更便宜的 4 条式",
          so.make_swar_spec(max_insn=4, tl=900.0))
    stage("A3  carrystep 重判 —— carry 核真的已到头吗",
          so.make_carrystep_spec(max_insn=4, tl=900.0))
    stage("A4  divmod_r 重搜 (深度 3)",
          so.make_divmod_spec(only_r=True, max_insn=3, tl=600.0))

    # ---------- 阶段 B: 新目标, 消灭两张随机 gather 查表 ----------
    # digits2 已 Python 全域证明存在 5 条解 => 这一阶段同时是**修复的端到端验收**:
    # 若这里还搜不出来, 说明 P6 之外仍有漏解剪枝。
    stage("B1  digits2_lean (已知 5 条解, 兼作修复验收)",
          so.make_digits2_lean_spec(max_insn=5, tl=1500.0))
    stage("B2  digits2 完整常数集 (找 <=5 条更优式)",
          so.make_digits2_spec(max_insn=5, tl=1200.0))
    stage("B3  split100  x<10000 -> (x/100, x%100) 双 lane",
          so.make_split100_spec(max_insn=5, tl=1200.0))

    # ---------- 阶段 C: 大件 (能跑多少算多少) ----------
    stage("C1  str4toi_lean 深度 6 (击杀 64KB parseTable)",
          so.make_str4toi_lean_spec(max_insn=6, tl=1800.0))
    stage("C2  digits2x2 SWAR 双 lane 出 4 数字",
          so.make_digits2x2_spec(max_insn=5, tl=1800.0))

    banner("全部完成  总耗时 %.1f min" % ((time.time() - t0) / 60.0))
    print("候选池 : %s" % so.CAND_DIR)
    print("记账表 : %s" % so.ACCT_PATH)
    print("夜间校验:  python so.py verify")


if __name__ == "__main__":
    main()
