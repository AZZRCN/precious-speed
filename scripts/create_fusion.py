#!/usr/bin/env python3
"""创建 fusion.cpp = archieve/cpp/moptm.cpp + pragma O3"""
import os

src_path = r"d:\precious_speed\archieve\cpp\moptm.cpp"
dst_path = r"d:\precious_speed\fusion.cpp"

with open(src_path, "r", encoding="utf-8") as f:
    src = f.read()

# 检查是否已有 pragma
has_pragma = "pragma GCC optimize" in src and not src.strip().startswith("//")
print(f"Source has pragma: {has_pragma}")

# 去掉已有的注释 pragma（如果有）
lines = src.splitlines()
filtered = []
for line in lines:
    # 跳过注释掉的 pragma
    if line.strip().startswith("//") and "pragma GCC optimize" in line:
        continue
    filtered.append(line)
src = "\n".join(filtered) + "\n"

# 去掉硬编码的 #define HINT_OP_DIV（让编译开关完全由 -D 控制）
src = src.replace("#define HINT_OP_DIV\n", "")

# 在文件最顶部插入 pragma
pragma = '#pragma GCC optimize("O3,unroll-loops")\n'
fusion = pragma + src

with open(dst_path, "w", encoding="utf-8") as f:
    f.write(fusion)

print(f"Created {dst_path}: {len(fusion)} bytes ({len(fusion.splitlines())} lines)")

# 验证关键标记
for i, line in enumerate(fusion.splitlines(), 1):
    if "pragma GCC" in line or "#define HINT_OP" in line or "int main" in line or "mmap" in line:
        print(f"  L{i}: {line.strip()[:80]}")
