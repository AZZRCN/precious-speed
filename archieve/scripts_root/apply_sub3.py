#!/usr/bin/env python3
"""absSub_avx2 直接 store 优化 - 用行号定位"""
PATH = r'd:\precious_speed\add.cpp'

with open(PATH, 'r', encoding='utf-8', newline='') as f:
    lines = f.readlines()

# 找 absSub_avx2 中 alignas 行的行号 (0-indexed)
start_idx = None
end_idx = None
for i, line in enumerate(lines):
    if 'alignas(32) uint32_t tmp[8];' in line and start_idx is None:
        # 检查上下文是否在 absSub_avx2 中
        if i > 0 and '__m256i r = _mm256_sub_epi32' in lines[i-1]:
            start_idx = i - 1  # 从 __m256i r 行开始
    if start_idx is not None and end_idx is None:
        # 找到 borrow = static_cast<Limb>(bw); 后的 } 行
        if 'borrow = static_cast<Limb>(bw);' in line:
            # 下面 3 行应该是 }, 找到 _mm256_storeu 的行
            pass
        if '_mm256_storeu_si256' in line and 'out_vec' in line and i > start_idx:
            # 下一行是 }
            end_idx = i + 1  # 包含 } 行
            break

if start_idx is None or end_idx is None:
    print(f'NOT FOUND: start={start_idx}, end={end_idx}')
    exit(1)

print(f'Found block: lines {start_idx+1} to {end_idx+1}')
print('Old block:')
for i in range(start_idx, end_idx + 1):
    print(f'  L{i+1}: {lines[i].rstrip()!r}')

# 新内容 (16 空格缩进, CRLF)
new_lines = [
    '                __m256i r = _mm256_sub_epi32(_mm256_add_epi32(a, bias_vec), b);\r\n',
    '                // 直接 store 到 out, 省去 tmp 缓冲区 (absSub 直接 store 优化)\r\n',
    '                _mm256_storeu_si256(reinterpret_cast<__m256i *>(out.ptr + i), r);\r\n',
    '                uint32_t *p32 = reinterpret_cast<uint32_t *>(out.ptr + i);\r\n',
    '                uint32_t bw = borrow;\r\n',
    '                for (int k = 0; k < 8; k++)\r\n',
    '                {\r\n',
    '                    uint32_t lo = p32[k] & 0xFFFF;\r\n',
    '                    uint32_t hi = p32[k] >> 16;\r\n',
    '                    lo -= bw;\r\n',
    '                    uint32_t blo = lo < 10000;\r\n',
    '                    lo -= (1u - blo) * 10000u;\r\n',
    '                    hi -= blo;\r\n',
    '                    uint32_t bhi = hi < 10000;\r\n',
    '                    hi -= (1u - bhi) * 10000u;\r\n',
    '                    bw = bhi;\r\n',
    '                    p32[k] = lo | (hi << 16);\r\n',
    '                }\r\n',
    '                borrow = static_cast<Limb>(bw);\r\n',
    '            }\r\n',
]

# 替换
lines[start_idx:end_idx + 1] = new_lines

with open(PATH, 'w', encoding='utf-8', newline='') as f:
    f.writelines(lines)

print(f'\nReplaced {end_idx - start_idx + 1} lines with {len(new_lines)} lines')
print('DONE')
