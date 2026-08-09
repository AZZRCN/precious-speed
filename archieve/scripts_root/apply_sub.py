#!/usr/bin/env python3
"""absSub_avx2 直接 store 优化"""
PATH = r'd:\precious_speed\add.cpp'

with open(PATH, 'r', encoding='utf-8', newline='') as f:
    content = f.read()

old = """                __m256i r = _mm256_sub_epi32(_mm256_add_epi32(a, bias_vec), b);
                alignas(32) uint32_t tmp[8];
                _mm256_store_si256(reinterpret_cast<__m256i *>(tmp), r);
                // 串行 borrow 传播: 8 步, 每步处理 2 个 limb (lo/hi), 无分支掩码
                uint32_t bw = borrow;
                for (int k = 0; k < 8; k++)
                {
                    uint32_t lo = tmp[k] & 0xFFFF;
                    uint32_t hi = tmp[k] >> 16;
                    lo -= bw;
                    uint32_t blo = lo < 10000;
                    lo -= (1u - blo) * 10000u;
                    hi -= blo;
                    uint32_t bhi = hi < 10000;
                    hi -= (1u - bhi) * 10000u;
                    bw = bhi;
                    tmp[k] = lo | (hi << 16);
                }
                borrow = static_cast<Limb>(bw);
                __m256i out_vec = _mm256_load_si256(reinterpret_cast<__m256i *>(tmp));
                _mm256_storeu_si256(reinterpret_cast<__m256i *>(out.ptr + i), out_vec);
            }"""

new = """                __m256i r = _mm256_sub_epi32(_mm256_add_epi32(a, bias_vec), b);
                // 直接 store 到 out, 省去 tmp 缓冲区 (absSub 直接 store 优化)
                _mm256_storeu_si256(reinterpret_cast<__m256i *>(out.ptr + i), r);
                uint32_t *p32 = reinterpret_cast<uint32_t *>(out.ptr + i);
                uint32_t bw = borrow;
                for (int k = 0; k < 8; k++)
                {
                    uint32_t lo = p32[k] & 0xFFFF;
                    uint32_t hi = p32[k] >> 16;
                    lo -= bw;
                    uint32_t blo = lo < 10000;
                    lo -= (1u - blo) * 10000u;
                    hi -= blo;
                    uint32_t bhi = hi < 10000;
                    hi -= (1u - bhi) * 10000u;
                    bw = bhi;
                    p32[k] = lo | (hi << 16);
                }
                borrow = static_cast<Limb>(bw);
            }"""

count = content.count(old)
print(f'found: {count}')
if count == 1:
    content = content.replace(old, new)
    with open(PATH, 'w', encoding='utf-8', newline='') as f:
        f.write(content)
    print('DONE')
else:
    # 尝试找部分匹配
    if 'alignas(32) uint32_t tmp[8];' in content:
        print('partial: alignas line found')
    if '串行 borrow 传播' in content:
        print('partial: 串行 borrow found')
    print('NOT FOUND')
