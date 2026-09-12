#!/usr/bin/env python3
"""把 lc_bench/cases/<prob>/*.in 规范化为 LC 真实格式（LF 行尾 + 文件名去空格），
输出到 lc_bench/cases_lf/<prob>/。原目录保持不动，便于对照。

Library Checker 判题数据是 Linux 生成的 LF 文本；本地这批 .in 在某次经 Windows
中转时被转成了 CRLF，导致所有本地基准/剖析跑的是「幽灵空 token」错位路径，
与 LC 实际执行路径不同。
"""
import os
import sys
import glob

ROOT = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(ROOT, 'cases')
DST = os.path.join(ROOT, 'cases_lf')

probs = sys.argv[1:] or ['add', 'mul', 'div']

for prob in probs:
    src_dir = os.path.join(SRC, prob)
    dst_dir = os.path.join(DST, prob)
    if not os.path.isdir(src_dir):
        print(f'skip {prob}: no {src_dir}')
        continue
    os.makedirs(dst_dir, exist_ok=True)
    n_conv = n_same = 0
    for path in sorted(glob.glob(os.path.join(src_dir, '*.in'))):
        name = os.path.basename(path).replace(' ', '')
        data = open(path, 'rb').read()
        fixed = data.replace(b'\r\n', b'\n')
        # 确保结尾有换行（LC 数据保证）
        if fixed and not fixed.endswith(b'\n'):
            fixed += b'\n'
        if fixed != data:
            n_conv += 1
        else:
            n_same += 1
        with open(os.path.join(dst_dir, name), 'wb') as f:
            f.write(fixed)
    print(f'{prob}: converted={n_conv} unchanged={n_same} -> {dst_dir}')
