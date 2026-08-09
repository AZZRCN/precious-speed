#!/usr/bin/env python3
"""给 add.cpp 打分支计数探针，定位为什么小数用例掉进大整数慢路径。"""
import sys

src_path, dst_path = sys.argv[1], sys.argv[2]
s = open(src_path, encoding='utf-8').read()
orig = s

# 1) 计数器声明
anchor = "int main() {"
assert s.count(anchor) == 1, f"main anchor count={s.count(anchor)}"
s = s.replace(anchor,
    "static long g_cntFast = 0, g_cntTry = 0, g_cntBig = 0;\n" + anchor, 1)

# 2) 快路径计数
fast_old = ("        if (la > 0 && la <= 18 && lb > 0 && lb <= 18 && "
            "sa[0] != '-' && sb[0] != '-') {\n")
assert s.count(fast_old) == 1, f"fast anchor count={s.count(fast_old)}"
s = s.replace(fast_old, fast_old + "            ++g_cntFast;\n", 1)

# 3) tryParse 成功 / 失败计数（失败时打印前 8 个样本）
try_old = ("            if (tryParseI64Unchecked(sa, la, va) && "
           "tryParseI64Unchecked(sb, lb, vb)) {\n"
           "                writeI64(va + vb);\n"
           "            } else {\n")
assert s.count(try_old) == 1, f"try anchor count={s.count(try_old)}"
try_new = ("            if (tryParseI64Unchecked(sa, la, va) && "
           "tryParseI64Unchecked(sb, lb, vb)) {\n"
           "                ++g_cntTry;\n"
           "                writeI64(va + vb);\n"
           "            } else {\n"
           "                ++g_cntBig;\n"
           "                if (g_cntBig <= 8) std::fprintf(stderr,\n"
           "                    \"BIG#%ld la=%zu lb=%zu a=[%.*s] b=[%.*s]\\n\",\n"
           "                    g_cntBig, la, lb, int(la), sa, int(lb), sb);\n")
s = s.replace(try_old, try_new, 1)

# 4) 结果打印：挂在最后一个 flushOutput(); 之后
idx = s.rfind("    flushOutput();")
assert idx > 0, "no trailing flushOutput"
end = idx + len("    flushOutput();")
s = (s[:end] +
     "\n    std::fprintf(stderr, \"[PROBE] fast=%ld try=%ld big=%ld total=%ld\\n\","
     " g_cntFast, g_cntTry, g_cntBig, g_cntFast + g_cntTry + g_cntBig);" +
     s[end:])

assert s != orig
open(dst_path, 'w', encoding='utf-8').write(s)
print(f"OK  {len(orig)} -> {len(s)} bytes")
