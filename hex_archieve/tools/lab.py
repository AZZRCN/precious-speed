#!/usr/bin/env python3
"""本地驱动: 编译候选 -> 官方测试点验证 -> perf 指令数/周期对比.

用法:
  python lab.py <prob> <local.cpp> [more.cpp ...] [--flags "..."] [--reps N]
                [--verify-only] [--no-verify] [--only PATTERN]

默认对手 = web_best/<prob>.cpp (自动加入对比).
编译参数默认与 LC 一致: -O2 -std=c++23 -DEVAL -DONLINE_JUDGE -march=native
"""
import os, sys, posixpath
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl

WS = '/home/azzr/hexbench'
ROOT = 'D:/hex_precious_speed'
LCFLAGS = '-O2 -std=c++23 -DEVAL -DONLINE_JUDGE -march=native'

def main():
    args = sys.argv[1:]
    flags = LCFLAGS
    reps = 3
    extra = []
    rest = []
    i = 0
    while i < len(args):
        a = args[i]
        if a == '--flags':
            flags = args[i + 1]; i += 2; continue
        if a == '--reps':
            reps = int(args[i + 1]); i += 2; continue
        if a.startswith('--'):
            extra.append(a); i += 1; continue
        rest.append(a); i += 1
    prob = rest[0]
    cands = rest[1:]

    vmctl.put(f'{ROOT}/tools/oracle.py', f'{WS}/oracle.py')
    vmctl.put(f'{ROOT}/tools/measure.py', f'{WS}/measure.py')

    bins = []
    for c in cands:
        local = c if os.path.isabs(c) or c.startswith(('D:', 'd:')) else f'{ROOT}/{c}'
        name = os.path.splitext(os.path.basename(local))[0]
        vmctl.put(local, f'{WS}/{prob}/{name}.cpp')
        bins.append(name)
    # baseline: web_best
    wb = f'{ROOT}/web_best/{prob}.cpp'
    if os.path.exists(wb):
        vmctl.put(wb, f'{WS}/{prob}/web.cpp')
        bins.append('web')

    print('=== compile ===', flush=True)
    cmds = []
    for b in bins:
        cmds.append(f'g++ {flags} -o {WS}/{prob}/{b} {WS}/{prob}/{b}.cpp 2>&1 | tail -6')
    vmctl.run(' ; '.join(cmds) + f' ; ls -la {WS}/{prob}/ | grep -vE "\\.cpp"', timeout=900)

    print('=== oracle (.exp) ===', flush=True)
    vmctl.run(f'python3 {WS}/oracle.py {prob} {WS}/data/{prob}', timeout=3600)

    print('=== verify + perf ===', flush=True)
    binlist = ' '.join(f'{WS}/{prob}/{b}' for b in bins)
    vmctl.run(f'python3 {WS}/measure.py {prob} {WS}/data/{prob} {binlist} '
              f'--reps={reps} {" ".join(extra)}', timeout=7200)

if __name__ == '__main__':
    main()
