import sys
with open(sys.argv[1], 'rb') as f:
    data = f.read()
lines = data.split(b'\n')
print(f"总行数: {len(lines)}")
print(f"第1行长度: {len(lines[0])}")
print(f"第1行前50字符: {lines[0][:50]}")
print(f"第2行长度: {len(lines[1])}")
print(f"第2行前50字符: {lines[1][:50]}")
print(f"第3行长度: {len(lines[2])}")
print(f"第3行前50字符: {lines[2][:50]}")
# 检查第1行是否是纯数字（用例数量）
if lines[0].isdigit():
    print(f"第1行是用例数量: {lines[0].decode()}")
else:
    # 检查第1行是否有空格分隔
    parts = lines[0].split(b' ')
    print(f"第1行有 {len(parts)} 个空格分隔部分")
    for i, p in enumerate(parts[:3]):
        print(f"  部分{i}: 长度={len(p)} 前30={p[:30]}")
