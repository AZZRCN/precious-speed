#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
outline.py — C/C++ 源码大纲扫描器

用途：快速摸清一份陌生大源码的结构，不用全文读进上下文。

输出：
  1. 文件级统计（行数/字节/注释块）
  2. 顶层结构大纲（#include / #define / using / struct / 函数签名 / 全局数组）
  3. 关键词雷达（阈值/分档/算法名/内建函数/SIMD 宽度）
  4. 常量池（所有形如 NAME = 数字 的编译期常量，用来找阈值）

用法：
  python outline.py <file...>            # 大纲
  python outline.py --body <file> <名字>  # 打印某个函数/结构体的完整体
  python outline.py --grep <file> <正则>  # 带行号的定向抓取
"""
import io
import os
import re
import sys

# ---------- 关键词雷达：分组 -> 正则 ----------
RADAR = {
    "算法分档": r"\b(karatsuba|toom|schoolbook|comba|fft|ntt|nussbaumer|barrett|montgomery|newton|goldschmidt|divrem|burnikel|ziegler)\b",
    "阈值常量": r"\b([A-Z_][A-Z0-9_]{2,})\s*=\s*(\d+)",
    "SIMD": r"(__m512|__m256|__m128|_mm512_|_mm256_|_mm_|vpternlog|avx512|avx2)",
    "内建": r"(__builtin_\w+|__int128|__uint128_t|_umul128|_addcarry|_subborrow)",
    "IO": r"(fread|read\(|mmap|getchar|fwrite|write\(|putchar|setvbuf|sys_read)",
    "内存": r"(madvise|MADV_HUGEPAGE|aligned_alloc|alignas|posix_memalign|static\s+.*\[\s*\d)",
    "编译指示": r"(#pragma\s+\w+|__attribute__\s*\(\(\s*\w+)",
}

FUNC_RE = re.compile(
    r"^(?:template\s*<[^>]*>\s*)?"
    r"((?:static|inline|constexpr|extern|\[\[[^\]]+\]\]|__attribute__\s*\(\([^)]*\)\)|\s)*)"
    r"([A-Za-z_][\w:<>,\s\*&]*?)\s+"
    r"([A-Za-z_]\w*)\s*\("
)
TYPE_RE = re.compile(r"^\s*(struct|class|union|enum(?:\s+class)?)\s+([A-Za-z_]\w*)")
GLOBAL_ARR_RE = re.compile(
    r"^\s*(?:static\s+|alignas\s*\(\s*\d+\s*\)\s*|constexpr\s+|const\s+|inline\s+)*"
    r"([A-Za-z_][\w:]*)\s+([A-Za-z_]\w*)\s*\[\s*([^\]]*)\s*\]"
)
DEFINE_RE = re.compile(r"^\s*#\s*define\s+(\w+)(?:\(([^)]*)\))?\s*(.*)")
CONST_RE = re.compile(
    r"(?:static\s+)?(?:constexpr|const)\s+"
    r"(?:unsigned\s+|signed\s+)?(?:int|long|size_t|u32|u64|i32|i64|uint32_t|uint64_t|double|float)"
    r"(?:\s+long)?\s+([A-Za-z_]\w*)\s*=\s*([^;]+);"
)


def strip_strings(line):
    """粗暴去掉字符串字面量，避免误判"""
    return re.sub(r'"(?:[^"\\]|\\.)*"', '""', line)


def scan(path):
    raw = io.open(path, encoding="utf-8", errors="replace").read()
    lines = raw.split("\n")
    out = []
    ap = out.append

    ap("=" * 78)
    ap("FILE: %s" % path)
    ap("  bytes=%d  lines=%d" % (len(raw), len(lines)))

    # ---- 顶部注释块（往往写着算法说明） ----
    head = []
    for ln in lines[:60]:
        s = ln.strip()
        if s.startswith("//") or s.startswith("/*") or s.startswith("*"):
            head.append(s)
        elif s == "":
            continue
        elif head:
            break
    if head:
        ap("-" * 78)
        ap("HEADER COMMENT (%d lines):" % len(head))
        for h in head[:40]:
            ap("  " + h[:150])

    # ---- 大纲扫描 ----
    includes, defines, types, funcs, arrays, consts = [], [], [], [], [], []
    depth = 0
    in_block_comment = False
    for i, ln in enumerate(lines, 1):
        s = strip_strings(ln)
        # 块注释状态机（粗略）
        if in_block_comment:
            if "*/" in s:
                in_block_comment = False
            continue
        if "/*" in s and "*/" not in s:
            in_block_comment = True
            continue
        code = s.split("//")[0]
        stripped = code.strip()

        if stripped.startswith("#include"):
            includes.append(stripped)
            continue
        m = DEFINE_RE.match(code)
        if m:
            defines.append((i, m.group(1), (m.group(2) or ""), m.group(3).strip()[:70]))
            continue

        # 顶层判定（depth==0 时出现的定义才算顶层）
        if depth == 0:
            m = TYPE_RE.match(code)
            if m:
                types.append((i, m.group(1), m.group(2)))
            else:
                m = FUNC_RE.match(code)
                if m and m.group(3) not in ("if", "for", "while", "switch", "return", "sizeof"):
                    quals = " ".join(m.group(1).split())
                    sig = stripped[:110]
                    funcs.append((i, quals, m.group(3), sig))
                else:
                    m = GLOBAL_ARR_RE.match(code)
                    if m and "return" not in stripped and "(" not in m.group(2):
                        arrays.append((i, m.group(1), m.group(2), m.group(3)[:40]))

        for m in CONST_RE.finditer(code):
            consts.append((i, m.group(1), m.group(2).strip()[:50]))

        depth += code.count("{") - code.count("}")
        if depth < 0:
            depth = 0

    ap("-" * 78)
    ap("INCLUDES (%d): %s" % (len(includes), " ".join(x.replace("#include", "").strip() for x in includes)))

    if defines:
        ap("-" * 78)
        ap("DEFINES (%d):" % len(defines))
        for i, n, a, v in defines[:60]:
            ap("  L%-6d %-26s %s %s" % (i, n, ("(" + a + ")") if a else "  ", v))

    if types:
        ap("-" * 78)
        ap("TYPES (%d):" % len(types))
        for i, k, n in types:
            ap("  L%-6d %-8s %s" % (i, k, n))

    if arrays:
        ap("-" * 78)
        ap("GLOBAL ARRAYS (%d):" % len(arrays))
        for i, t, n, d in arrays[:50]:
            ap("  L%-6d %-14s %-24s [%s]" % (i, t, n, d))

    if consts:
        ap("-" * 78)
        ap("CONSTANTS (%d)  <-- 阈值/分档常常藏在这里:" % len(consts))
        seen = set()
        for i, n, v in consts:
            if n in seen:
                continue
            seen.add(n)
            ap("  L%-6d %-30s = %s" % (i, n, v))

    if funcs:
        ap("-" * 78)
        ap("FUNCTIONS (%d):" % len(funcs))
        for i, q, n, sig in funcs:
            ap("  L%-6d %s" % (i, sig))

    # ---- 关键词雷达 ----
    ap("-" * 78)
    ap("RADAR:")
    low = raw.lower()
    for g, pat in RADAR.items():
        hits = {}
        for m in re.finditer(pat, low if g != "阈值常量" else raw):
            k = m.group(0) if g != "阈值常量" else m.group(1)
            hits[k] = hits.get(k, 0) + 1
        if hits:
            top = sorted(hits.items(), key=lambda x: -x[1])[:14]
            ap("  %-8s %s" % (g, "  ".join("%s(%d)" % (k, v) for k, v in top)))

    return "\n".join(out)


def body(path, name):
    """打印某函数/结构体的完整体（花括号配平）"""
    lines = io.open(path, encoding="utf-8", errors="replace").read().split("\n")
    pat = re.compile(r"\b" + re.escape(name) + r"\b\s*[\(\{:]")
    for i, ln in enumerate(lines):
        if pat.search(ln) and not ln.strip().startswith("//"):
            # 找起点
            j = i
            depth = 0
            started = False
            out = []
            while j < len(lines) and j < i + 400:
                out.append("%6d| %s" % (j + 1, lines[j]))
                depth += lines[j].count("{") - lines[j].count("}")
                if "{" in lines[j]:
                    started = True
                if started and depth <= 0:
                    break
                if not started and ";" in lines[j]:
                    break
                j += 1
            print("\n".join(out))
            print("-" * 60)


def grep(path, pattern):
    rx = re.compile(pattern)
    for i, ln in enumerate(io.open(path, encoding="utf-8", errors="replace"), 1):
        if rx.search(ln):
            print("%6d| %s" % (i, ln.rstrip()))


if __name__ == "__main__":
    a = sys.argv[1:]
    if not a:
        print(__doc__)
        sys.exit(1)
    if a[0] == "--body":
        body(a[1], a[2])
    elif a[0] == "--grep":
        grep(a[1], a[2])
    else:
        for p in a:
            print(scan(p))
            print()
