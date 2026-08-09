# -*- coding: utf-8 -*-
"""D33 = D31 + FFT buffer de-zeroing (resize -> reserve) + NDEBUG (源码内消除 assert).

测量依据 (d31 O2 callgrind, lri_02): __memset_avx2_unaligned_erms 占 3.72% (1104万 Ir).
根因: std::vector::resize 对新增容量做值初始化 (memset 零), 而这些 FFT double 缓冲
在 resize 之后立刻被 copyU16ToF64AndFill / prepareDFT 全量覆写 [0, float_len), 零初始化纯浪费.
AlignedVec32 = std::vector<double, AlignedAlloc32>, reserve 不做值初始化; FFT 代码用
float_len (非 .size()) 索引, 故 resize->reserve 完全安全.

已逐一核对覆写安全性:
  tv1/tv2 (fftMul), tv (fftSqr/fftMulPre/fftMulModBm1/fftMulModBm1Pre), b_dft (fftMulUnbalanced)
    -> copyU16ToF64AndFill(in, buf, n, float_len) 写满 [0, float_len)
  inv_dft_buf/divisor_dft_buf/divisor_dft_mod_buf/inv_dft_buf_c1/div_dft_buf_c1
    -> prepareDFT(...) -> copyU16ToF64AndFill 写满
  large_copy (copy_n), tbuf (fftMulPre 写 [0,this_conv]), tprod (absMul1 写 [0,len2])
均验证全覆写, 去零化无正确性风险.
"""
import io

SRC = r'D:\precious_speed\best\div_D31.cpp'
OUT = r'D:\precious_speed\best\div_D33.cpp'

t = io.open(SRC, encoding='utf-8').read()

# 1) 插入 NDEBUG (在 #define HINT_OP_DIV 之后, 早于 #include <cassert> 第31行)
assert '#define HINT_OP_DIV' in t
t = t.replace('#define HINT_OP_DIV\n',
              '#define HINT_OP_DIV\n'
              '#define NDEBUG  // D33: 干掉源码中 81 个活 assert (LC 编 -O2 不带 NDEBUG)\n',
              1)

# 2) 版本注释
t = t.replace(
    '// D31 = D27 + Knuth D3 qhat refinement + Granlund-Montgomery reciprocal',
    '// D33 = D31 + FFT buffer de-zeroing (resize->reserve) + NDEBUG (源码内 assert 消除)',
    1)

# 3) resize -> reserve (grow-if-needed 习惯用法, 缓冲均被全量覆写, 故去零化安全)
repl = [
    ('tv1.resize(float_len)',          'if (tv1.capacity() < float_len) tv1.reserve(float_len)'),
    ('tv2.resize(float_len)',          'if (tv2.capacity() < float_len) tv2.reserve(float_len)'),
    ('tv.resize(float_len)',           'if (tv.capacity() < float_len) tv.reserve(float_len)'),  # 3433 & 3621
    ('b_dft.resize(float_len)',        'if (b_dft.capacity() < float_len) b_dft.reserve(float_len)'),
    ('large_copy.resize(len_big)',     'if (large_copy.capacity() < len_big) large_copy.reserve(len_big)'),
    ('tbuf.resize(tbuf_max)',          'if (tbuf.capacity() < tbuf_max) tbuf.reserve(tbuf_max)'),
    ('tv.resize(2 * m)',               'if (tv.capacity() < 2 * m) tv.reserve(2 * m)'),
    ('tv.resize(m)',                   'if (tv.capacity() < m) tv.reserve(m)'),
    ('tprod.resize(len2 + 1)',         'if (tprod.capacity() < len2 + 1) tprod.reserve(len2 + 1)'),
    ('inv_dft_buf_c1.resize(inv_fl)',  'if (inv_dft_buf_c1.capacity() < inv_fl) inv_dft_buf_c1.reserve(inv_fl)'),
    ('div_dft_buf_c1.resize(div_fl)',  'if (div_dft_buf_c1.capacity() < div_fl) div_dft_buf_c1.reserve(div_fl)'),
    ('inv_dft_buf.resize(inv_float_len)',    'if (inv_dft_buf.capacity() < inv_float_len) inv_dft_buf.reserve(inv_float_len)'),  # 4982 & 5615
    ('divisor_dft_buf.resize(divisor_float_len)', 'if (divisor_dft_buf.capacity() < divisor_float_len) divisor_dft_buf.reserve(divisor_float_len)'),  # 4983 & 5593
    ('divisor_dft_mod_buf.resize(cyclic_m)',     'if (divisor_dft_mod_buf.capacity() < cyclic_m) divisor_dft_mod_buf.reserve(cyclic_m)'),
]
for old, new in repl:
    cnt = t.count(old)
    assert cnt >= 1, 'not found: ' + old
    t = t.replace(old, new)
    print('replaced %s x%d' % (old, cnt))

# 3b) 收尾: 把 "if (X.size()<N) if (X.capacity()<N) X.reserve(N);" 塌缩为单层守卫
import re
pat = re.compile(
    r'if \(([A-Za-z_]\w*)\.size\(\) < ([^)]+)\) '
    r'if \(\1\.capacity\(\) < \2\) \1\.reserve\(\2\);')
before = t.count('if (')  # noqa
t, n_collapse = pat.subn(r'if (\1.capacity() < \2) \1.reserve(\2);', t)
print('collapsed double-guards x%d' % n_collapse)
# use_cyclic 特例
t = t.replace(
    'if (use_cyclic && divisor_dft_mod_buf.size() < cyclic_m) '
    'if (divisor_dft_mod_buf.capacity() < cyclic_m) divisor_dft_mod_buf.reserve(cyclic_m);',
    'if (use_cyclic && divisor_dft_mod_buf.capacity() < cyclic_m) divisor_dft_mod_buf.reserve(cyclic_m);')
# 断言塌缩后不应再残留 "size() < ...) if (" 双层守卫
leftover = re.compile(r'\.size\(\) < [^)]+\) if \(')
assert not leftover.search(t), 'leftover double-guard found'

io.open(OUT, 'w', encoding='utf-8').write(t)
print('WROTE', OUT, len(t), 'bytes')
