#!/usr/bin/env python3
"""给 v10b 加 THPDBG 探针: 退出前打印 smaps_rollup 的 AnonHugePages"""
import io, os, sys
os.chdir(os.path.join(os.path.dirname(__file__), '..'))
src = sys.argv[1] if len(sys.argv) > 1 else 'work/mul/v10b.cpp'
dst = sys.argv[2] if len(sys.argv) > 2 else 'work/mul/_v10b_dbg.cpp'
s = io.open(src, encoding='utf-8').read()

probe = '''#include <sys/mman.h>
#ifdef THPDBG
#include <cstdio>
#include <cstring>
static void thpdump() {
    FILE* f = fopen("/proc/self/smaps_rollup", "r");
    if (!f) return;
    char l[256];
    while (fgets(l, sizeof l, f))
        if (strstr(l, "Rss:") || strstr(l, "AnonHugePages") || strstr(l, "Anonymous:"))
            fputs(l, stderr);
    fclose(f);
}
#endif'''
assert '#include <sys/mman.h>' in s
s = s.replace('#include <sys/mman.h>', probe, 1)

idx = s.rfind('return 0;')
assert idx > 0
s = s[:idx] + '#ifdef THPDBG\n    thpdump();\n#endif\n    ' + s[idx:]

io.open(dst, 'w', encoding='utf-8').write(s)
print('wrote', dst)
