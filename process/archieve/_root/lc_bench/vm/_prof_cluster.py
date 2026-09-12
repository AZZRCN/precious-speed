#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""对一组用例批量跑 callgrind 函数级 Ir 排行 (复用 _top_remote.py 的解析)。"""
import subprocess, sys

tag = sys.argv[1]
cases = sys.argv[2:]
if not cases:
    cases = ['length_ratio_integer_02', 'length_ratio_integer_00',
             'burnikel_ziegler_02', 'r_nearly_zero_01', 'medium_01',
             'a_max_b_random_02', 'length_ratio_integer_01']

for C in cases:
    print('===== %s =====' % C)
    subprocess.run(['python3', '_top_remote.py', tag, C, '16'],
                   cwd='/home/azzr/divbench')
    print()
