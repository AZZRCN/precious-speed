import difflib
import sys

with open(r'd:\precious_speed\mul.cpp', 'r', encoding='utf-8', errors='replace') as f:
    cur = f.readlines()
with open(r'd:\precious_speed\best\mul.cpp', 'r', encoding='utf-8', errors='replace') as f:
    best = f.readlines()

print(f'CUR lines: {len(cur)}, BEST lines: {len(best)}, diff: {len(cur)-len(best)}')

# 找所有差异块
sm = difflib.SequenceMatcher(None, cur, best)
opcodes = sm.get_opcodes()
print(f'Total diff blocks: {len([o for o in opcodes if o[0]!="equal"])}')
print()
for tag, i1, i2, j1, j2 in opcodes:
    if tag == 'equal':
        continue
    print(f'=== {tag} CUR[{i1+1}:{i2}] ({i2-i1} lines) <-> BEST[{j1+1}:{j2}] ({j2-j1} lines) ===')
    # 上下文：前一非空行
    ctx_pre = ''
    k = i1 - 1
    while k >= 0 and k >= i1 - 3:
        if cur[k].strip():
            ctx_pre = f'  ctx: {cur[k].rstrip()[:100]}'
            break
        k -= 1
    if ctx_pre:
        print(ctx_pre)
    # CUR块
    for i in range(i1, min(i2, i1 + 30)):
        print(f'  CUR {i+1:>5}: {cur[i].rstrip()[:160]}')
    if i2 - i1 > 30:
        print(f'  CUR ... ({i2-i1-30} more)')
    # BEST块
    for j in range(j1, min(j2, j1 + 30)):
        print(f'  BEST{j+1:>5}: {best[j].rstrip()[:160]}')
    if j2 - j1 > 30:
        print(f'  BEST... ({j2-j1-30} more)')
    print()
