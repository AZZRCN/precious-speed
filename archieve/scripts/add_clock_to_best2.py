#!/usr/bin/env python3
# add_clock_to_best2.py
import re

for src, name in [('best_add.cpp', 'ADD'), ('best_mul.cpp', 'MUL'), ('best_div.cpp', 'DIV')]:
    path = f'/tmp/bench2/{src}'
    with open(path, 'r') as f:
        code = f.read()

    # 确保 ctime 已包含
    if '#include <ctime>' not in code and '#include <time.h>' not in code:
        code = code.replace('#include <cstdio>', '#include <cstdio>\n#include <ctime>', 1)

    # 在第一个 int main() { 后加计时开始
    code = re.sub(
        r'int main\(\) \{',
        'int main() { clock_t _bt0 = clock();',
        code,
        count=1
    )

    # 在最后一个 return 0; 前加计时输出
    # 匹配任意缩进的 return 0; 后跟 }
    code = re.sub(
        r'(\s+)return 0;\s*\n\}',
        r'\1fprintf(stderr, "BEST_CPU: %.3f ms\\n", double(clock() - _bt0) * 1000.0 / CLOCKS_PER_SEC);\n\1return 0;\n}',
        code,
        count=1
    )

    with open(path, 'w') as f:
        f.write(code)

    # 验证
    with open(path, 'r') as f:
        c = f.read()
    has_clock = '_bt0' in c
    has_output = 'BEST_CPU' in c
    print(f"{name}: clock_start={has_clock} output={has_output}")

print("Done")
