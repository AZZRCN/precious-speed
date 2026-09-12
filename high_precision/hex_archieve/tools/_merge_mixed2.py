#!/usr/bin/env python3
# 健壮版: 把 391969 的混合 radix 移植进 DIV (div_v11_hugify.cpp) -> div_v12_mixed.cpp
# 仅移植: pointwiseSq + Mixed 命名空间正文 + fft_ceil_tiers/fft_len_for + mul_fft(派发版).
# 保留 DIV 自身的 fm_prep/fm_mul (原 merge 脚本误删, 这里不再动它们).
import os

DIV = r"D:/hex_precious_speed/work/div/div_v11_hugify.cpp"
SRC = r"D:/hex_precious_speed/web_best/mul_tmp.cpp"
OUT = r"D:/hex_precious_speed/work/div/div_v12_mixed.cpp"

div = open(DIV, encoding="utf-8").read().split("\n")
src = open(SRC, encoding="utf-8").read().split("\n")

def block(lines, a, b):
    return "\n".join(lines[a-1:b])

pointwiseSq = block(src, 538, 556)
mixed       = block(src, 557, 793)
fterms      = block(src, 850, 869)
mul_fft     = block(src, 892, 965)

# 1) fft 命名空间闭合前插入 pointwiseSq + Mixed
out = []
inserted_fft = False
for ln in div:
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

# 3) 替换 mul_fft (DIV 的 -> MUL 派发版)
divlines = text.split("\n")
newlines = []
i = 0
replaced_mf = False
while i < len(divlines):
    ln = divlines[i]
    if not replaced_mf and ln.startswith("static void mul_fft("):
        depth = 0; j = i; started = False
        while j < len(divlines):
            cur = divlines[j]
            depth += cur.count("{") - cur.count("}")
            if "{" in cur: started = True
            if started and depth <= 0:
                break
            j += 1
        for ml in mul_fft.split("\n"):
            newlines.append(ml)
        i = j + 1
        replaced_mf = True
    else:
        newlines.append(ln)
        i += 1
assert replaced_mf, "未替换 mul_fft"
text = "\n".join(newlines)

# 4) #include <vector>
assert "#include <complex>" in text
text = text.replace("#include <complex>\n", "#include <complex>\n#include <vector>\n", 1)

# 5) split_b2 -> 5 参, 返回 size_t, 带 zlim memset
text = text.replace(
    "static void split_b2(const u64* src, double* g, size_t n, int k) {",
    "static size_t split_b2(const u64* src, double* g, size_t n, int k, size_t zlim) {",
    1)
old_tail = "        g[j] = (double)(u32)(v & m);\n    }\n}"
new_tail = "        g[j] = (double)(u32)(v & m);\n    }\n    if (zlim > total) std::memset(g + total, 0, (zlim - total) * 8);\n    return total;\n}"
assert old_tail in text, "split_b2 尾未找到"
text = text.replace(old_tail, new_tail, 1)

open(OUT, "w", encoding="utf-8").write(text)
print("WROTE", OUT, len(text), "bytes")
