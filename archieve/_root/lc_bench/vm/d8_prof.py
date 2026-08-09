#!/usr/bin/env python3
"""剖析瓶颈用例 length_ratio_integer_02 的内部时间结构 (PROFILE_DIV)。

注意: 这是"同一二进制内部的结构归因", 不用于候选间比较 (符合测量纪律)。
"""
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

IN = Path('/home/azzr/lcp/big_integer/division_of_big_integers/in')
SRCDIR = Path('/home/azzr/divbench/src')
BINDIR = Path('/home/azzr/divbench/bin')
FLAGS = '-O2 -std=c++23 -march=x86-64-v3 -DPROFILE_DIV'

WHICH = sys.argv[1] if len(sys.argv) > 1 else 'div_D4'
CASE = sys.argv[2] if len(sys.argv) > 2 else 'length_ratio_integer_02'


def main():
    r = subprocess.run(f'g++ {FLAGS} -o {BINDIR}/{WHICH}_prof {SRCDIR}/{WHICH}.cpp 2>&1',
                       shell=True, capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stdout[-3000:])
        return 1
    print(f'build {WHICH}_prof OK', flush=True)
    with open(IN / f'{CASE}.in', 'rb') as f:
        p = subprocess.run([str(BINDIR / f'{WHICH}_prof')], stdin=f,
                           capture_output=True, timeout=900)
    err = p.stderr.decode('utf-8', 'replace')
    pf = Path('/home/azzr/divbench/prof_detail.log')
    if pf.exists():
        err += pf.read_text(errors='replace')
    lines = [l for l in err.splitlines() if '[prof]' in l]
    print(f'{WHICH} / {CASE}: prof 行数 = {len(lines)}')

    # 聚合: 按 "标签" 累加毫秒
    agg = defaultdict(lambda: [0.0, 0])
    for l in lines:
        m = re.search(r'\[prof\]\s+(.*?):\s+([\d.]+)\s*ms', l)
        if not m:
            continue
        label = m.group(1)
        # 归一化: 去掉 block 序号与具体尺寸参数, 保留结构名
        label = re.sub(r'block \d+', 'block N', label)
        label = re.sub(r'\(.*?\)', '', label).strip()
        agg[label][0] += float(m.group(2))
        agg[label][1] += 1
    tot = sum(v[0] for v in agg.values())
    print(f'{"标签":<52}{"总ms":>10}{"次数":>7}{"占比":>8}')
    print('-' * 78)
    for k, (ms, n) in sorted(agg.items(), key=lambda kv: -kv[1][0]):
        print(f'{k[:52]:<52}{ms:10.2f}{n:7d}{ms / tot * 100 if tot else 0:7.1f}%')
    print(f'{"合计(仅已计时片段)":<52}{tot:10.2f}')

    # 原始行样例 (前 25 行), 便于看到尺寸参数
    print('\n--- 原始样例 ---')
    for l in lines[:25]:
        print('  ' + l.strip()[:150])
    return 0


if __name__ == '__main__':
    sys.exit(main())
