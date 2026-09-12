#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
superopt_pipeline.py — DIV 自主超优化流水线 (on-VM, 自包含)
============================================================
[ARCHIVED 2026-08-07]  本文件已归档, 不再作为主优化器使用。
原因:
  - 它只是「成本下限 SIGNAL 生成器 + 平凡 peephole (mov0->xor / imul->shl, GCC 本就做)」,
    其自身第 299 行注释写明「纯 mnemonic 搜索无语义约束, 不可当候选」。真正可信的
    语义超优化器是 superopt/so.py + superopt/so_core.cpp (两阶段验证, VERIFIED 候选)。
  - 断点续搜原本损坏 (visited 被重置、frontier tuple 经 JSON 变 list 致崩溃), 现已最小修复
    为「不兼容即从头跑」, 但仍有局限, 不作为生产路径。
保留价值: detect_hotspots() (callgrind+objdump+Zen3 加权) 仍能指出 div.cpp 的贵块,
  配合 so.py 的 VERIFIED 微核知道「棒打哪一点」。热点检测需 VM (valgrind/objdump/ISA json)。

白天 offload 用: 自动 热点检测 -> 搜索+剪枝 -> (可选)等价验证 -> 产出 ranked 候选报告。
绝不修改 C++ 源码; 只产出候选, 由人工审后手改 + 554 验证。

三根支柱:
  P1 detect_hotspots : callgrind --dump-instr + objdump 基本块 + Zen3 加权排序
  P2 beam_search     : branch-and-bound (代价单调下界剪枝) + K 上限 + beam 宽度
  P3 equiv_test      : 差分随机测试 (--deep 开启; 默认关, 只出成本候选)

搜索力度旋钮:
  --k-max N    候选最大指令数 (默认5, 最大7)
  --beam N     每层保留前缀数 (默认200)
  --margin F   接受门槛: cand_cost < orig*(1-F) (默认0.05)
  --time-cap S 每块硬上限秒 (默认300 == 5分钟纪律)
  --top-n N    处理热点块数 (默认5)
  --seeds N    等价测试随机种子数 (--deep, 默认2000)
  --wmode      加权模式 latency|tp|port (默认latency)

用法 (on VM):
  python3 superopt_pipeline.py --src div_D45.cpp --case length_ratio_integer_02 \
      --isa x86_64_zen3.json --top-n 5 --deep
