# -*- coding: utf-8 -*-
"""
D32 = D31 + 源码内编译选项特调 (LC 无法改命令行, 必须内嵌到源码)

命令行实测 (d31b = -O3 -DNDEBUG): sum 501->487 (-2.8%), 26 例逐字节全绿。
但 LC 固定 -O2, 所以这 2.8% 只有搬进源码才算数。分两变体做归因:

  D32a: 仅 #define NDEBUG
        纯预处理器行为, 必然生效。干掉源码里 81 个活 assert。
        必须在 #include <cassert> 之前定义。

  D32b: NDEBUG + #pragma GCC optimize("O3","unroll-loops")
        div_D31.cpp 第 12 行历史注释断言「#pragma GCC optimize 无效」,
        本变体就是来证伪/证实它的 —— 用 callgrind Ir 判定:
        若 D32b 的 Ir 明显低于 D32a 且逼近命令行 -O3 版, 则 pragma 生效。

顺序陷阱: GCC 已知行为 —— #pragma GCC optimize 会重置当前 target 选项。
若 optimize 放在 target("avx2,bmi,bmi2,...") 之后, AVX2/BMI2 会被悄悄关掉
(SWAR absSub 依赖 _pext_u32, 关掉直接编译失败或退化)。故 optimize 必须前置。
"""
import re, sys, io, os

SRC = os.path.join(os.path.dirname(__file__), "..", "best", "div_D31.cpp")
OUT_A = os.path.join(os.path.dirname(__file__), "..", "best", "div_D32a.cpp")
OUT_B = os.path.join(os.path.dirname(__file__), "..", "best", "div_D32b.cpp")

with io.open(SRC, "r", encoding="utf-8", errors="surrogateescape") as f:
    src = f.read()

ANCHOR = "#define HINT_OP_DIV"
if ANCHOR not in src:
    sys.exit("anchor '#define HINT_OP_DIV' not found")

# 卫生检查: NDEBUG 必须先于 <cassert>
i_anchor = src.index(ANCHOR)
i_cassert = src.index("#include <cassert>")
assert i_anchor < i_cassert, "anchor must precede <cassert>"

BLOCK_A = ANCHOR + """

// ==================== D32: 源码内编译特调 (LC 固定 -O2, 改不了命令行) ====================
// 本文件 81 处 assert 在 LC 上全部是活的 —— 判题命令不带 -DNDEBUG。
// 已逐条 grep 验证: 所有 assert 表达式均无副作用 (无 ++/--/赋值/函数调用),
// 故定义 NDEBUG 不改变任何语义, 纯减指令。必须先于 <cassert> 生效。
// 命令行对照 (d31 vs d31nd, perf task-clock): sum 500 -> 494 ms
#define NDEBUG
"""

BLOCK_B = BLOCK_A + """
// #pragma GCC optimize 必须放在 #pragma GCC target 之前:
// GCC 已知行为 —— optimize pragma 会重置当前 target 选项集, 若后置会把
// avx2/bmi2 悄悄关掉, 而 absSub_avx2 的 SWAR 借位链依赖 _pext_u32 (BMI2)。
// 命令行对照 (d31 vs d31b = -O3 -DNDEBUG): sum 501 -> 487 ms (-2.8%)
#pragma GCC optimize("O3", "unroll-loops")
"""

for out, block in ((OUT_A, BLOCK_A), (OUT_B, BLOCK_B)):
    dst = src.replace(ANCHOR, block, 1)
    tag = os.path.basename(out).replace("div_", "").replace(".cpp", "").upper()
    dst = dst.replace(
        "// D31 = D27 + Knuth D3 qhat refinement + Granlund-Montgomery reciprocal",
        "// D31 = D27 + Knuth D3 qhat refinement + Granlund-Montgomery reciprocal\n"
        "// %s = D31 + in-source compile tuning (%s)"
        % (tag, "NDEBUG" if block is BLOCK_A else "NDEBUG + pragma O3"),
        1,
    )
    with io.open(out, "w", encoding="utf-8", errors="surrogateescape", newline="") as f:
        f.write(dst)
    print("wrote %s (%d bytes)" % (out, len(dst)))
