# -*- coding: utf-8 -*-
"""编译候选并统计 carryPropSeg 四段并行热循环的**每 limb 指令数**。

方法: 反汇编取 carryPropSeg 函数体, 找所有回跳 (jXX target < 本指令地址),
      取「循环体内含 >=4 条 vcvtsd2si」的那个回跳 = 四段并行主循环,
      统计 [target, 回跳] 区间指令数 / 4 = 指令/limb。
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import run, put  # noqa: E402

PS = r"D:\precious_speed"
RD = "/home/azzr/divbench"
CANDS = sys.argv[1:] or ["D42", "D44"]
SYM = "_ZN4hint7Integer12carryPropSegEPKdPtm"

LINE = re.compile(r"^\s*([0-9a-f]+):\s+(\S+)\s*(.*)$")
JMP = re.compile(r"^j\w+$")

for tag in CANDS:
    lp = os.path.join(PS, "best", "div_%s.cpp" % tag)
    if not os.path.exists(lp):
        print("skip", tag)
        continue
    b = tag.lower()
    put(lp, "%s/div_%s.cpp" % (RD, tag))
    rc, o, e = run("cd %s && g++ -O2 -std=c++23 -march=x86-64-v3 div_%s.cpp -o bin/%s 2>&1|tail -3"
                   % (RD, tag, b), timeout=1200)
    if o.strip():
        print("[build %s] %s" % (tag, o.strip()[:200]))
    rc, o, e = run("cd %s && objdump -d --no-show-raw-insn bin/%s | "
                   "awk '/<%s>:/{f=1} f{print} f&&NF==0{exit}'" % (RD, b, SYM), timeout=600)
    insns = []
    for L in o.splitlines():
        m = LINE.match(L)
        if m:
            insns.append((int(m.group(1), 16), m.group(2), m.group(3)))
    if not insns:
        print("[%s] 反汇编为空" % tag)
        continue
    addr2idx = {a: i for i, (a, _, _) in enumerate(insns)}
    seen = set()
    rows = []
    for i, (a, op, arg) in enumerate(insns):
        if not JMP.match(op):
            continue
        m = re.match(r"^([0-9a-f]+)", arg.strip())
        if not m:
            continue
        t = int(m.group(1), 16)
        if t >= a or t not in addr2idx:
            continue
        j = addr2idx[t]
        body = insns[j:i + 1]
        nc = sum(1 for _, o2, _ in body if o2.startswith("vcvtsd2si"))
        if nc < 2 or t in seen:
            continue
        seen.add(t)
        npf = sum(1 for _, o2, _ in body if o2.startswith("prefetch"))
        nsp = sum(1 for _, o2, a2 in body if o2 == "mov" and "(%rsp)" in a2)
        nmov = sum(1 for _, o2, a2 in body if o2 == "movabs")
        nmul = sum(1 for _, o2, _ in body if o2 in ("mul", "mulx"))
        eff = len(body) - npf * 7.0 / 8.0
        rows.append((nc, len(body), nmul, nmov, nsp, npf, eff / nc, t))
    if not rows:
        print("[%s] 未找到循环" % tag)
        continue
    for nc, nb, nmul, nmov, nsp, npf, per, t in sorted(rows):
        print("[%s] @%x limb/iter=%d loop=%-3d mul=%d movabs=%d rsp_mov=%d prefetch=%d"
              "  => %.2f insn/limb" % (tag, t, nc, nb, nmul, nmov, nsp, npf, per))