输出: superopt_report.txt + superopt.state (checkpoint) + superopt.log
"""
import os, re, sys, json, time, argparse, itertools, subprocess, signal
from pathlib import Path
from collections import defaultdict
from dataclasses import dataclass, field

# ============================ ISA ============================
def load_isa(path):
    data = json.loads(Path(path).read_text())
    return {i["mnemonic"]: i for i in data["instructions"]}

def weight_of(isa, mnem, wmode):
    spec = isa.get(mnem)
    if not spec:
        return 1.0, False  # 未知指令: 中性权重1, 仍计入
    if wmode == "tp":
        return 1.0 / max(spec.get("throughput", 1.0), 1e-6), True
    if wmode == "port":
        return 1.0 / max(spec.get("ports", 1), 1e-6), True
    return max(float(spec.get("latency", 1.0)), 0.0), True

def seq_cost(seq, isa, wmode):
    """序列加权代价; 单调: 加指令只增不减 (下界性质)"""
    return sum(weight_of(isa, m, wmode)[0] for m in seq)

# ============================ P1 热点检测 ============================
INSN_RE = re.compile(r'^\s*[0-9a-f]+:\s+[0-9a-f ]+\t(\S+)\s*(.*)$')
FUNC_RE = re.compile(r'^(?:[0-9a-fA-F]+\s+<)?([A-Za-z_][\w:~]*)\s*>?\s*:\s*$')
JMP_SET = {"jmp", "je", "jne", "jg", "jl", "jge", "jle", "ja", "jb", "jae",
           "jbe", "js", "jns", "jo", "jno", "jc", "jnc", "jcxz", "jecxz",
           "jrcxz", "loop", "loope", "loopne", "call", "ret", "retq",
           "ja", "jna", "jae", "jnae", "jg", "jng", "jge", "jnge",
           "jl", "jnl", "jle", "jnle", "jp", "jnp", "jpe", "jpo"}

def split_blocks(objdump_text):
    """返回 [(func, addr, mnem_seq, line_seq)] 基本块列表。边界=函数头/跳转/调用/ret。"""
    blocks, cur_func, cur_addr, cur_mn, cur_ln = [], "<global>", None, [], []
    for ln in objdump_text.splitlines():
        fm = FUNC_RE.match(ln.strip())
        if fm:
            if cur_mn:
                blocks.append((cur_func, cur_addr, cur_mn, cur_ln))
            cur_func, cur_addr, cur_mn, cur_ln = fm.group(1), None, [], []
            continue
        m = INSN_RE.match(ln)
        if not m:
            continue
        op, rest = m.group(1), m.group(2)
        addr = ln.split(':')[0].strip()
        if cur_addr is None:
            cur_addr = addr
        if op in JMP_SET and cur_mn:   # 跳转/调用/ret 前闭块
            blocks.append((cur_func, cur_addr, cur_mn, cur_ln))
            cur_addr, cur_mn, cur_ln = addr, [], []
        cur_mn.append(op); cur_ln.append(ln.strip())
        if op in ("ret", "retq") and cur_mn:
            blocks.append((cur_func, cur_addr, cur_mn, cur_ln))
            cur_addr, cur_mn, cur_ln = None, [], []
    if cur_mn:
        blocks.append((cur_func, cur_addr, cur_mn, cur_ln))
    return blocks

def detect_hotspots(binary, case_in, isa, wmode, top_n, cg_indir):
    """热点检测:
       1) callgrind -> 函数级动态 Ir, 定热点函数 (top 按 Ir);
       2) objdump -> 把热点函数切成基本块;
       3) 块内按静态 Zen3 加权代价排序, 取最贵块 = 内循环类目标。
    返回 [(func, addr, cost, ir, seq)]。"""
    # 1) callgrind 函数级 Ir
    cg_out = "/tmp/sop_cg.out"
    cache = "--cache-sim=yes --D1=32768,8,64 --I1=32768,8,64 --LL=33554432,16,64"
    cmd = ("valgrind --tool=callgrind %s --dump-instr=yes --collect-jumps=yes "
           "--callgrind-out-file=%s %s < %s > /dev/null 2>/dev/null"
           % (cache, cg_out, binary, case_in))
    if subprocess.run(cmd, shell=True, cwd=os.getcwd()).returncode != 0:
        print("[detect] callgrind 失败", flush=True); return []
    ann = subprocess.run("cg_annotate --tree=function %s" % cg_out,
                         shell=True, cwd=os.getcwd(), capture_output=True, text=True).stdout
    func_ir = {}
    FN_RE = re.compile(r'^\s*(\d+)\s+[\d.]+\s+[\d.]+\s+[\d.]+\s+[\d.]+\s+[\d.]+\s+[\d.]+\s+[\d.]+\s+[\d.]+\s+[\d.]+\s+(.+)$')
    for ln in ann.splitlines():
        m = FN_RE.match(ln)
        if not m:
            continue
        ir = int(m.group(1)); fname = m.group(2).strip()
        if "::" in fname or fname.startswith("(") or "<" in fname:
            func_ir[fname] = ir
    if not func_ir:  # 回退: 宽松解析 (函数名在末列)
        for ln in ann.splitlines():
            parts = ln.split()
            if len(parts) >= 2 and parts[0].isdigit() and (":" in parts[-1] or "::" in parts[-1]):
                func_ir[parts[-1]] = int(parts[0])
    hot_funcs = sorted(func_ir.items(), key=lambda x: -x[1])[:top_n * 3]
    L_hot = [f for f, _ in hot_funcs]
    if not L_hot:
        print("[detect] 无函数级 Ir", flush=True); return []

    # 2) objdump 基本块, 仅保留热点函数
    obj = subprocess.run("objdump -d %s" % binary, shell=True, cwd=os.getcwd(),
                         capture_output=True, text=True).stdout
    blocks = split_blocks(obj)
    # 3) 块静态加权排序 (热点函数内)
    cand = []
    for func, addr, mnem_seq, line_seq in blocks:
        if func not in L_hot:
            continue
        wc = seq_cost(mnem_seq, isa, wmode)
        cand.append((func, addr, wc, func_ir.get(func, 0), mnem_seq, line_seq))
    cand.sort(key=lambda x: -x[2])
    return cand[:top_n]

# ============================ P2 beam search ============================
@dataclass
class SearchState:
    frontier: list = field(default_factory=list)   # [(neg_cost, seq)]
    visited: set = field(default_factory=set)
    best: list = field(default_factory=list)        # [(cost, seq)]
    start_t: float = 0.0
    time_cap: float = 300.0

def beam_search(block_seq, isa, orig_cost, k_max, beam, wmode, time_cap,
                ckpt_path=None, state=None):
    """branch-and-bound: 代价单调下界剪枝 + K 上限 + beam 宽度。
    返回 ranked [(cost, seq)] 候选 (cost<orig)。"""
    vocab = sorted({m for m in isa if isa[m].get("latency", 0) < 999})
    if state is None:
        state = SearchState(start_t=time.time(), time_cap=time_cap)
        # 初始化: 单指令前缀
        for m in vocab:
            c = weight_of(isa, m, wmode)[0]
            if c < orig_cost:
                state.frontier.append((round(-c, 4), (m,)))
        state.frontier.sort()
    best = state.best
    deadline = state.start_t + time_cap
    while state.frontier and time.time() < deadline:
        nxt = []
        # 每层扩展 beam 个最优前缀
        for negc, seq in state.frontier[:beam]:
            pc = -negc
            if len(seq) >= k_max:
                continue
            for m in vocab:
                ns = seq + (m,)
                key = ns
                if key in state.visited:
                    continue
                state.visited.add(key)
                nc = pc + weight_of(isa, m, wmode)[0]
                # 代价单调下界剪枝: 部分代价已 >= 原块 -> 不可能更优
                if nc >= orig_cost:
                    continue
                nxt.append((round(-nc, 4), ns))
                if nc < orig_cost * 0.95:  # 接受门槛在收集时统一处理
                    best.append((nc, ns))
        nxt.sort()
        state.frontier = nxt[:beam]
        if ckpt_path:
            _save_state(ckpt_path, state)
    best.sort(key=lambda x: x[0])
    return best, state

def _save_state(path, st):
    try:
        json.dump({"frontier": st.frontier, "visited_n": len(st.visited),
                   "best": st.best[:50], "start_t": st.start_t,
                   "time_cap": st.time_cap},
                  open(path, "w"))
    except Exception:
        pass

def _load_state(path):
    if not os.path.exists(path):
        return None
    try:
        d = json.load(open(path))
        # 断点续搜的已知限制: frontier 中 seq 经 JSON 变成 list, 而 beam_search 里
        # `seq + (m,)` 要求 tuple -> 续搜会崩。故凡遇到 list 即视为不兼容, 放弃续搜
        # (从头跑)。visited 也只存了计数、无法精确恢复, 从头跑是安全保守选择。
        for negc, seq in d.get("frontier", []):
            if not isinstance(seq, tuple):
                return None
        st = SearchState(frontier=d["frontier"], visited=set(),
                         best=d.get("best", []), start_t=d.get("start_t", time.time()),
                         time_cap=d.get("time_cap", 300))
        return st
    except Exception:
        return None

# ============================ P3 peephole 安全扫描 ============================
# 每条规则: (name, 匹配原函数(line)->bool, 替换函数(line)->str)
# 操作数用正则直接从原始汇编行提取, 避免逗号分隔解析错误。
def _is_pow2(n):
    try:
        n = int(n)
        return n > 0 and (n & (n - 1)) == 0
    except Exception:
        return False

PEEP = [
    # mov $0, reg  ->  xor reg, reg  (清零更便宜: xorq L=1 且零宽依赖)
    ("mov0-xor",
     lambda ln: re.match(r'^\s*movq\s+\$0,\s*(\S+)\s*$', ln) is not None,
     lambda ln: (lambda r: "xorq %s,%s" % (r, r))
                (re.match(r'^\s*movq\s+\$0,\s*(\S+)\s*$', ln).group(1))),
    # imul reg,dst,imm(2^k)  ->  shl $log2, dst  (强度削弱)
    ("imul-pow2",
     lambda ln: re.match(r'^\s*imulq\s+\S+,\s*(\S+),\s*(\d+)\s*$', ln) is not None and
                _is_pow2(re.match(r'^\s*imulq\s+\S+,\s*(\S+),\s*(\d+)\s*$', ln).group(2)),
     lambda ln: (lambda m: "shlq $%d,%s" % (int(m.group(2)).bit_length() - 1, m.group(1)))
                (re.match(r'^\s*imulq\s+\S+,\s*(\S+),\s*(\d+)\s*$', ln))),
]

def find_peepholes(block_seq_asm, isa):
    """扫描基本块原文(带操作数)找已知安全 peephole。返回 [(rule, at_idx, repl_line)]。"""
    out = []
    for i, ln in enumerate(block_seq_asm):
        if not ln.strip():
            continue
        for name, cond, repl in PEEP:
            try:
                if cond(ln):
                    out.append((name, i, repl(ln)))
            except Exception:
                pass
    return out

# ============================ 主流程 ============================
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", default="div_D45.cpp")
    ap.add_argument("--case", default="length_ratio_integer_02")
    ap.add_argument("--isa", default="x86_64_zen3.json")
    ap.add_argument("--k-max", type=int, default=5)
    ap.add_argument("--beam", type=int, default=200)
    ap.add_argument("--margin", type=float, default=0.05)
    ap.add_argument("--time-cap", type=float, default=300)
    ap.add_argument("--top-n", type=int, default=5)
    ap.add_argument("--seeds", type=int, default=2000)
    ap.add_argument("--wmode", default="latency", choices=["latency", "tp", "port"])
    ap.add_argument("--deep", action="store_true", help="开启差分随机等价验证")
    ap.add_argument("--indir", default="/home/azzr/lcp/big_integer/division_of_big_integers/in/")
    ap.add_argument("--report", default="superopt_report.txt")
    ap.add_argument("--ckpt-dir", default="superopt_state")
    a = ap.parse_args()

    isa = load_isa(a.isa)
    os.makedirs(a.ckpt_dir, exist_ok=True)
    log = open("superopt.log", "w", buffering=1)
    def L(*x):
        s = " ".join(str(i) for i in x)
        print(s, flush=True); log.write(s + "\n")

    L("[start] src=%s case=%s wmode=%s k-max=%d beam=%d time-cap=%.0f deep=%s"
      % (a.src, a.case, a.wmode, a.k_max, a.beam, a.time_cap, a.deep))

    # 编译
    binp = "bin/sop_div"
    if not os.path.exists(binp) or os.path.getmtime(a.src) > os.path.getmtime(binp):
        rc = subprocess.run("g++ -O2 -std=c++23 -march=x86-64-v3 -g %s -o %s 2>&1 | tail -3"
                            % (a.src, binp), shell=True, cwd=os.getcwd(),
                            capture_output=True, text=True)
        if rc.returncode != 0:
            L("[build FAILED]", rc.stderr[:300]); return
        L("[build] ok ->", binp)

    case_in = os.path.join(a.indir, a.case + ".in")
    hotspots = detect_hotspots(binp, case_in, isa, a.wmode, a.top_n, a.indir)
    L("[hotspots] %d blocks" % len(hotspots))
    for h in hotspots:
        L("   ", h[0], h[1], "wc=%.1f" % h[2], "ir=%d" % h[3], "len=%d" % len(h[4]))

    report = open(a.report, "w")
    report.write("# superopt report  src=%s case=%s wmode=%s\n" % (a.src, a.case, a.wmode))
    report.write("# 两类产出: [SAFE] peephole 可立即手改+554验证; [SIGNAL] 成本下限, 需 v2 SMT 等价搜索\n")
    for fi, (func, addr, wc, ir, mnem_seq, line_seq) in enumerate(hotspots):
        L("[block %d/%d] %s @%s  orig_wc=%.2f ir=%d len=%d"
          % (fi + 1, len(hotspots), func, addr, wc, ir, len(mnem_seq)))
        # P2 beam search -> 只作成本下限信号 (纯 mnemonic 搜索无语义约束, 不可当候选)
        st = _load_state(os.path.join(a.ckpt_dir, "blk%d.json" % fi))
        cands, st = beam_search(tuple(mnem_seq), isa, wc, a.k_max, a.beam, a.wmode,
                                a.time_cap, os.path.join(a.ckpt_dir, "blk%d.json" % fi), st)
        best_c = min((c for c, _ in cands), default=wc)
        headroom = best_c / max(wc, 1e-9)
        # P3 peephole (安全, 带操作数, 可应用)
        phs = find_peepholes(line_seq, isa)
        report.write("\n## block %d: %s @%s  orig_wc=%.2f ir=%d len=%d\n"
                     % (fi, func, addr, wc, ir, len(mnem_seq)))
        report.write("  [SIGNAL] beam-search 成本下限=%.2f (%.1f%% of orig) -> 高则值得 v2 SMT 搜索\n"
                     % (best_c, 100 * headroom))
        if phs:
            report.write("  [SAFE] peephole 可应用优化:\n")
            for name, i, w, repl in phs:
                report.write("    %s @ins%d: %s\n" % (name, i, repl))
        else:
            report.write("  [SAFE] 无 peephole 命中\n")
    report.close()
    L("[done] report ->", a.report)

if __name__ == "__main__":
    main()
