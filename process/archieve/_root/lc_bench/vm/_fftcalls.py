# -*- coding: utf-8 -*-
"""数清某个 case 里 DFT/IDFT 及各乘法入口的调用次数, 找冗余变换。
用法: python _fftcalls.py [bin] [case]
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run

B = sys.argv[1] if len(sys.argv) > 1 else 'd38'
C = sys.argv[2] if len(sys.argv) > 2 else 'length_ratio_integer_02'
OUT = r'D:\precious_speed\lc_bench\vm\_fftcalls_%s_%s.txt' % (B, C)

put(r'D:\precious_speed\lc_bench\vm\_callers.py', '/home/azzr/divbench/_callers.py')

cg = '/tmp/cgT_%s_%s.out' % (B, C)
# 若缓存不在则重新生成
cmd = ('cd /home/azzr/divbench && ([ -f %s ] || python3 _top_remote.py %s %s 2 >/dev/null) && '
       'python3 _callers.py %s difDispatch iditDispatch difSmall iditSmall '
       'absSqr absMul fftMul fftMulPre fftMulModBm1 absInvNewton absDivMu 2>&1 | head -120'
       % (cg, B, C, cg))
rc, o, e = run(cmd, timeout=5400, verbose=False)
txt = 'rc=%s\n%s\n%s' % (rc, o, e[-800:])
open(OUT, 'w', encoding='utf-8').write(txt)
print(txt)
