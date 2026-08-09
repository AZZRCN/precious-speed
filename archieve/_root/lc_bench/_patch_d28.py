# -*- coding: utf-8 -*-
"""
D28 = D25 + absDivRem 分派阈值参数化

背景 (2026-08-04 夜, 函数级 profile 发现):
    r_nearly_zero_01 是 T=3993 组小除法, 典型 limbA=250 / limbB=186 / qn=65.
    分派 `if (len2 <= 64 || (len1-len2) <= 64)` 中 len1-len2 = 64 恰好命中边界,
    3021 组一头栽进 schoolbook absDivBasicCore, 工作量 11.06M limb-op.
    实测 absSub 88.5M Ir(26.9%) + absMul1 86.1M(26.2%) + basicMul 36.6M(11.1%)
        + absDivBasicCore 36.2M(11.0%) = 75% 的总指令数,  FFT 只占 11%.
    而 len1 < len2*2 (250 < 372) 成立 => 本该走 absDivNewtonCore1.

本 patch 只把两个 64 变成宏, 不改任何语义 (默认值完全等价于 D25),
用 -DDIV_BASIC_QN=<n> 扫描最优切换点。

    -DDIV_BASIC_QN=n   商长阈值 (默认 64)
    -DDIV_BASIC_L2=n   除数长阈值 (默认 64)
"""
import io
import os
import shutil
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'best', 'div_D25.cpp')
DST = os.path.join(ROOT, 'best', 'div_D28.cpp')

OLD = '''                Span quot_span(quotient.data.data(), len1 - len2);
                if (len2 <= 64 || (len1 - len2) <= 64)
                {
                    absDivBasicCore(dividend_span, divisor_span, quot_span);
                }'''

NEW = '''                Span quot_span(quotient.data.data(), len1 - len2);
                // D28: 分派阈值参数化。原为硬编码 64/64。
                //   r_nearly_zero_01 的 3021 组恰好 len1-len2==64, 卡边界进了
                //   schoolbook(O(qn*len2)), 而 len1 < len2*2 成立本该走 Newton1。
                if (len2 <= DIV_BASIC_L2 || (len1 - len2) <= DIV_BASIC_QN)
                {
                    absDivBasicCore(dividend_span, divisor_span, quot_span);
                }'''

DEFS = '''
// ==================== D28: 除法分派阈值 (可调) ====================
#ifndef DIV_BASIC_QN
#define DIV_BASIC_QN 64
#endif
#ifndef DIV_BASIC_L2
#define DIV_BASIC_L2 64
#endif
'''

ANCHOR = '#include <chrono>'


def main():
    shutil.copyfile(SRC, DST)
    s = io.open(DST, encoding='utf-8', errors='replace').read()
    n0 = len(s)

    i = s.find(ANCHOR)
    if i < 0:
        print('[A] FATAL: anchor not found')
        sys.exit(1)
    s = s[:i] + DEFS + '\n' + s[i:]
    print('[A] threshold macros inserted @%d' % i)

    if s.count(OLD) != 1:
        print('[B] FATAL: dispatch anchor count=%d' % s.count(OLD))
        sys.exit(1)
    s = s.replace(OLD, NEW, 1)
    print('[B] absDivRem dispatch parameterized')

    io.open(DST, 'w', encoding='utf-8').write(s)
    print('written %s : %d -> %d bytes' % (DST, n0, len(s)))


if __name__ == '__main__':
    main()
