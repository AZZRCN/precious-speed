#!/usr/bin/env python3
"""absSub_avx2 直接 store 优化 (二进制模式处理 CRLF)"""
PATH = r'd:\precious_speed\add.cpp'

with open(PATH, 'rb') as f:
    content = f.read()

old = (
    b'                __m256i r = _mm256_sub_epi32(_mm256_add_epi32(a, bias_vec), b);\r\n'
    b'                alignas(32) uint32_t tmp[8];\r\n'
    b'                _mm256_store_si256(reinterpret_cast<__m256i *>(tmp), r);\r\n'
    b'                // \xe4\xb8\xb2\xe8\xa1\x8c borrow \xe4\xbc\xa0\xe6\x92\xad: 8 \xe6\xad\xa5, \xe6\xaf\x8f\xe6\xad\xa5\xe5\xa4\x84\xe7\x90\x86 2 \xe4\xb8\xaa limb (lo/hi), \xe6\x97\xa0\xe5\x88\x86\xe6\x94\xaf\xe6\x8e\xa9\xe7\xa0\x81\r\n'
    b'                uint32_t bw = borrow;\r\n'
    b'                for (int k = 0; k < 8; k++)\r\n'
    b'                {\r\n'
    b'                    uint32_t lo = tmp[k] & 0xFFFF;\r\n'
    b'                    uint32_t hi = tmp[k] >> 16;\r\n'
    b'                    lo -= bw;\r\n'
    b'                    uint32_t blo = lo < 10000;\r\n'
    b'                    lo -= (1u - blo) * 10000u;\r\n'
    b'                    hi -= blo;\r\n'
    b'                    uint32_t bhi = hi < 10000;\r\n'
    b'                    hi -= (1u - bhi) * 10000u;\r\n'
    b'                    bw = bhi;\r\n'
    b'                    tmp[k] = lo | (hi << 16);\r\n'
    b'                }\r\n'
    b'                borrow = static_cast<Limb>(bw);\r\n'
    b'                __m256i out_vec = _mm256_load_si256(reinterpret_cast<__m256i *>(tmp));\r\n'
    b'                _mm256_storeu_si256(reinterpret_cast<__m256i *>(out.ptr + i), out_vec);\r\n'
    b'            }'
)

new = (
    b'                __m256i r = _mm256_sub_epi32(_mm256_add_epi32(a, bias_vec), b);\r\n'
    b'                // \xe7\x9b\xb4\xe6\x8e\xa5 store \xe5\x88\xb0 out, \xe7\x9c\x81\xe5\x8e\xbb tmp \xe7\xbc\x93\xe5\x86\xb2\xe5\x8c\xba (absSub \xe7\x9b\xb4\xe6\x8e\xa5 store \xe4\xbc\x98\xe5\x8c\x96)\r\n'
    b'                _mm256_storeu_si256(reinterpret_cast<__m256i *>(out.ptr + i), r);\r\n'
    b'                uint32_t *p32 = reinterpret_cast<uint32_t *>(out.ptr + i);\r\n'
    b'                uint32_t bw = borrow;\r\n'
    b'                for (int k = 0; k < 8; k++)\r\n'
    b'                {\r\n'
    b'                    uint32_t lo = p32[k] & 0xFFFF;\r\n'
    b'                    uint32_t hi = p32[k] >> 16;\r\n'
    b'                    lo -= bw;\r\n'
    b'                    uint32_t blo = lo < 10000;\r\n'
    b'                    lo -= (1u - blo) * 10000u;\r\n'
    b'                    hi -= blo;\r\n'
    b'                    uint32_t bhi = hi < 10000;\r\n'
    b'                    hi -= (1u - bhi) * 10000u;\r\n'
    b'                    bw = bhi;\r\n'
    b'                    p32[k] = lo | (hi << 16);\r\n'
    b'                }\r\n'
    b'                borrow = static_cast<Limb>(bw);\r\n'
    b'            }'
)

count = content.count(old)
print(f'found: {count}')
if count == 1:
    content = content.replace(old, new)
    with open(PATH, 'wb') as f:
        f.write(content)
    print('DONE')
else:
    print('NOT FOUND')
