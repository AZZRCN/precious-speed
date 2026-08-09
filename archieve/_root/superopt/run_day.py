#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""白天无人值守超优化器算力投放 (7 进程分片; 只搜集候选, 不做大规模校验)。

设计要点 (对应本轮改造):
  * 并行 = 多进程分片 (so_core --shard k n), 每进程独立地址空间 => 零数据竞争,
    彻底避开 in-process std::thread 的堆损坏。
  * 快筛 = spec 里的 K 个「预设输入 -> 预设目标输出」向量; 命中即收集为候选。
  * 候选原子落盘 superopt/cand_pool/<name>.json, 记账 superopt/search_accounting.json。
  * 大规模独立校验 (40 万样本) 一律推迟到夜间:  python so.py verify

运行:  python jobs.py submit so-day -- python superopt/run_day.py --timeout 18000
"""
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import so  # noqa: E402

NPROC = int(os.environ.get("SO_NPROC", 7))


def stamp(msg):
    print(f"\n########## [{time.strftime('%H:%M:%S')}] {msg} ##########", flush=True)


def main():
    stamp(f"SO 白天投放开始  nproc={NPROC}")

    # 1) 全部已注册核跑一遍, 把候选灌进候选池 (为夜间统一校验备料)
    stamp("阶段1: 全注册核多进程搜索")
    so.autopilot(nproc=NPROC)

    # 2) 最高价值深搜: digits4 max_insn=10
    #    若搜到 => writeTo 的 40KB gather 查表 (Zen3 L1D 仅 32KB, 必然溢出) 可整张删除
    stamp("阶段2: digits4 深搜 max_insn=10 (预算 3h)")
    so.search_parallel(so.make_digits4_spec(
        max_insn=10, K=32, tl=10800.0,
        ops=["add", "sub", "mul", "mulhi", "shr", "shl", "and", "or",
             "lea2", "lea4", "lea8"]), nproc=NPROC)

    # 3) swar3: 判定 3 指令进位链是否存在
    #    找到 => 五处热点再省 1 条; 搜完无解 => 证明现有 4 条式已是最优
    stamp("阶段3: swar3 判定 (预算 30min)")
    so.search_parallel(so.make_swar_spec(max_insn=3, tl=1800.0), nproc=NPROC)

    stamp("全部完成")
    print("候选池 :", so.CAND_DIR)
    print("记账表 :", so.ACCT_PATH)
    print("夜间校验:  python so.py verify")


if __name__ == "__main__":
    main()
