# -*- coding: utf-8 -*-
"""D36 = D34 + 把 absMul/absSqr 的 basicMul 分发阈值从 FFT_MUL_THRESHOLD 解耦出来。

动机 (CEILHIST, bz_02):
  最大的 FFT 档位浪费集中在极小尺寸 ——
    lin need=129 -> used=192  x835  waste 48.8%
    lin need=135 -> used=192  x874  waste 42.2%
    lin need=134 -> used=192  x817  waste 43.3%
    mn  need=67  -> used=128  x810  waste 91.0%
  need=129 恰好是 65x65 limb 的卷积长度, 即"刚过 FFT_MUL_THRESHOLD=64"。
  这些点被推进 192 点 FFT, 白付近 50% 的 N*logN。radix-5 档位帮不上
  (radix-k 要求每块 M>=32, 即 float_len>=320), 所以正确的杠杆是把
  basicMul 的适用区间往上抬。

但 FFT_MUL_THRESHOLD 同时被 absDivRem(4871) 用作 fast/slow Newton 路径闸门,
两个语义必须解耦, 否则扫描会一动全动、无法归因。

本补丁默认行为与 D34 逐字节等价 (MUL_BASIC_THRESHOLD 默认取 FFT_MUL_THRESHOLD)。
"""
import io
import os
import re

SRC = r'D:\precious_speed\best\div_D34.cpp'
DST = r'D:\precious_speed\best\div_D36.cpp'

s = io.open(SRC, 'r', encoding='utf-8', errors='surrogateescape').read()
orig = s

# --- 1) 在 FFT_MUL_UNBALANCED_RATIO 定义之后插入两个解耦宏 ---
anchor = '#ifndef FFT_MUL_UNBALANCED_RATIO\n#define FFT_MUL_UNBALANCED_RATIO 6\n#endif\n'
assert s.count(anchor) == 1, 'anchor FFT_MUL_UNBALANCED_RATIO not unique'
inject = anchor + (
    '// === D36: basicMul 分发阈值 (与 absDivRem 的 Newton 路径闸门解耦) ===\n'
    '// CEILHIST 显示 bz_02 最大的 FFT 档位浪费全在 need=129..192 (65x65 limb),\n'
    '// 即"刚过阈值就被推进 192 点 FFT", 白付 ~49% N*logN。radix-5 档位够不着\n'
    '// (要求 float_len>=320), 故改由抬高 basicMul 适用区间来吃掉这块浪费。\n'
    '// 默认值与 D34 完全一致, 只有显式 -DMUL_BASIC_THRESHOLD 才改变行为。\n'
    '#ifndef MUL_BASIC_THRESHOLD\n'
    '#define MUL_BASIC_THRESHOLD FFT_MUL_THRESHOLD\n'
    '#endif\n'
    '#ifndef SQR_BASIC_THRESHOLD\n'
    '#define SQR_BASIC_THRESHOLD FFT_SQR_THRESHOLD\n'
    '#endif\n'
)
s = s.replace(anchor, inject, 1)

# --- 2) absSqr 的分发点 ---
a = 'if (in.size <= FFT_SQR_THRESHOLD)'
assert s.count(a) == 1, 'absSqr dispatch site not unique: %d' % s.count(a)
s = s.replace(a, 'if (in.size <= SQR_BASIC_THRESHOLD)', 1)

# --- 3) absMul 的分发点 ---
b = 'if (sml <= FFT_MUL_THRESHOLD)'
assert s.count(b) == 1, 'absMul dispatch site not unique: %d' % s.count(b)
s = s.replace(b, 'if (sml <= MUL_BASIC_THRESHOLD)', 1)

# --- 安全闸门: absDivRem(4871) 的 Newton 路径闸门必须原样保留 ---
gate = 'if (divisor_high.size >= FFT_MUL_THRESHOLD)'
assert s.count(gate) == 1, 'absDivRem Newton gate must stay on FFT_MUL_THRESHOLD'

assert s != orig
io.open(DST, 'w', encoding='utf-8', errors='surrogateescape').write(s)
print('wrote %s  (%d -> %d bytes)' % (DST, len(orig), len(s)))
