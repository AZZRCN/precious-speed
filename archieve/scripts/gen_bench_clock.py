#!/usr/bin/env python3
# gen_bench_clock.py
# 给 moptm.cpp 添加 clock() 计时，生成 moptm_bench.cpp
# 用 str.replace 代替 re.sub，避免反斜杠转义问题

with open('/tmp/bench2/moptm.cpp', 'r') as f:
    code = f.read()

# 计时输出语句（用 chr(92) 构造反斜杠）
bs = chr(92)  # 反斜杠字符
timer_stmt = 'fprintf(stderr, "CPU: %.3f ms' + bs + 'n", double(clock() - _bench_t0) * 1000.0 / CLOCKS_PER_SEC);'

# 1. 在 main 函数开头插入 clock() 开始计时
code = code.replace('int main() {', 'int main() { clock_t _bench_t0 = clock();')

# 2. 在 flushOutput(); 后的 return 0; 前插入计时输出 (MUL/DIV main)
code = code.replace(
    'flushOutput();\n    return 0;',
    'flushOutput();\n    ' + timer_stmt + '\n    return 0;'
)

# 3. 在 flushOutput(); 后的 #ifdef PROFILE_DIV 前插入计时输出 (ADD main)
code = code.replace(
    'flushOutput();\n#ifdef PROFILE_DIV',
    'flushOutput();\n    ' + timer_stmt + '\n#ifdef PROFILE_DIV'
)

with open('/tmp/bench2/moptm_bench.cpp', 'w') as f:
    f.write(code)

print('moptm_bench.cpp generated successfully')
