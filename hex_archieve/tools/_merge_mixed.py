#!/usr/bin/env python3
# 把 391969 的混合 radix 全套移植进 DIV 的 div_v11_hugify.cpp -> div_v12_mixed.cpp
import re

DIV = r"D:/hex_precious_speed/work/div/div_v11_hugify.cpp"
SRC = r"D:/hex_precious_speed/web_best/mul_tmp.cpp"
OUT = r"D:/hex_precious_speed/work/div/div_v12_mixed.cpp"

div = open(DIV, encoding="utf-8").read().split("\n")
src = open(SRC, encoding="utf-8").read().split("\n")

def block(lines, a, b):
    # 含端点 a,b (1-based) -> 0-based [a-1:b]
    return "\n".join(lines[a-1:b])

# --- 从 391969 抽取 ---
pointwiseSq = block(src, 538, 556)                 # 含端点
mixed       = block(src, 557, 793)                 # Mixed 命名空间正文(到 794 闭合前)
fterms      = block(src, 850, 869)                 # fft_ceil_tiers + fft_len_for
mul_fft     = block(src, 892, 965)                 # 派发版 mul_fft
fm_prep     = block(src, 657, 671)                 # fm_prep
fm_mul      = block(src, 672, 680)                 # fm_mul

# --- 在 DIV 中插入 ---
# 1) fft 命名空间闭合前插入 pointwiseSq + Mixed
out = []
inserted_fft = False
for i, ln in enumerate(div):
    if not inserted_fft and ln.strip() == "}  // namespace fft":
        out.append(pointwiseSq)
        out.append("")
        out.append(mixed)
        out.append("")
        inserted_fft = True
    out.append(ln)
assert inserted_fft, "未找到 fft 命名空间闭合"

# 2) pick_k 前插入 fft_ceil_tiers + fft_len_for
text = "\n".join(out)
pick = "static inline int pick_k(size_t u) {"
assert pick in text, "未找到 pick_k"
text = text.replace(pick, fterms + "\n\n" + pick, 1)

# 3) 替换 mul_fft
divlines = text.split("\n")
newlines = []
i = 0
replaced_mf = False
while i < len(divlines):
    ln = divlines[i]
    if not replaced_mf and ln.startswith("static void mul_fft("):
        # 找到该函数起始, 向后到孤立 '}' 闭合
        depth = 0
        j = i
        started = False
        while j < len(divlines):
            cur = divlines[j]
            depth += cur.count("{") - cur.count("}")
            if "{" in cur: started = True
            if started and depth <= 0:
                break
            j += 1
        # 替换 [i..j]
        for ml in mul_fft.split("\n"):
            newlines.append(ml)
        i = j + 1
        replaced_mf = True
    else:
        newlines.append(ln)
        i += 1
assert replaced_mf, "未替换 mul_fft"

# 4) 替换 fm_prep + fm_mul (连续)
text = "\n".join(newlines)
# fm_prep 起始
fp_start = text.index("static void fm_prep(")
# 找 fm_prep 闭合
def find_close(s, start):
    depth = 0; started = False; j = s.index("{", start)
    while j < len(s):
        depth += s[j:].count("{")  # 粗糙, 用逐字符更稳
        break
# 改为逐字符
def slice_fn(s, start):
    # start 指向 "static void fm_prep(" 行首
    k = s.index("{", start)
    depth = 0
    idx = k
    while idx < len(s):
        c = s[idx]
        if c == "{": depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return s[start:idx+1], idx+1
        idx += 1
    return s[start:], len(s)

fp_body, fp_end = slice_fn(text, fp_start)
# fm_mul 紧跟其后
fm_start = text.index("static void fm_mul(", fp_end)
fm_body, fm_end = slice_fn(text, fm_start)
repl = fm_prep + "\n\n" + fm_mul
text = text[:fp_start] + repl + text[fm_end:]

open(OUT, "w", encoding="utf-8").write(text)
print("WROTE", OUT, len(text), "bytes")
print("pointwiseSq len", len(pointwiseSq), "| mixed len", len(mixed), "| fterms len", len(fterms))
print("mul_fft len", len(mul_fft), "| fm_prep len", len(fm_prep), "| fm_mul len", len(fm_mul))
