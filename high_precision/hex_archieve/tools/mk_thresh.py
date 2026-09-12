#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""v10c -> v10c_th : 把 comba/FFT 切换阈值改成编译期可调 BF_LIMIT。
用法: python mk_thresh.py <src> <dst>
"""
import io, sys

src, dst = sys.argv[1], sys.argv[2]
s = io.open(src, encoding='utf-8').read()

old = "        if (mb <= 48) mul_bf(pa, ma, pb, mb, Rr);"
new = "        if (mb <= BF_LIMIT) mul_bf(pa, ma, pb, mb, Rr);"
assert old in s, 'dispatch not found'
s = s.replace(old, new)

anchor = "#ifndef FFT_LEAF_LOG"
assert anchor in s
s = s.replace(anchor,
              "#ifndef BF_LIMIT\n#define BF_LIMIT 48\n#endif\n" + anchor, 1)

io.open(dst, 'w', encoding='utf-8', newline='\n').write(s)
print('OK', dst, len(s))
