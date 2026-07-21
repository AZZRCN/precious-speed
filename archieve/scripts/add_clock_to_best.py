#!/usr/bin/env python3
# add_clock_to_best.py
# 给 best 源码加 clock() 计时，输出到 stderr
import re

for src, name in [('best_add.cpp', 'ADD'), ('best_mul.cpp', 'MUL'), ('best_div.cpp', 'DIV')]:
    path = f'/tmp/bench2/{src}'
    with open(path, 'r') as f:
        code = f.read()

    # 在 main 函数开头加 clock 计时
    # best 的 main 可能是 int main() { 或 int main() {
    # 在 return 0; 前加计时输出

    # 找到 int main() {
    code = code.replace(
        'int main() {',
        'int main() { clock_t _bt0 = clock();',
        1  # 只替换第一个
    )

    # 在 return 0; 前加计时输出
    # 可能是 "return 0;" 或 "return 0; }"
    code = code.replace(
        '    return 0;\n}',
        '    fprintf(stderr, "BEST_CPU: %.3f ms\\n", double(clock() - _bt0) * 1000.0 / CLOCKS_PER_SEC);\n    return 0;\n}',
        1  # 只替换最后一个
    )

    # 确保 ctime 已包含
    if '#include <ctime>' not in code and '#include <time.h>' not in code:
        code = code.replace('#include <cstdio>', '#include <cstdio>\n#include <ctime>')

    with open(path, 'w') as f:
        f.write(code)

    print(f"{name}: modified")

print("Done")
