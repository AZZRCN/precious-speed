# -*- coding: utf-8 -*-
"""2026-08-07 夜间第二轮投放 (run_night2 的接力棒)

为什么要接力而不是并行:
    机器 8 逻辑核, night2 已用 nproc=7 占满。并行提交只会互相抢 CPU,
    总吞吐反而下降。本脚本开头**阻塞等待** night2 结束 (轮询 jobs.py status),
    腾出核之后再开跑, 全程无人值守。

本轮目标 (按「价值 x 成功率」降序):
    D0  digits2_leanest  —— 端到端验收: 已知 5 条解的最小空间, 秒级应出。
                            若 0 候选 => so_core 除 P6 外仍有漏解剪枝, 后面
                            所有「无解」结论一律不可信, 脚本会打印醒目告警。
    D1  digits2_lean     —— 完整 lean 空间深度 5 长时限
    D2  split100         —— x<10000 -> (x/100, x%100) 双 lane, 是 digits4 的上半
    D3  digits2x2        —— 双 lane 出 4 数字, 拿下 40KB outTable 的核心
    D4  str4toi_lean d6  —— 击杀 64KB parseTable (最大的一块)
    D5  verify           —— 统一夜间校验: 输入域 <=10000 的 kind 走**全域穷举
                            证明**(强于抽样), 其余 40 万随机样本两级校验。

候选只入池, 结论以 D5 verify 写入 superopt_results.json 为准。
"""
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import so  # noqa: E402

WAIT_FOR = "so-night2-98489318"   # 前一棒的 job id


def banner(msg):
    print("\n" + "#" * 74)
    print("########## [%s] %s" % (time.strftime("%H:%M:%S"), msg))
    print("#" * 74, flush=True)


def prev_job_alive(job):
    """轮询 jobs.py status; 任何异常都按「已结束」处理, 绝不把自己卡死。"""
    try:
        r = subprocess.run([sys.executable, os.path.join(ROOT, "jobs.py"), "status", job],
                           capture_output=True, text=True, timeout=60)
        return "RUN" in (r.stdout or "")
    except Exception:
        return False


def wait_prev(max_wait=4 * 3600):
    banner("等待前一棒 %s 结束 (最多 %.1f h)" % (WAIT_FOR, max_wait / 3600.0))
    t0 = time.time()
    while time.time() - t0 < max_wait:
        if not prev_job_alive(WAIT_FOR):
            print("[chain] 前一棒已结束, 等待 %.1f min" % ((time.time() - t0) / 60.0), flush=True)
            return
        time.sleep(60)
    print("[chain] 等待超时, 强行开跑 (可能与前一棒抢 CPU)", flush=True)


def stage(title, spec, nproc=7):
    banner(title)
    try:
        cands = so.search_parallel(spec, nproc=nproc)
        print("[stage] %s -> 候选池 %d 条" % (spec["name"], len(cands)), flush=True)
        return len(cands)
    except Exception as e:
        print("[stage] %s FAILED: %r" % (spec.get("name"), e), flush=True)
        return -1


def main():
    t0 = time.time()
    wait_prev()

    banner("构建 so_core (MSVC 优先; 前一棒已退出, exe 可安全覆盖)")
    try:
        so.build_core(force=True)
    except Exception as e:
        print("[build] force 失败 (%r), 沿用现有 exe" % e, flush=True)

    # ---------- D0 端到端验收 (最重要, 最便宜) ----------
    n0 = stage("D0  digits2_leanest —— P6 修复端到端验收 (已知 5 条解)",
               so.make_digits2_leanest_spec(max_insn=5, tl=300.0))
    if n0 == 0:
        print("\n" + "!" * 74)
        print("!!! 告警: 最小空间仍搜不到已知 5 条解 => so_core 还有漏解剪枝 !!!")
        print("!!! 后续所有「无解」结论不可信; 优先查 P1/P4/P7, 而非继续扩搜 !!!")
        print("!" * 74 + "\n", flush=True)

    # ---------- D1-D4 正式搜索 ----------
    stage("D1  digits2_lean 深度 5 长时限", so.make_digits2_lean_spec(max_insn=5, tl=3600.0))
    stage("D2  split100 深度 5", so.make_split100_spec(max_insn=5, tl=2400.0))
    stage("D3  digits2x2 深度 5 (40KB outTable 核心)", so.make_digits2x2_spec(max_insn=5, tl=3600.0))
    stage("D4  str4toi_lean 深度 6 (64KB parseTable)", so.make_str4toi_lean_spec(max_insn=6, tl=5400.0))

    # ---------- D5 统一校验 ----------
    banner("D5  夜间统一校验 (全域穷举 + 40 万样本)")
    try:
        so.verify_pool()          # 注意: verify(spec,cand) 是单候选级, 池级入口是 verify_pool
    except Exception as e:
        print("[verify] FAILED: %r" % e, flush=True)

    banner("run_night3 全部完成  总耗时 %.1f min" % ((time.time() - t0) / 60.0))
    print("候选池 : %s" % so.CAND_DIR)
    print("结论表 : superopt_results.json")


if __name__ == "__main__":
    main()
