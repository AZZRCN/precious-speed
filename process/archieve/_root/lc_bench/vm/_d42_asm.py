# -*- coding: utf-8 -*-
"""编译 D41/D42/D43 并对 carryPropSeg 做反汇编指令统计对比。

只看源码级可验证的事实:
  vcomisd/jae  = double->unsigned 护栏 (应在 D42 归零)
  vaddsd       = +0.5 (应在 D42 归零)
  vcvttsd2si   = 截断转换 (D42 应换成 vcvtsd2si)
  movabs .. 0x68db8bac710cc = Barrett 魔数重载 (D43 应显著减少)
  mul / mulx   = Barrett 乘法形式
"""
import re
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import run, put  # noqa: E402

PS = r"D:\precious_speed"
CANDS = sys.argv[1:] or ["D41", "D42", "D43"]
RD = "/home/azzr/bench"

FLAGS = "-O2 -std=c++23 -march=x86-64-v3"

PATS = [
    ("vcomisd", re.compile(r"\bvcomisd\b")),
    ("jae", re.compile(r"\bjae\b")),
    ("vaddsd", re.compile(r"\bvaddsd\b")),
    ("vcvttsd2si", re.compile(r"\bvcvttsd2si\b")),
    ("vcvtsd2si", re.compile(r"\bvcvtsd2si\b")),
    ("movabs_barrett", re.compile(r"movabs\s+\$0x68db8bac710cc")),
    ("mul_r64", re.compile(r"\bmul\s+%r")),
    ("mulx", re.compile(r"\bmulx\b")),
    ("spill_rsp", re.compile(r"mov\s+%r\w+,-0x[0-9a-f]+\(%rsp\)")),
]

print("host ok?", run("hostname")[1].strip())

for c in CANDS:
    lp = os.path.join(PS, "best", "div_%s.cpp" % c)
    if not os.path.exists(lp):
        print("skip (missing)", c)
        continue
    put(lp, "%s/div_%s.cpp" % (RD, c))

for c in CANDS:
    rc, out, err = run(
        "cd %s && g++ %s div_%s.cpp -o d_%s 2>&1 | tail -5" % (RD, FLAGS, c, c),
        timeout=900)
    print("[build %s] rc=%d %s" % (c, rc, out.strip()[:300]))

for c in CANDS:
    rc, out, err = run(
        "cd %s && objdump -d --no-show-raw-insn d_%s | "
        "awk '/<_?Z?[^>]*carryPropSeg[^>]*>:/{f=1} f{print} f&&/^$/{exit}'" % (RD, c),
        timeout=600)
    lines = out.splitlines()
    if len(lines) < 5:
        print("[asm %s] carryPropSeg 未找到 (可能被内联), 改扫全函数体" % c)
        continue
    stat = {}
    for name, p in PATS:
        stat[name] = sum(1 for L in lines if p.search(L))
    print("[asm %s] insns=%-6d %s" % (c, len(lines) - 1,
                                      " ".join("%s=%d" % (k, stat[k]) for k, _ in PATS)))
