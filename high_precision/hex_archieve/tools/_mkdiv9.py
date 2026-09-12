#!/usr/bin/env python3
"""把 MUL v19 的 FFT 命名空间(radix-4 + 两级 twiddle + 向量化 pointwise)整体移植进 DIV,
丢弃未调用的混合基/平方代码。保持 DIV 的 split_b2/merge_b2/pick_k/mul_bf/mul_fft/mulg/
FixedFFT/fm_prep/fm_mul/knuthD/BZ/invertappr/main 不变。
"""
import os, sys
ROOT = 'D:/hex_precious_speed'
div = open(os.path.join(ROOT, 'submit_ready/div.cpp')).read().splitlines()
v19 = open(os.path.join(ROOT, 'work/mul/v19.cpp')).read().splitlines()

# v19 fft 命名空间: 1-indexed 119..505 (pointwise 闭合); 排除 506+ 的混合基/平方
# 0-indexed: 118..504
a, b = 118, 505
block = v19[a:b]
print('[block head]', repr(block[0]))
print('[block tail]', repr(block[-1]))
assert block[0].strip().startswith('namespace fft'), block[0]
assert block[-1].strip() == '}', block[-1]

# 在 div 中找到 fft 命名空间范围
start = end = None
for i, l in enumerate(div):
    if l.strip() == 'namespace fft {':
        start = i
    elif l.strip().startswith('}  // namespace fft'):
        end = i
        break
assert start is not None and end is not None, (start, end)
print(f'[div fft ns] lines {start+1}..{end+1}')

newdiv = div[:start] + block + ['}  // namespace fft'] + div[end + 1:]
out = os.path.join(ROOT, 'work/div/v9.cpp')
open(out, 'w').write('\n'.join(newdiv) + '\n')
print('[written]', out, 'lines=', len(newdiv))

# 简单一致性检查: 新文件仍含关键入口
txt = '\n'.join(newdiv)
for key in ['void mul_fft', 'void fm_prep', 'void fm_mul', 'void difRec', 'void ditRec',
            'void pointwise', 'twbase[1u << 12]', 'FFT_LEAF_LOG']:
    assert key in txt, f'missing {key}'
print('[checks] OK')
