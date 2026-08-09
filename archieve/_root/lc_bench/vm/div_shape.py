#!/usr/bin/env python3
"""DIV 形状采集: 导出每个官方用例中所有顶层除法的 (len1, len2)。

输出 shapes.json: {case: [[len1,len2], ...]}
之后可在本地离线评估任意分发策略的建模代价, 无需反复上机。
"""
import json
import re
import subprocess
import sys
from pathlib import Path

SRC_IN = Path('/home/azzr/divbench/src/div_D4.cpp')
SRC_OUT = Path('/home/azzr/divbench/src/div_shape.cpp')
BIN = Path('/home/azzr/divbench/bin/div_shape')
INDIR = Path('/home/azzr/lcp/big_integer/division_of_big_integers/in')
OUT = Path('/home/azzr/divbench/shapes.json')
FLAGS = '-O2 -std=c++23 -march=x86-64-v3 -DDIVSHAPE'

HEADER = r'''
// ==================== DIVSHAPE (形状采集) ====================
#ifdef DIVSHAPE
#include <cstdio>
#include <vector>
struct ShRec { size_t l1, l2; };
struct ShStat {
    std::vector<ShRec> v;
    ~ShStat() {
        for (auto &r : v) std::fprintf(stderr, "@@SH %zu %zu\n", r.l1, r.l2);
    }
};
static ShStat g_sh;
#define SH(a, b) g_sh.v.push_back({(a), (b)})
#else
#define SH(a, b) ((void)0)
#endif
// =============================================================
'''

PATCHES = [
    ('#ifdef PROFILE_DIV\nstatic FILE *_prof_fp = nullptr;',
     HEADER + '\n#ifdef PROFILE_DIV\nstatic FILE *_prof_fp = nullptr;'),

    ('''                Span quot_span(quotient.data.data(), len1 - len2);
                if (len2 <= 64 || (len1 - len2) <= 64)''',
     '''                Span quot_span(quotient.data.data(), len1 - len2);
                SH(len1, len2);
                if (len2 <= 64 || (len1 - len2) <= 64)'''),
]


def main():
    src = SRC_IN.read_text(encoding='utf-8', errors='replace')
    for i, (a, b) in enumerate(PATCHES):
        if src.count(a) != 1:
            print(f'!! patch {i}: anchor count = {src.count(a)}')
            return 1
        src = src.replace(a, b, 1)
    SRC_OUT.write_text(src, encoding='utf-8')
    r = subprocess.run(f'g++ {FLAGS} -o {BIN} {SRC_OUT} 2>&1', shell=True,
                       capture_output=True, text=True)
    if r.returncode != 0:
        print('BUILD FAIL:\n' + r.stdout[-4000:])
        return 1
    print('build OK', flush=True)

    res = {}
    for p in sorted(INDIR.glob('*.in')):
        with open(p, 'rb') as f:
            r = subprocess.run([str(BIN)], stdin=f, stdout=subprocess.DEVNULL,
                               stderr=subprocess.PIPE, timeout=300)
        sh = [[int(a), int(b)] for a, b in
              re.findall(r'@@SH (\d+) (\d+)', r.stderr.decode('utf-8', 'replace'))]
        res[p.stem] = sh
        tot = sum(a for a, _ in sh)
        print(f'{p.stem:28s} calls={len(sh):>5d} sum_len1={tot}', flush=True)
    OUT.write_text(json.dumps(res))
    print('wrote', OUT)
    return 0


if __name__ == '__main__':
    sys.exit(main())
