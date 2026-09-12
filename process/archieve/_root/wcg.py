#!/usr/bin/env python3
r"""
加权 callgrind — 用 Zen3 (EPYC 7B13) ISA 代价模型给指令计数加权。
本地权威判据（不耗 LC 提交，可多跑取稳）。

两种模式:
  dis  解析 objdump -d 反汇编, 统计每函数静态指令混合的加权代价(快速差分同函数跨候选)
  cg   解析 cg_annotate --dump-instr=yes 的逐指令动态 Ir, 直接加权求和(真·加权 callgrind)

权重模式 (--mode):
  latency     用指令延迟(多周期指令权重高)        [默认, 符合"多周期指令权重高"]
  tp          用 1/throughput (吞吐瓶颈权重高)
  port        用 1/ports (端口压力权重高)

用法:
  python wcg.py dis  bin/D45.objdump --isa D:/gcc\ modifield/_o4_impl/isa/x86_64_zen3.json
  python wcg.py cg   cg_instr.txt --func carryPropSeg
  python wcg.py cg   cg_D41.txt cg_D45.txt --mode latency   # 多候选比比值
"""
import sys, json, argparse, re
from pathlib import Path
from collections import defaultdict

DEFAULT_ISA = r"D:/gcc modifield/_o4_impl/isa/x86_64_zen3.json"

FUNC_RE = re.compile(r'^(?:[0-9a-fA-F]+\s+<)?([A-Za-z_][\w:~]*)\s*>?\s*:\s*$')
# cg_annotate --dump-instr 行: "      123  mov    %rax,%rdx" 或 "         0  => ???"
INSTR_RE = re.compile(r'^\s*(\d+)\s+([a-zA-Z][\w.]*)\s*(.*)$')
# objdump 行: "  400:	48 89 f0    	mov    %rsi,%rax"
OBJ_RE = re.compile(r'^\s*[0-9a-f]+:\s+[0-9a-f ]+\t(\S+)\s*(.*)$')


def load_isa(path):
    return {i["mnemonic"]: i for i in json.loads(Path(path).read_text())["instructions"]}


def weight_of(isa, mnem, mode):
    spec = isa.get(mnem)
    if not spec:
        # 未知指令: 给中性权重 1, 但仍计入
        return 1.0, False
    if mode == "tp":
        return 1.0 / max(spec["throughput"], 1e-6), True
    if mode == "port":
        return max(1.0 / max(spec["ports"], 1), 1e-6), True
    return max(float(spec["latency"]), 0.0), True  # latency (默认)


def norm_mnem(mnem, rest, isa):
    m = mnem
    # 64-bit 默认补 q
    if m in ("mov", "add", "sub", "and", "or", "xor", "cmp", "test", "inc",
             "dec", "neg", "not", "shl", "shr", "sar", "imul", "lea", "movq"):
        if m not in isa and (m + "q") in isa:
            m = m + "q"
    # 内存操作识别 load/store
    if "(" in rest:
        ops = [o for o in rest.split(",")]
        is_load = "(" in ops[0]
        if m in ("movq", "mov"):
            m = "mov_load" if is_load else "mov_store"
        elif m in ("vmovdqu", "vmovdqa"):
            m = "vmovdqu_load" if is_load else "vmovdqu_store"
    return m


def parse_cg(text, isa, mode, target_func=None):
    """返回 {func: (weighted_cost, ir_total)} 与 unknown 计数"""
    res = defaultdict(lambda: [0.0, 0])
    cur = "<global>"
    unknown = defaultdict(int)
    for line in text.splitlines():
        fm = FUNC_RE.match(line)
        if fm:
            cur = fm.group(1)
            continue
        im = INSTR_RE.match(line)
        if not im:
            continue
        ir = int(im.group(1))
        if ir == 0 and "???" in line:
            continue
        m = norm_mnem(im.group(2), im.group(3), isa)
        w, known = weight_of(isa, m, mode)
        if not known and m not in isa:
            unknown[m] += ir
        res[cur][0] += ir * w
        res[cur][1] += ir
    return res, unknown


def parse_obj(text, isa, mode):
    """静态指令混合加权(同一函数跨候选差分用)"""
    res = defaultdict(lambda: [0.0, 0])
    cur = "<global>"
    for line in text.splitlines():
        fm = FUNC_RE.match(line)
        if fm:
            cur = fm.group(1)
            continue
        om = OBJ_RE.match(line)
        if not om:
            continue
        m = norm_mnem(om.group(1), om.group(2), isa)
        w, _ = weight_of(isa, m, mode)
        res[cur][0] += w
        res[cur][1] += 1
    return res, {}


def main():
    ap = argparse.ArgumentParser(description="Weighted callgrind (Zen3 cost model)")
    ap.add_argument("mode_cmd", choices=["cg", "dis"])
    ap.add_argument("files", nargs="+", help="cg_annotate 或 objdump 文件")
    ap.add_argument("--isa", default=DEFAULT_ISA)
    ap.add_argument("--wmode", default="latency",
                    choices=["latency", "tp", "port"])
    ap.add_argument("--func", default=None, help="只打印该函数")
    args = ap.parse_args()

    isa = load_isa(args.isa)
    parse = parse_cg if args.mode_cmd == "cg" else parse_obj

    all_res = []
    for f in args.files:
        text = Path(f).read_text(errors="ignore")
        res, unknown = parse(text, isa, args.wmode)
        all_res.append((f, res, unknown))

    # 打印
    for idx, (f, res, unknown) in enumerate(all_res):
        print(f"\n=== [{idx}] {Path(f).name}  (wmode={args.wmode}) ===")
        funcs = sorted(res.items(), key=lambda kv: -kv[1][0])
        for fn, (wc, ir) in funcs:
            if args.func and fn != args.func:
                continue
            ratio = wc / max(ir, 1)
            print(f"  {fn:32s}  weighted={wc:14.1f}  Ir={ir:12d}  w/Ir={ratio:6.3f}")
        if unknown:
            print(f"  [unknown mnemonics weighted as 1]: {dict(unknown)}")

    # 多候选比比值(基于 --func 或首个函数)
    if len(all_res) >= 2:
        base_f, base_res, _ = all_res[0]
        print(f"\n=== ratio vs {Path(base_f).name} (wmode={args.wmode}) ===")
        cmp_funcs = [args.func] if args.func else list(base_res.keys())
        for fn in cmp_funcs:
            vals = []
            for f, res, _ in all_res:
                if fn in res:
                    vals.append(res[fn][0])
                else:
                    vals.append(None)
            if None in vals or not vals:
                continue
            base = vals[0]
            rs = " ".join(f"{v/base:.4f}" if base else "?" for v in vals)
            print(f"  {fn:32s}  ratio= {rs}")


if __name__ == "__main__":
    main()
